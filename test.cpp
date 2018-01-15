#include <is/msgs/image.pb.h>
#include <is/msgs/marker.pb.h>
#include <algorithm>
#include <is/is.hpp>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <unordered_map>

using is::vision::Image;
using is::vision::ImageAnnotation;
using is::vision::ImageAnnotations;

std::pair<std::string, std::string> parse_topic(std::string const& topic) {
  auto first_dot = topic.find_first_of('.');
  if (first_dot == std::string::npos) return std::make_pair("", "");
  auto second_dot = topic.find_first_of('.', first_dot + 1);
  if (second_dot == std::string::npos) return std::make_pair("", "");
  return std::make_pair(topic.substr(first_dot + 1, second_dot - first_dot - 1),
                        topic.substr(second_dot + 1));
}

void draw_annotation(cv::Mat image, ImageAnnotation const& annotation) {
  int fontFace = cv::FONT_HERSHEY_SIMPLEX;
  double fontScale = 1.0;
  auto color = cv::Scalar(0, 255, 0);
  int linetype = 8;
  int baseline = 0;
  int lineThickness = 3;

  std::vector<cv::Point> vertices;
  for (auto&& vertex : annotation.region().vertices())
    vertices.emplace_back(vertex.x(), vertex.y());

  if (vertices.size() > 2) {
    cv::polylines(image, vertices, /*closed*/ true, color, lineThickness);
  } else if (vertices.size() == 2) {
    cv::rectangle(image, vertices[0], vertices[1], color, lineThickness);
  }

  auto min = std::min_element(vertices.begin(), vertices.end(), [](cv::Point lhs, cv::Point rhs) {
    return lhs.x < rhs.x && lhs.y < rhs.y;
  });
  auto text = fmt::format("{} {:.2f}", annotation.label(), annotation.score());
  int thickness = 3;
  auto textColor = cv::Scalar(255, 255, 255);
  cv::Size text_size = cv::getTextSize(text, fontFace, fontScale, /*thickness*/ 4, &baseline);
  cv::rectangle(image, *min + cv::Point(0, baseline),
                *min + cv::Point(text_size.width, -text_size.height), color, CV_FILLED);
  cv::putText(image, text, *min, fontFace, fontScale, textColor, thickness, linetype);
}

void draw_annotations(cv::Mat image, ImageAnnotations const& annotations) {
  for (auto&& annotation : annotations.annotations()) {
    draw_annotation(image, annotation);
  }
}

void tile(const std::vector<cv::Mat>& src, cv::Mat& dst, int grid_x, int grid_y) {
  // patch size
  int width = dst.cols / grid_x;
  int height = dst.rows / grid_y;
  // iterate through grid
  int k = 0;
  for (int i = 0; i < grid_y; i++) {
    for (int j = 0; j < grid_x; j++) {
      cv::Mat s = src[k++];
      cv::resize(s, s, cv::Size(width, height));
      s.copyTo(dst(cv::Rect(j * width, i * height, width, height)));
    }
  }
}

int main(int argc, char** argv) {
  std::string uri;
  int height, width, per_line, draw_fps;

  is::po::options_description opts("Options");
  auto&& opt_add = opts.add_options();
  opt_add("uri,u", is::po::value<std::string>(&uri)->required(), "broker uri");
  opt_add("draw-fps,f", is::po::value<int>(&draw_fps)->default_value(10),
          "rate that frames will be drawn");
  opt_add("per-line,p", is::po::value<int>(&per_line)->default_value(2), "frames per line");
  opt_add("width,w", is::po::value<int>(&width)->default_value(1288), "width of displayed image");
  opt_add("height,h", is::po::value<int>(&height)->default_value(728), "height of displayed image");
  is::parse_program_options(argc, argv, opts);

  auto channel = is::rmq::Channel::CreateFromUri(uri);
  auto tag = is::declare_queue(channel);
  std::vector<std::string> topics = {"CameraGateway.*.Frame", "ArUco.*.Detection"};
  is::subscribe(channel, tag, topics);

  std::unordered_map<std::string, is::rmq::Envelope::ptr_t> frames, annotations;
  auto draw_deadline =
      is::current_time() + is::pb::TimeUtil::MillisecondsToDuration(1000 / draw_fps);

  for (;;) {
    for (;;) {
      auto envelope = is::consume_until(channel, draw_deadline);
      if (envelope == nullptr) break;
      auto topic = envelope->RoutingKey();
      std::string id, subtopic;
      std::tie(id, subtopic) = parse_topic(topic);

      if (subtopic == "Frame") {
        frames[id] = envelope;
      } else if (subtopic == "Detection") {
        annotations[id] = envelope;
      }
    }

    std::vector<cv::Mat> images;
    images.reserve(frames.size());
    for (auto keyval : frames) {
      auto it = annotations.find(keyval.first);
      if (it != annotations.end()) {
        auto maybe_image = is::unpack<Image>(keyval.second);
        std::vector<char> coded(maybe_image->data().begin(), maybe_image->data().end());
        auto image = cv::imdecode(coded, CV_LOAD_IMAGE_COLOR);

        auto maybe_annotations = is::unpack<ImageAnnotations>(it->second);
        draw_annotations(image, *maybe_annotations);

        images.push_back(image);
      }
    }

    if (images.size() >= per_line) {
      cv::Mat display = cv::Mat(height, width, CV_8UC3);
      tile(images, display, per_line, images.size() / per_line);
      cv::imshow("Detections", display);
      cv::waitKey(1);
    }

    draw_deadline += is::pb::TimeUtil::MillisecondsToDuration(1000 / draw_fps);
  }
}
