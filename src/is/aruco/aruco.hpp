#pragma once

#include <is/msgs/camera.pb.h>
#include <is/msgs/common.pb.h>
#include <is/msgs/image.pb.h>
#include <algorithm>
#include <is/msgs/cv.hpp>
#include <opencv2/aruco.hpp>
#include <opencv2/calib3d.hpp>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <string>
#include <unordered_map>
#include <vector>

namespace is {

class Aruco {
  cv::Ptr<cv::aruco::DetectorParameters> parameters;
  cv::Ptr<cv::aruco::Dictionary> dictionary;
  std::unordered_map<int64_t, float> lengths;

 public:
  Aruco(int dict, std::unordered_map<int64_t, float> const& lengths);

  auto detect(vision::Image const& image) const -> vision::ObjectAnnotations;

  auto localize(vision::ObjectAnnotations const& annotations,
                vision::CameraCalibration& calibration) const -> vision::FrameTransformations;

  void set_cpu_parallelism(int threads) const;
  auto cpu_parallelism() const -> int;
};

}  // namespace is