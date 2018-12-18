ArUco Marker Detection Service
==================

ArUco marker localization and detection service. 

Dependencies:
-----
This service has no dependency on other services.

Events:
--------
<img width=1000/> ⇒ Triggered By | <img width=1000/> Triggers ⇒ | <img width=200/> Description  
:------------ | :-------- | :----------
:incoming_envelope: **topic:** `CameraGateway.{camera_id}.Frame` <br> :gem: **schema:** [Image] | :incoming_envelope: **topic:** `ArUco.{camera_id}.FrameTransformations` <br> :gem: **schema:** [FrameTransformations] | [Stream] Localize markers on the world in relation to the respective camera frame an publishes a FrameTransformations messsage with each marker localized. **The marker frame id is calculated as ArUcoMarkerID + 100.**
:incoming_envelope: **topic:** `CameraGateway.{camera_id}.Frame` <br> :gem: **schema:** [Image] | :incoming_envelope: **topic:** `ArUco.{camera_id}.Detection` <br> :gem: **schema:** [ObjectAnnotations] | [Stream] Detect markers on images published by cameras an publishes a ObjectAnnotations message containing all the detected markers. 

[Image]: https://github.com/labviros/is-msgs/blob/modern-cmake/docs/README.md#is.vision.Image
[FrameTransformations]: https://github.com/labviros/is-msgs/blob/modern-cmake/docs/README.md#is.vision.FrameTransformations
[ObjectAnnotations]: https://github.com/labviros/is-msgs/tree/v1.1.8/docs#is.vision.ObjectAnnotations


Configuration:
----------------
The behavior of the service can be customized by passing a JSON configuration file as the first argument, e.g: `./service config.json`. The schema and documentation for this file can be found in [`src/is/aruco-service/conf/options.proto`](src/is/aruco-service/conf/options.proto). An example configuration file can be found in [`etc/conf/options.json`](etc/conf/options.json).


About
------

> "Pose estimation is of great importance in many computer vision applications: robot navigation, augmented reality, and many more. This process is based on finding correspondences between points in the real environment and their 2d image projection. This is usually a difficult step, and thus it is common the use of synthetic or fiducial markers to make it easier.
> One of the most popular approach is the use of binary square fiducial markers. The main benefit of these markers is that a single marker provides enough correspondences (its four corners) to obtain the camera pose. Also, the inner binary codification makes them specially robust, allowing the possibility of applying error detection and correction techniques." [More...](https://docs.opencv.org/3.1.0/d5/dae/tutorial_aruco_detection.html)