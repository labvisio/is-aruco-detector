ArUco Marker Detection Service
==================

ArUco marker localization and detection service. 

Streams
-----------

| Name | Input (Topic/Message) | Output (Topic/Message) | Description | 
| ---- | --------------------- | ---------------------- | ----------- |
| ArUco.Localization | **CameraGateway.(ID).Frame** [Image] | **ArUco.(ID).FrameTransformations** [FrameTransformations] | Localize markers on the world in relation to the respective camera frame an publishes a FrameTransformations messsage with each marker localized. **The marker frame id is calculated as ArUcoMarkerID + 100**. |
| ArUco.Localization | **CameraGateway.(ID).Frame** [Image] | **ArUco.(ID).Detection** [ObjectAnnotations] | Detect markers on images published by cameras an publishes a ObjectAnnotations message containing all the detected markers. |


[Image]: https://github.com/labviros/is-msgs/blob/modern-cmake/docs/README.md#is.vision.Image
[FrameTransformations]: https://github.com/labviros/is-msgs/blob/modern-cmake/docs/README.md#is.vision.FrameTransformations
[ObjectAnnotations]: https://github.com/labviros/is-msgs/tree/v1.1.8/docs#is.vision.ObjectAnnotations


About
------

> "Pose estimation is of great importance in many computer vision applications: robot navigation, augmented reality, and many more. This process is based on finding correspondences between points in the real environment and their 2d image projection. This is usually a difficult step, and thus it is common the use of synthetic or fiducial markers to make it easier.
> One of the most popular approach is the use of binary square fiducial markers. The main benefit of these markers is that a single marker provides enough correspondences (its four corners) to obtain the camera pose. Also, the inner binary codification makes them specially robust, allowing the possibility of applying error detection and correction techniques." [More...](https://docs.opencv.org/3.1.0/d5/dae/tutorial_aruco_detection.html)