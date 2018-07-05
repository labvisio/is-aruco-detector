
#include <regex>
#include <unordered_map>
#include "aruco/aruco.hpp"
#include "calibration_fetcher.hpp"
#include "is/msgs/camera.pb.h"
#include "is/wire/core.hpp"
#include "is/wire/rpc.hpp"
#include "utils.hpp"

auto process_images(is::Message const& message, is::Channel channel, is::Aruco const& aruco,
                    is::CalibrationFetcher& fetcher,
                    std::shared_ptr<opentracing::Tracer> const& tracer) -> bool {
  std::string topic = message.topic();
  std::smatch matches;
  auto ok = std::regex_match(topic, matches, std::regex("CameraGateway.(\\d+).Frame"));
  if (ok) {
    auto maybe_ctx = message.extract_tracing(tracer);
    auto detection_span =
        maybe_ctx ? tracer->StartSpan("Detection", {opentracing::ChildOf(maybe_ctx->get())})
                  : tracer->StartSpan("Detection");

    auto maybe_image = message.unpack<is::vision::Image>();
    if (maybe_image) {
      auto id = std::stoi(matches[1]);

      is::vision::ImageAnnotations annotations;
      try {
        annotations = aruco.detect(*maybe_image);
        channel.publish(fmt::format("ArUco.{}.Detection", id), is::Message{annotations});
      } catch (...) { is::warn("Detector throwed exception"); }
      detection_span->SetTag("Detections", annotations.annotations().size());
      detection_span->Finish();

      auto localization_span =
          tracer->StartSpan("Localization", {opentracing::FollowsFrom(&detection_span->context())});

      auto maybe_calibration = fetcher.find(id);
      if (maybe_calibration) {
        auto poses = aruco.localize(annotations, *maybe_calibration);
        for (auto&& pose : poses) {
          auto span = tracer->StartSpan(std::to_string(pose.from()),
                                        {opentracing::ChildOf(&localization_span->context())});
          auto message = is::Message{pose};
          message.inject_tracing(tracer, span->context());
          channel.publish(fmt::format("ArUco.{}.Pose", id), message);
        }
      }
    }
  }
  return ok;
}

int main(int argc, char** argv) {
  auto const service = std::string{"ArUco.Localization"};
  auto options = load_cli_options(argc, argv);

  auto tracer = make_tracer(service, options.zipkin_uri());

  auto channel = is::Channel{options.broker_uri()};
  channel.set_tracer(tracer);

  auto images = is::Subscription{channel, "ArUco.Localization"};
  images.subscribe("CameraGateway.*.Frame");
  auto server = is::ServiceProvider{channel};

  auto aruco_config = options.config();
  auto aruco = is::Aruco{aruco_config.dictionary(),
                         std::unordered_map<int64_t, float>{aruco_config.lengths().begin(),
                                                            aruco_config.lengths().end()}};

  auto rpcs = is::Subscription{channel};
  auto fetcher = is::CalibrationFetcher{};

  for (;;) {
    auto message = channel.consume();
    auto ok = fetcher.eval(message, channel, rpcs);
    if (!ok) ok = process_images(message, channel, aruco, fetcher, tracer);
    if (!ok) ok = server.serve(message);
  }
}
