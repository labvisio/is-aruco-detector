#pragma once

#include <is/msgs/camera.pb.h>
#include <boost/optional.hpp>
#include <chrono>
#include <is/msgs/utils.hpp>
#include <is/wire/core.hpp>
#include <unordered_set>

namespace is {

class CalibrationFetcher {
  std::unordered_map<int64_t, vision::CameraCalibration> calibrations;
  uint64_t correlation_id;
  std::chrono::system_clock::time_point timeout;
  std::unordered_set<int64_t> camera_ids;

 public:
  CalibrationFetcher();
  auto find(int64_t camera_id) -> boost::optional<vision::CameraCalibration>;
  auto eval(Message const& message, Channel const& channel, Subscription const& subscription)
      -> bool;
};

}  // namespace is