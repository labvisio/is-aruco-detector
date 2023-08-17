import os

from conan import ConanFile
from conan.tools.files import copy, rmdir, load
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout


class ArucoLocalizationServiceConan(ConanFile):
    name = "is-aruco-detector"
    version = "0.0.5"
    license = "MIT"
    url = ""
    description = ""
    settings = "os", "compiler", "build_type", "arch"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
    }
    default_options = {
        "shared": True,
        "fPIC": True,
    }
    exports_sources = "*"

    def requirements(self):
        self.requires(
            "is-wire/1.1.6@is/stable",
            transitive_headers=True,
            transitive_libs=True,
        )
        self.requires("zipkin-cpp-opentracing/0.3.1@is/stable")
        self.requires(
            "is-msgs/1.1.19@is/stable",
            transitive_headers=True,
            transitive_libs=True,
        )
        self.requires("opencv/4.5.5")
        # opencv dependencies conflicts
        self.requires(
            "protobuf/3.20.0",
            transitive_headers=True,
            transitive_libs=True,
            force=True,
        )
        self.requires("libwebp/1.3.0", override=True)
        self.requires("libjpeg-turbo/2.1.5", override=True)

    def build_requirements(self):
        self.tool_requires("protobuf/3.20.0")

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")
        self.options["zipkin-cpp-opentracing"].shared = self.options.shared
        self.options["protobuf"].shared = self.options.shared
        self.options["is-msgs"].shared = self.options.shared
        self.options["fmt"].shared = self.options.shared
        self.options["opencv"].shared = self.options.shared
        self.options["opencv"].parallel = "tbb"
        self.options["opencv"].with_jpeg = "libjpeg-turbo"
        self.options["opencv"].with_gtk = False
        self.options["opencv"].with_qt = False
        self.options["opencv"].with_quirc = False
        self.options["opencv"].with_ffmpeg = False
        self.options["opencv"].with_msmf = False
        self.options["opencv"].with_msmf_dxva = False
        self.options["opencv"].neon = False
        self.options["opencv"].dnn = True
        self.options["opencv"].aruco = True

    def layout(self):
        cmake_layout(self, src_folder="./")

    def generate(self):
        tc = CMakeToolchain(self)
        tc.generate()
        deps = CMakeDeps(self)
        deps.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(
            self,
            "LICENSE.txt",
            src=self.source_folder,
            dst=os.path.join(self.package_folder, "licenses"),
        )
        cmake = CMake(self)
        cmake.install()
