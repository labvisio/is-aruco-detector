#pragma once

#include <chrono>
#include <unordered_set>
#include "boost/optional.hpp"
#include "is/msgs/camera.pb.h"
#include "is/wire/core.hpp"

namespace is {

class CalibrationFetcher {
  static constexpr auto fetch_interval = std::chrono::seconds(2);

  std::unordered_map<int64_t, vision::CameraCalibration> calibrations;
  uint64_t correlation_id;
  std::chrono::system_clock::time_point timeout = std::chrono::system_clock::now() + fetch_interval;
  std::unordered_set<int64_t> camera_ids;

 public:
  auto find(int64_t camera_id) -> boost::optional<vision::CameraCalibration>;
  auto eval(Message const& message, Channel const& channel, Subscription const& subscription)
      -> bool;
};

}  // namespace is