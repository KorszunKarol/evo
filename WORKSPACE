workspace(name = "evolution")

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

http_archive(
    name = "entt",
    urls = ["https://github.com/skypjack/entt/archive/refs/tags/v3.12.2.tar.gz"],
    strip_prefix = "entt-3.12.2",
    build_file_content = """
cc_library(
    name = "entt",
    hdrs = glob(["src/**/*.hpp", "src/**/*.h"]),
    includes = ["src"],
    visibility = ["//visibility:public"],
)
""",
)

http_archive(
    name = "spdlog",
    urls = ["https://github.com/gabime/spdlog/archive/refs/tags/v1.13.0.tar.gz"],
    strip_prefix = "spdlog-1.13.0",
    build_file_content = """
cc_library(
    name = "spdlog",
    hdrs = glob(["include/**/*.h", "include/**/*.hpp"]),
    includes = ["include"],
    visibility = ["//visibility:public"],
)
""",
)

http_archive(
    name = "flatbuffers",
    urls = ["https://github.com/google/flatbuffers/archive/refs/tags/v24.3.25.tar.gz"],
    strip_prefix = "flatbuffers-24.3.25",
    build_file_content = """
cc_library(
    name = "flatbuffers",
    hdrs = glob(["include/**/*.h", "include/**/*.hpp"]),
    includes = ["include"],
    visibility = ["//visibility:public"],
)
""",
)
