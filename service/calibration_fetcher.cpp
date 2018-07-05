#include "calibration_fetcher.hpp"
#include "google/protobuf/repeated_field.h"

namespace is {

auto CalibrationFetcher::find(int64_t camera_id) -> boost::optional<vision::CameraCalibration> {
  auto it = calibrations.find(camera_id);
  if (it == calibrations.end()) {
    camera_ids.insert(camera_id);
    return boost::none;
  }
  return it->second;
}

auto CalibrationFetcher::eval(Message const& message, Channel const& channel,
                              Subscription const& subscription) -> bool {
  auto is_reply = message.has_correlation_id() && message.correlation_id() == correlation_id;

  if (is_reply) {
    if (message.status().code() == is::wire::StatusCode::OK) {
      auto reply = message.unpack<vision::GetCalibrationReply>();
      if (reply) {
        for (auto&& calibration : reply->calibrations()) {
          calibrations[calibration.id()] = calibration;
          camera_ids.erase(calibration.id());
          is::info("[CalibrationFetcher] Updating calibration\n {}", calibration);
        }
      }
    } else {
      is::info("[CalibrationFetcher] RPC Failed {}", message.status());
    }
  }

  auto now = std::chrono::system_clock::now();
  if (now >= timeout) {
    timeout = now + std::chrono::seconds(fetch_interval);
    if (camera_ids.size()) {
      auto request = vision::GetCalibrationRequest{};
      request.mutable_ids()->Reserve(camera_ids.size());
      std::copy(camera_ids.begin(), camera_ids.end(),
                google::protobuf::RepeatedFieldBackInserter(request.mutable_ids()));
      is::info("[CalibrationFetcher] Requesting Calibrations: {}", request);

      auto message = is::Message{request};
      message.set_reply_to(subscription).set_deadline(timeout);
      correlation_id = message.correlation_id();
      channel.publish("FrameConversion.GetCalibration", message);
    }
  }

  return is_reply;
}

}  // namespace is