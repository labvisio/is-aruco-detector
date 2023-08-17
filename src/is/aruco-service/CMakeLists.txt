cmake_minimum_required(VERSION 3.10.0)

find_package(is-wire REQUIRED)
find_package(is-msgs REQUIRED)
find_package(zipkin-cpp-opentracing REQUIRED)

find_package(Protobuf REQUIRED)
get_target_property(Protobuf_IMPORT_DIRS is-msgs::is-msgs INTERFACE_INCLUDE_DIRECTORIES)

get_filename_component(options_pb "conf/options.proto" ABSOLUTE)
get_filename_component(options_pb_path "${options_pb}" PATH)

set(options_src "${CMAKE_CURRENT_BINARY_DIR}/options.pb.cc")
set(options_hdr "${CMAKE_CURRENT_BINARY_DIR}/options.pb.h")

add_custom_command(
  OUTPUT
    "${options_src}"
    "${options_hdr}"
  COMMAND ${Protobuf_PROTOC_EXECUTABLE}
  ARGS --cpp_out ${CMAKE_CURRENT_BINARY_DIR}
  "-I=${options_pb_path}" "-I=${Protobuf_IMPORT_DIRS}" ${options_pb}
  DEPENDS
  ${proto})

add_executable(service.bin
 service.cpp
 ${options_src}
 ${options_hdr}
 calibration-fetcher.cpp
 calibration-fetcher.hpp
)

target_link_libraries(
 service.bin
PUBLIC
 is-aruco::is-aruco
 is-wire::is-wire
 is-msgs::is-msgs
 zipkin-cpp-opentracing::zipkin-cpp-opentracing
)

target_include_directories(
 service.bin
PUBLIC
 $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}> # for headers when building
 $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}> # for generated files in build mode
)

set_property(TARGET service.bin PROPERTY CXX_STANDARD 11)
