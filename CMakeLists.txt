cmake_minimum_required(VERSION 3.10.0)
include(GNUInstallDirs)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

include(${CMAKE_BINARY_DIR}/conan_paths.cmake)

add_subdirectory("./aruco")

find_package(is-wire REQUIRED)
find_package(zipkin-cpp-opentracing REQUIRED)

#######
####
#######

find_package(Protobuf REQUIRED)
get_target_property(Protobuf_IMPORT_DIRS is-msgs::is-msgs INTERFACE_INCLUDE_DIRECTORIES)
set(PROTOBUF_GENERATE_CPP_APPEND_PATH OFF)
PROTOBUF_GENERATE_CPP(options_src options_hdr options.proto)

add_executable(service.bin
  service/service.cpp
  ${options_src}
  ${options_hdr}
  service/utils.cpp
  service/utils.hpp
  service/calibration_fetcher.cpp
  service/calibration_fetcher.hpp
)

target_link_libraries(
  service.bin
 PUBLIC
  is-aruco::is-aruco
  is-wire::is-wire
  zipkin-cpp-opentracing::zipkin-cpp-opentracing
)

target_include_directories(
  service.bin
 PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}> # for headers when building
  $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}> # for generated files in build mode
)

set_property(TARGET service.bin PROPERTY CXX_STANDARD 11)
