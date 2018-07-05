#include "aruco.hpp"
#include <iostream>

namespace is {

Aruco::Aruco(int dict, std::unordered_map<int64_t, float> const& len) {
  parameters = cv::aruco::DetectorParameters::create();
  parameters->cornerRefinementMethod = cv::aruco::CORNER_REFINE_CONTOUR;
  dictionary = cv::aruco::getPredefinedDictionary(dict);
  lengths = len;
}

auto Aruco::detect(vision::Image const& img) const -> vision::ImageAnnotations {
  std::vector<char> coded(img.data().begin(), img.data().end());
  auto image = cv::imdecode(coded, CV_LOAD_IMAGE_GRAYSCALE);

  std::vector<int> ids;
  std::vector<std::vector<cv::Point2f>> corners;
  cv::aruco::detectMarkers(image, dictionary, corners, ids, parameters);

  auto annotations = vision::ImageAnnotations{};
  for (int i = 0; i < ids.size(); ++i) {
    auto annotation = annotations.add_annotations();
    annotation->set_label(std::to_string(ids[i]));
    annotation->set_id(ids[i]);
    for (auto&& point : corners[i]) {
      auto vertex = annotation->mutable_region()->add_vertices();
      vertex->set_x(point.x);
      vertex->set_y(point.y);
    }
  }

  auto resolution = annotations.mutable_resolution();
  resolution->set_height(image.rows);
  resolution->set_width(image.cols);
  return annotations;
}

auto Aruco::localize(vision::ImageAnnotations const& anno,
                     vision::CameraCalibration& calibration) const
    -> std::vector<vision::FrameTransformation> {
  auto sx = anno.resolution().width() / static_cast<double>(calibration.resolution().width());
  auto sy = anno.resolution().height() / static_cast<double>(calibration.resolution().height());
  cv::Mat scale = (cv::Mat_<double>(3, 3) << sx, 0, 0, 0, sy, 0, 0, 0, 1);
  auto intrinsic = scale * to_mat_view(calibration.mutable_intrinsic());
  auto distortion = to_mat_view(calibration.mutable_distortion());

  std::vector<vision::FrameTransformation> poses;
  poses.reserve(anno.annotations().size());

  for (auto const& annotation : anno.annotations()) {
    auto vertices = annotation.region().vertices();
    if (vertices.size() != 4) continue;

    auto length_kv = lengths.find(annotation.id());
    if (length_kv == lengths.end()) continue;
    auto marker_id = length_kv->first;
    auto marker_size = length_kv->second;

    std::vector<std::vector<cv::Point2f>> corners{{
        {static_cast<float>(vertices[0].x()), static_cast<float>(vertices[0].y())},
        {static_cast<float>(vertices[1].x()), static_cast<float>(vertices[1].y())},
        {static_cast<float>(vertices[2].x()), static_cast<float>(vertices[2].y())},
        {static_cast<float>(vertices[3].x()), static_cast<float>(vertices[3].y())},
    }};
    std::vector<cv::Vec3d> rvecs, tvecs;
    cv::aruco::estimatePoseSingleMarkers(corners, marker_size, intrinsic, distortion, rvecs, tvecs);

    cv::Mat tf;
    cv::Rodrigues(rvecs[0], tf);
    cv::hconcat(tf, tvecs[0], tf);
    cv::Mat lastRow = (cv::Mat_<double>(1, 4) << 0, 0, 0, 1);
    cv::vconcat(tf, lastRow, tf);

    vision::FrameTransformation transformation;
    transformation.set_from(100 + marker_id);
    transformation.set_to(calibration.id());
    *transformation.mutable_tf() = to_tensor(tf);

    poses.push_back(transformation);
  }

  return poses;
}

}  // namespace is