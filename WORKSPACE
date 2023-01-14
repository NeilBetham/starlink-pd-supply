workspace(name = "starlink-pd-supply")

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

# External dep compilation tools
http_archive(
  name = "rules_foreign_cc",
  url = "https://github.com/bazelbuild/rules_foreign_cc/archive/3b72ab3468cc8b101352dbe681353a2f8821a057.tar.gz",
  sha256 = "682fa59997d214d42743d822a1284780fd8fb0db4dd88bcb0725904b423cef20",
  strip_prefix="rules_foreign_cc-3b72ab3468cc8b101352dbe681353a2f8821a057",
)
load("@rules_foreign_cc//foreign_cc:repositories.bzl", "rules_foreign_cc_dependencies")
rules_foreign_cc_dependencies()

# Bazel Toolchain for ARM embedded
http_archive(
  name = "bazel_arm_none",
  url = "https://github.com/NeilBetham/bazel-arm-none/archive/858d8e0c6b060c456d96fe067a6d1b622c429872.tar.gz",
  sha256 = "28b18c1ec140fba42c036e8c7ba94dcdf06e3a4428726bf5fa3b6436b457c553",
  strip_prefix = "bazel-arm-none-858d8e0c6b060c456d96fe067a6d1b622c429872",
)

load("@bazel_arm_none//:deps.bzl", "toolchain_deps")
toolchain_deps()


