from conans import ConanFile, CMake, tools


class ArucoLocalizationServiceConan(ConanFile):
    name = "is-aruco-localization"
    version = "0.0.2"
    license = "MIT"
    url = ""
    description = ""
    settings = "os", "compiler", "build_type", "arch"
    options = {"shared": [True, False], "fPIC": [True, False], "build_tests": [True, False]}
    default_options = "shared=False", "fPIC=True", "build_tests=False"
    generators = "cmake", "cmake_find_package", "cmake_paths"
    requires = ("is-wire/[>=1.1.2]@is/stable", "is-msgs/[>=1.1.7]@is/stable",
                "opencv/[>=3.3]@is/stable", "zipkin-cpp-opentracing/0.3.1@is/stable")
    exports_sources = "*"

    def build_requirements(self):
        if self.options.build_tests:
            self.build_requires("gtest/1.8.0@bincrafters/stable")

    def configure(self):
        self.options["opencv"].with_qt = False
        self.options["is-msgs"].shared = True

    def build(self):
        cmake = CMake(self, generator='Ninja')
        cmake.definitions["CMAKE_POSITION_INDEPENDENT_CODE"] = self.options.fPIC
        cmake.definitions["enable_tests"] = self.options.build_tests
        cmake.configure()
        cmake.build()
        if self.options.build_tests:
            cmake.test()
        cmake.install()

    def package_info(self):
        self.cpp_info.libs = ["is-aruco"]
