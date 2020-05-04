package(default_visibility = ["//visibility:public"])

cc_library(
  name = "tangent-gtk",
  srcs = glob(
    [
      "*.c",
      "*.cc",
    ],
    exclude = [
      "*demo.cc",
      "*_test.cc",
    ],
  ),
  hdrs = glob(["*.h"]),
  deps = [
    "//argue",
    "//tangent/json",
    "//third_party:phash",
    "@system//:eigen3",
    "@system//:fmt",
    "@system//:gtk-3.0",
    "@system//:gtkmm-3.0",
    "@system//:re2",
    "@system//:tinyxml2",
  ],
)

cc_binary(
  name = "gtk_panzoom_demo",
  srcs = ["panzoomdemo.cc"],
  deps = [
    ":tangent-gtk",
    "@system//:libX11",
  ],
)

cc_binary(
  name = "gtk_serialize_demo",
  srcs = ["serializedemo.cc"],
  deps = [
    ":tangent-gtk",
    "@system//:libX11",
  ],
)

cc_test(
  name = "colormap-test",
  srcs = ["colormap_test.cc"],
  deps = [
    ":tangent-gtk",
    "//third_party/googletest:gtest",
    "//third_party/googletest:gtest_main",
  ],
)

py_test(
  name = "panzoom-test",
  timeout = "moderate",
  srcs = ["panzoom_test.py"],
  args = ["tangent/gtkutil"],
  data = [
    "panzoom.ui",
    "panzoom-base.ui",
    ":gtk_panzoom_demo",
  ],
  main = "panzoom_test.py",
  python_version = "PY3",
  tags = [
    "exclusive",
    "local",
  ],
)

py_test(
  name = "serialize-test",
  timeout = "short",
  srcs = ["serialize_test.py"],
  args = ["tangent/gtkutil"],
  data = [
    "serialize.ui",
    ":gtk_serialize_demo",
  ],
  main = "serialize_test.py",
  python_version = "PY3",
  tags = [
    "exclusive",
    "local",
  ],
)
