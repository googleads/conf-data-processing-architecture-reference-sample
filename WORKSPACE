load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

############
# SCP Repo #
############

# Open source SCP repo version tag
SCP_VERSION = "v0.174.0"  # latest as of Wed Sept 18 12:38:00 PM UTC 2024

# The following lines are utilized by the `build_worker.sh` script
# when setting up the environment within a container.
# Since internal dependencies cannot be fetched within a Docker container,
# we upload a local copy of the repo to the container and use that as a
# dependency instead.
#
# CONTAINER_BUILD__UNCOMMENT:local_repository(
# CONTAINER_BUILD__UNCOMMENT:    name = "com_google_adm_cloud_scp",
# CONTAINER_BUILD__UNCOMMENT:    path = "../pairbuild/scp",
# CONTAINER_BUILD__UNCOMMENT:)
#
# CONTAINER_BUILD__REMOVE_SECTION.START
git_repository(
    name = "com_google_adm_cloud_scp",
    remote = "https://github.com/googleads/conf-data-processing-architecture-reference/",
    tag = SCP_VERSION,
)
# CONTAINER_BUILD__REMOVE_SECTION.END

# Use following instead of git_repository for local development
#local_repository(
#    name = "com_google_adm_cloud_scp",
#    path = "<path to location SCP workspace>",
#)

load("//build_defs/cc:pair_dependencies.bzl", "pair_dependencies")

# Declare explicit protobuf version and hash, to override any implicit dependencies.
# Please update both while upgrading to new versions.
PROTOBUF_CORE_VERSION = "24.4"

PROTOBUF_SHA_256 = "616bb3536ac1fff3fb1a141450fa28b875e985712170ea7f1bfe5e5fc41e2cd8"

pair_dependencies(PROTOBUF_CORE_VERSION, PROTOBUF_SHA_256)

####################
# JVM Dependencies #
####################

################################################################################
# Rules JVM External: Begin
################################################################################
RULES_JVM_EXTERNAL_TAG = "4.3"

RULES_JVM_EXTERNAL_SHA = "6274687f6fc5783b589f56a2f1ed60de3ce1f99bc4e8f9edef3de43bdf7c6e74"

http_archive(
    name = "rules_jvm_external",
    sha256 = RULES_JVM_EXTERNAL_SHA,
    strip_prefix = "rules_jvm_external-%s" % RULES_JVM_EXTERNAL_TAG,
    url = "https://github.com/bazelbuild/rules_jvm_external/archive/%s.zip" % RULES_JVM_EXTERNAL_TAG,
)

load("@rules_jvm_external//:repositories.bzl", "rules_jvm_external_deps")

rules_jvm_external_deps()

load("@rules_jvm_external//:setup.bzl", "rules_jvm_external_setup")

rules_jvm_external_setup()

################################################################################
# Rules JVM External: End
################################################################################

load("//build_defs/java:maven_dependencies.bzl", "maven_dependencies")

maven_dependencies()

#############
# CPP Rules #
#############
load("@rules_cc//cc:repositories.bzl", "rules_cc_dependencies", "rules_cc_toolchains")

rules_cc_dependencies()

rules_cc_toolchains()

###########################
# CC Dependencies         #
###########################

# Load indirect dependencies due to
#     https://github.com/bazelbuild/bazel/issues/1943
load("@com_github_googleapis_google_cloud_cpp//bazel:google_cloud_cpp_deps.bzl", "google_cloud_cpp_deps")

google_cloud_cpp_deps()

load("@com_google_googleapis//:repository_rules.bzl", "switched_rules_by_language")

switched_rules_by_language(
    name = "com_google_googleapis_imports",
    cc = True,
    grpc = True,
)

# Boost
load("@com_github_nelhage_rules_boost//:boost/boost.bzl", "boost_deps")

boost_deps()

load("@rules_foreign_cc//foreign_cc:repositories.bzl", "rules_foreign_cc_dependencies")

rules_foreign_cc_dependencies()

##########
# GRPC C #
##########
# These dependencies from @com_github_grpc_grpc need to be loaded after the
# google cloud deps so that the grpc version can be set by the google cloud deps
load("@com_github_grpc_grpc//bazel:grpc_deps.bzl", "grpc_deps")

grpc_deps()

load("@com_github_grpc_grpc//bazel:grpc_extra_deps.bzl", "grpc_extra_deps")

grpc_extra_deps()

###############
# Proto rules #
###############
load("@rules_proto//proto:repositories.bzl", "rules_proto_dependencies", "rules_proto_toolchains")

rules_proto_dependencies()

rules_proto_toolchains()

load("@com_google_protobuf//:protobuf_deps.bzl", "protobuf_deps")

protobuf_deps()

############
# Go rules #
############
# Need to be after grpc_extra_deps to share go_register_toolchains.
load("@io_bazel_rules_go//go:deps.bzl", "go_rules_dependencies")

go_rules_dependencies()

###################
# Container rules #
###################
load(
    "@io_bazel_rules_docker//repositories:repositories.bzl",
    container_repositories = "repositories",
)

container_repositories()

load("@io_bazel_rules_docker//repositories:deps.bzl", container_deps = "deps")

container_deps()

#############
# PKG rules #
#############
load("@rules_pkg//:deps.bzl", "rules_pkg_dependencies")

rules_pkg_dependencies()

###############################
# Non-opensource dependencies #
###############################
load("//:non_opensource_deps.bzl", "non_opensource_deps")

non_opensource_deps()

#############
# SDK Deps #
#############
load("@com_google_adm_cloud_scp//build_defs/cc:sdk.bzl", "sdk_dependencies")

PROTOBUF_CORE_VERSION_FOR_CC = "24.4"

PROTOBUF_SHA_256_FOR_CC = "616bb3536ac1fff3fb1a141450fa28b875e985712170ea7f1bfe5e5fc41e2cd8"

sdk_dependencies(PROTOBUF_CORE_VERSION_FOR_CC, PROTOBUF_SHA_256_FOR_CC)

#########################
## NodeJS dependencies ##
#########################

http_archive(
    name = "build_bazel_rules_nodejs",
    sha256 = "94070eff79305be05b7699207fbac5d2608054dd53e6109f7d00d923919ff45a",
    urls = ["https://github.com/bazelbuild/rules_nodejs/releases/download/5.8.2/rules_nodejs-5.8.2.tar.gz"],
)

load("@build_bazel_rules_nodejs//:repositories.bzl", "build_bazel_rules_nodejs_dependencies")

build_bazel_rules_nodejs_dependencies()

load("@build_bazel_rules_nodejs//:index.bzl", "npm_install")

npm_install(
    name = "npm",
    package_json = "//typescript/coordinator/aws/adtechmanagement:package.json",
    package_lock_json = "//typescript/coordinator/aws/adtechmanagement:package-lock.json",
)

#######################
## rules_esbuild setup #
#######################

load("@build_bazel_rules_nodejs//toolchains/esbuild:esbuild_repositories.bzl", "esbuild_repositories")

esbuild_repositories(npm_repository = "npm")

load("@io_bazel_rules_docker//container:container.bzl", "container_pull")

# Distroless debug image for running Java. Need to use debug image to install more dependencies for CC.
container_pull(
    name = "java_debug_runtime",
    # Using SHA-256 for reproducibility.
    digest = "sha256:f311c37af17ac6e96c44f5990aa2bb5070da32d5e4abc11b2124750c1062c308",
    registry = "gcr.io",
    repository = "distroless/java17-debian11",
    tag = "debug-nonroot-amd64",
)
