load("@bazel_arm_none//:utils.bzl", "gdb_bin")

cc_library(
  name = "starlink-pd-supply-lib",
  hdrs = glob(["include/**/*.h"]),
  srcs = glob(["src/**/*.cpp"], exclude = ["src/startup.cpp", "src/handlers.cpp"]),
  strip_include_prefix = "include/",
  deps = [
  ],
  copts = [
    "-ggdb",
    "-mthumb",
    "-mcpu=cortex-m0plus",
    "-specs=nano.specs",
    "-specs=nosys.specs",
    "-std=c++11",
    "-fno-exceptions",
  ],
)

cc_binary(
  name = "starlink-pd-supply",
  srcs = [
      "src/startup.cpp",
      "src/handlers.cpp",
  ],
  deps = [
    ":starlink-pd-supply-lib",
    ":ld/STM32L011F4.ld",
  ],
  copts = [
    "-ggdb",
    "-mthumb",
    "-mcpu=cortex-m0plus",
    "-specs=nano.specs",
    "-specs=nosys.specs",
    "-std=c++11",
    "-fno-exceptions",
  ],
  linkopts = [
    "-T $(location :ld/STM32L011F4.ld)",
    "-mthumb",
    "-mcpu=cortex-m0plus",
    "-specs=nano.specs",
    "-specs=nosys.specs",
    "-lm",
    "-lsupc++",
    "-lstdc++",
    "-fno-exceptions",
    "-Xlinker -Map=/tmp/starlink-pd.map",
  ],
)

gdb_bin()

