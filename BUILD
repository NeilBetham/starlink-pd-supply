load("@bazel_arm_none//:utils.bzl", "gdb_bin")

cc_library(
  name = "starlink-pd-supply-lib",
  hdrs = glob(["include/**/*.h"]),
  srcs = glob(["src/**/*.cpp"], exclude = ["src/startup.cpp", "src/handlers.cpp"]),
  deps = [
  ],
  strip_include_prefix = "include/",
  copts = [
    "-ggdb",
    "-mthumb",
    "-mcpu=cortex-m0plus",
    "-specs=nano.specs",
    "-specs=nosys.specs",
    "-std=c++11",
    "-fno-exceptions",
    "-fno-use-cxa-atexit",
    "-fno-rtti",
    "-fno-threadsafe-statics",
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
    "-fno-use-cxa-atexit",
    "-fno-rtti",
    "-fno-threadsafe-statics",
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
    "-Xlinker --wrap=malloc",
    "-Xlinker --wrap=free",
    "-Xlinker --wrap=atexit",
    "-Xlinker --wrap=memcpy",
    "-Xlinker --wrap=memset",
    "-Xlinker --wrap=exit",
  ],
)

gdb_bin()

