#include <is/msgs/image.pb.h>
#include <is/msgs/marker.pb.h>
#include <algorithm>
#include <is/is.hpp>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <unordered_map>

using is::vision::ImageAnnotations;
using is::vision::ImageAnnotation;
using is::vision::Image;

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

int main(int argc, char** argv) {
  std::string uri;
  float scale;

  is::po::options_description opts("Options");
  auto&& opt_add = opts.add_options();
  opt_add("uri,u", is::po::value<std::string>(&uri)->required(), "broker uri");
  opt_add("scale,s", is::po::value<float>(&scale)->default_value(1.0f), "scale");
  is::parse_program_options(argc, argv, opts);

  auto channel = is::rmq::Channel::CreateFromUri(uri);
  auto tag = is::declare_queue(channel);
  std::vector<std::string> topics = {"CameraGateway.*.Frame", "ArUco.*.Detection"};
  is::subscribe(channel, tag, topics);

  std::unordered_map<std::string, cv::Mat> images;
  std::unordered_map<std::string, ImageAnnotations> annotations;

  for (;;) {
    auto envelope = channel->BasicConsumeMessage();
    auto topic = envelope->RoutingKey();
    std::string id, subtopic;
    std::tie(id, subtopic) = parse_topic(topic);

    if (subtopic == "Frame") {
      auto maybe_image = is::unpack<Image>(envelope);
      std::vector<char> coded(maybe_image->data().begin(), maybe_image->data().end());
      auto image = cv::imdecode(coded, CV_LOAD_IMAGE_COLOR);
      draw_annotations(image, annotations[id]);
      images[id] = image;
    } else if (subtopic == "Detection") {
      auto maybe_annotations = is::unpack<ImageAnnotations>(envelope);
      annotations[id] = *maybe_annotations;
    }

    std::vector<cv::Mat> resized;
    for (auto keyvalue : images) {
      cv::Mat mat;
      auto image = keyvalue.second;
      cv::resize(image, mat, cv::Size(image.cols * scale, image.rows * scale));
      resized.push_back(mat);
    }

    if (!resized.empty()) {
      cv::Mat image;
      cv::hconcat(resized, image);
      cv::imshow("Detections", image);
      cv::waitKey(1);
    }
  }
}
