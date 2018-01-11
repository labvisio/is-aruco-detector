#include <is/msgs/camera.pb.h>
#include <is/msgs/common.pb.h>
#include <is/msgs/image.pb.h>
#include <is/msgs/marker.pb.h>
#include <boost/filesystem.hpp>
#include <boost/range/iterator_range.hpp>
#include <fstream>
#include <is/is.hpp>
#include <opencv2/aruco.hpp>
#include <opencv2/calib3d.hpp>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <vector>

using is::common::Pose;
using is::common::Tensor;
using is::vision::CameraCalibration;
using is::vision::Image;
using is::vision::ImageAnnotations;
using is::vision::MarkerSettings;

namespace fs = boost::filesystem;

is::pb::Status parse_json_file(std::string const& file, is::pb::Message* message) {
  std::ifstream in(file);
  std::string s((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  return is::pb::JsonStringToMessage(s, message);
};

std::unordered_map<std::string, CameraCalibration> load_calibrations(std::string const& folder) {
  std::unordered_map<std::string, CameraCalibration> calibrations;

  if (fs::is_directory(folder)) {
    for (auto& entry : boost::make_iterator_range(fs::directory_iterator(folder), {})) {
      CameraCalibration calibration;
      if (entry.path().extension() == ".json" &&
          parse_json_file(entry.path().string(), &calibration).ok()) {
        is::info("Read calibration from {}: {}", entry.path().string(), calibration);
        calibrations.emplace(calibration.id(), calibration);
      }
    }
  } else {
    is::warn("Unable to read camera calibrations, {} isn't a directory", folder);
  }
  return calibrations;
}

std::string parse_entity_id(std::string const& topic) {
  auto first_dot = topic.find_first_of('.');
  if (first_dot == std::string::npos) return "";
  auto second_dot = topic.find_first_of('.', first_dot + 1);
  if (second_dot == std::string::npos) return "";
  return topic.substr(first_dot + 1, second_dot - first_dot - 1);
}

cv::Mat cv_matrix_view(Tensor* tensor) {
  auto shape = tensor->shape();
  int rows = 0;
  int cols = 0;

  if (shape.dims_size() == 1) {
    rows = 1;
    cols = shape.dims(0).size();
  } else if (shape.dims_size() == 2) {
    rows = shape.dims(0).size();
    cols = shape.dims(1).size();
  }

  if (tensor->type() == is::common::DataType::FLOAT_TYPE)
    return cv::Mat(rows, cols, CV_32F, tensor->mutable_floats()->mutable_data());
  if (tensor->type() == is::common::DataType::DOUBLE_TYPE)
    return cv::Mat(rows, cols, CV_64F, tensor->mutable_doubles()->mutable_data());
  if (tensor->type() == is::common::DataType::INT32_TYPE)
    return cv::Mat(rows, cols, CV_32S, tensor->mutable_ints32()->mutable_data());
  return cv::Mat();
}

int main(int argc, char** argv) {
  std::string uri, service, calib_dir, zipkin_host;
  float marker_length;
  int dict_id, zipkin_port, parallelism;

  is::po::options_description opts("Options");
  auto&& opt_add = opts.add_options();
  opt_add("uri,u", is::po::value<std::string>(&uri)->required(), "broker uri");
  opt_add("name", is::po::value<std::string>(&service)->default_value("ArUco.Detector"),
          "service name");
  opt_add("calib,c", is::po::value<std::string>(&calib_dir)->default_value("./calibs"),
          "directory containing calibration files");
  opt_add("dictionary,d", is::po::value<int>(&dict_id)->default_value(0), "dictionary id");
  opt_add("marker-length,l", is::po::value<float>(&marker_length), "marker length");
  opt_add("parallelism", is::po::value<int>(&parallelism)->default_value(0), "number of threads");
  opt_add("zipkin-host,z",
          is::po::value<std::string>(&zipkin_host)->default_value("zipkin.default"),
          "zipkin hostname");
  opt_add("zipkin-port,p", is::po::value<int>(&zipkin_port)->default_value(9411), "zipkin port");
  is::parse_program_options(argc, argv, opts);

  cv::setNumThreads(parallelism);
  is::Tracer tracer(service, zipkin_host, zipkin_port);

  std::unordered_map<std::string, CameraCalibration> calibrations;
  if (marker_length > 0) calibrations = load_calibrations(calib_dir);
  auto detector_parameters = cv::aruco::DetectorParameters::create();
  detector_parameters->cornerRefinementMethod = cv::aruco::CORNER_REFINE_CONTOUR;
  auto dictionary =
      cv::aruco::getPredefinedDictionary(cv::aruco::PREDEFINED_DICTIONARY_NAME(dict_id));

  auto channel = is::rmq::Channel::CreateFromUri(uri);
  auto tag = is::consumer_id();
  is::declare_queue(channel, service, tag, /*exclusive*/ false, /*prefetch*/ 1);
  is::subscribe(channel, service, "CameraGateway.*.Frame");

  for (;;) {
    auto envelope = channel->BasicConsumeMessage();
    channel->BasicAck(envelope);
    auto maybe_image = is::unpack<Image>(envelope);
    if (!maybe_image) continue;

    auto start_time = is::current_time();
    auto span = tracer.extract(envelope, "detection");

    std::vector<char> coded(maybe_image->data().begin(), maybe_image->data().end());
    auto image = cv::imdecode(coded, CV_LOAD_IMAGE_GRAYSCALE);
    auto id = parse_entity_id(envelope->RoutingKey());

    auto annotate_image = [&](auto image, auto* annotations) {
      std::vector<int> ids;
      std::vector<std::vector<cv::Point2f> > corners;
      cv::aruco::detectMarkers(image, dictionary, corners, ids, detector_parameters);
      assert(ids.size() == corners.size());

      for (int i = 0; i < ids.size(); ++i) {
        auto image_annotation = annotations->add_annotations();
        image_annotation->set_label(std::to_string(ids[i]));
        for (auto&& point : corners[i]) {
          auto vertex = image_annotation->mutable_region()->add_vertices();
          vertex->set_x(point.x);
          vertex->set_y(point.y);
        }
      }
      return corners;
    };

    auto annotate_pose = [&](auto corners, auto* annotations) {
      auto keyval = calibrations.find(id);
      if (keyval == calibrations.end()) return;

      auto calibration = keyval->second;
      auto intrinsic = cv_matrix_view(calibration.mutable_intrinsic());
      auto distortion = cv_matrix_view(calibration.mutable_distortion());
      auto extrinsic = cv_matrix_view(calibration.mutable_extrinsic()->mutable_tf());

      std::vector<cv::Vec3d> rvecs, tvecs;
      cv::aruco::estimatePoseSingleMarkers(corners, marker_length, intrinsic, distortion, rvecs,
                                           tvecs);

      assert(rvecs.size() == tvecs.size() && rvecs.size() == corners.size());

      auto c0 = 3.0;  // lower limmit
      auto c1 = 5.0;  // uppper limmit
      auto k = 1.0 / (c1 - c0);
      auto compute_score = [&](auto z) { return z < c0 ? 1.0 : std::max(k * (c1 - z), 0.0); };

      for (int i = 0; i < corners.size(); ++i) {
        cv::Mat tf;
        cv::Rodrigues(rvecs[i], tf);
        cv::hconcat(tf, tvecs[i], tf);
        cv::Mat lastRow = (cv::Mat_<double>(1, 4) << 0, 0, 0, 1);
        cv::vconcat(tf, lastRow, tf);

        cv::Mat T = extrinsic * tf;

        // // http://www.staff.city.ac.uk/~sbbh653/publications/euler.pdf
        auto pitch = -std::asin(T.at<double>(2, 0));
        auto yaw = std::atan2(T.at<double>(2, 1) / cos(pitch), T.at<double>(2, 2) / cos(pitch));
        auto roll = std::atan2(T.at<double>(1, 0) / cos(pitch), T.at<double>(0, 0) / cos(pitch));

        auto annotation = annotations->mutable_annotations(i);
        auto pose = annotation->mutable_pose();
        auto position = pose->mutable_position();
        auto orientation = pose->mutable_orientation();

        position->set_x(T.at<double>(0, 3));
        position->set_y(T.at<double>(1, 3));
        position->set_z(T.at<double>(2, 3));
        orientation->set_pitch(pitch);
        orientation->set_yaw(yaw);
        orientation->set_roll(roll);
        annotation->set_score(compute_score(tvecs[i][2]));
      }
    };

    ImageAnnotations annotations;
    auto corners = annotate_image(image, &annotations);
    annotate_pose(corners, &annotations);

    auto msg = is::pack_proto(annotations);
    tracer.inject(msg, span->context());
    is::publish(channel, fmt::format("ArUco.{}.Detection", id), msg);

    is::info("Took: {}", is::current_time() - start_time);
    span->Finish();
  }

  return 0;
}