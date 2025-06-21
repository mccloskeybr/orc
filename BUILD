load("@rules_cc//cc:cc_library.bzl", "cc_library")

package(default_visibility = ["//visibility:public"])

cc_library(
  name = "orc",
  hdrs = ["orc.h"],
  deps = [],
)

cc_binary(
  name = "example",
  srcs = ["example.cc"],
  deps = ["//:orc"],
)
