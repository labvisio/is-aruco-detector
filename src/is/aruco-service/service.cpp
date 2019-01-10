
#include <is/msgs/camera.pb.h>
#include <zipkin/opentracing.h>
#include <is/msgs/utils.hpp>
#include <is/wire/core.hpp>
#include <is/wire/rpc.hpp>
#include <regex>
#include <unordered_map>
#include "aruco/aruco.hpp"
#include "calibration-fetcher.hpp"
#include "conf/options.pb.h"

auto process_images(is::Message const& message, is::Channel channel, is::Aruco const& aruco,
                    is::CalibrationFetcher& fetcher,
                    std::shared_ptr<opentracing::Tracer> const& tracer) -> bool {
  std::string topic = message.topic();
  std::smatch matches;
  auto ok = std::regex_match(topic, matches, std::regex("CameraGateway.(\\d+).Frame"));
  if (ok) {
    auto maybe_ctx = message.extract_tracing(tracer);
    auto localization_span =
        maybe_ctx ? tracer->StartSpan("Localization", {opentracing::ChildOf(maybe_ctx->get())})
                  : tracer->StartSpan("Localization");

    auto maybe_image = message.unpack<is::vision::Image>();
    if (maybe_image) {
      auto id = std::stoi(matches[1]);

      is::vision::ObjectAnnotations annotations = aruco.detect(*maybe_image);
      auto maybe_calibration = fetcher.find(id);
      if (maybe_calibration) {
        auto transformations = aruco.localize(annotations, *maybe_calibration);
        auto message = is::Message{transformations};
        message.inject_tracing(tracer, localization_span->context());
        channel.publish(fmt::format("ArUco.{}.FrameTransformations", id), message);
      }

      auto message = is::Message{annotations};
      message.inject_tracing(tracer, localization_span->context());
      channel.publish(fmt::format("ArUco.{}.Detection", id), message);
    }
  }
  return ok;
}

auto load_configuration(int argc, char** argv) -> is::ArucoServiceOptions {
  auto filename = (argc == 2) ? argv[1] : "options.json";
  auto options = is::ArucoServiceOptions{};
  is::load(filename, &options);
  is::validate_message(options);
  return options;
}

auto create_tracer(std::string const& name, std::string const& uri)
    -> std::shared_ptr<opentracing::Tracer> {
  std::smatch match;
  auto ok = std::regex_match(uri, match, std::regex("http:\\/\\/([a-zA-Z0-9\\.]+)(:(\\d+))?"));
  if (!ok) is::critical("Invalid zipkin uri \"{}\", expected http://<hostname>:<port>", uri);
  auto tracer_options = zipkin::ZipkinOtTracerOptions{};
  tracer_options.service_name = name;
  tracer_options.collector_host = match[1];
  tracer_options.collector_port = match[3].length() ? std::stoi(match[3]) : 9411;
  return zipkin::makeZipkinOtTracer(tracer_options);
}

int main(int argc, char** argv) {
  auto const service = std::string{"ArUco.Localization"};

  auto options = load_configuration(argc, argv);
  auto tracer = create_tracer(service, options.zipkin_uri());

  auto channel = is::Channel{options.broker_uri()};
  channel.set_tracer(tracer);

  auto images = is::Subscription{channel, "ArUco.Localization"};
  images.subscribe("CameraGateway.*.Frame");

  auto aruco_config = options.config();
  auto aruco = is::Aruco{aruco_config.dictionary(),
                         std::unordered_map<int64_t, float>{aruco_config.lengths().begin(),
                                                            aruco_config.lengths().end()}};
  aruco.set_cpu_parallelism(aruco_config.cpu_parallelism());

  auto rpcs = is::Subscription{channel};
  auto fetcher = is::CalibrationFetcher{};

  auto eval_message = [&](is::Message message) {
    auto ok = fetcher.eval(message, channel, rpcs);
    if (!ok) ok = process_images(message, channel, aruco, fetcher, tracer);
  };

  for (;;) {
    auto messages = channel.consume_ready();
    if (messages.empty()) {
      eval_message(channel.consume());
    } else {
      auto middle = std::stable_partition(messages.begin(), messages.end(), [&](is::Message& msg) {
        return msg.subscription_id() == rpcs.name();
      });
      // rpcs' messages
      std::for_each(messages.begin(), middle, eval_message);
      // images' messages
      if (middle != messages.end()) {
        auto dropped = std::distance(middle, messages.end()) - 1;
        if (dropped > 0) is::warn("event=ArUco.Consume dropped={}", dropped);
        eval_message(messages.back());
      }
    }
  }
}
