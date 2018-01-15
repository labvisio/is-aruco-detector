ArUco Marker Detection Service
==================

Consume images from camera gateways and try to detect ArUco markers on it. If the camera calibration and the marker length is provided the service will also estimate the pose of the markers. The result is published as [ImageAnnotation](https://github.com/labviros/is-msgs/blob/master/protos/image.proto) message.


> "Pose estimation is of great importance in many computer vision applications: robot navigation, augmented reality, and many more. This process is based on finding correspondences between points in the real environment and their 2d image projection. This is usually a difficult step, and thus it is common the use of synthetic or fiducial markers to make it easier.
> One of the most popular approach is the use of binary square fiducial markers. The main benefit of these markers is that a single marker provides enough correspondences (its four corners) to obtain the camera pose. Also, the inner binary codification makes them specially robust, allowing the possibility of applying error detection and correction techniques." [More...](https://docs.opencv.org/3.1.0/d5/dae/tutorial_aruco_detection.html)