#include "test_render_fixtures.h"

#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

namespace evolution::client::test {

// Camera structures and functions matching main.cpp
namespace {
struct OrbitCamera {
    glm::vec3 target{0.0F, 10.0F, 0.0F};
    float distance{85.0F};
    float yaw{0.0F};
    float pitch{0.65F};
};

[[nodiscard]] glm::vec3 compute_camera_position(const OrbitCamera& camera) {
    const float cp = std::cos(camera.pitch);
    const float sp = std::sin(camera.pitch);
    const float cy = std::cos(camera.yaw);
    const float sy = std::sin(camera.yaw);
    return camera.target + glm::vec3(camera.distance * cp * cy, camera.distance * sp,
                                     camera.distance * cp * sy);
}
}  // namespace

TEST(CameraTest, ComputeCameraPositionDefault) {
    OrbitCamera camera{};
    camera.target = glm::vec3(0.0F, 0.0F, 0.0F);
    camera.distance = 10.0F;
    camera.yaw = 0.0F;
    camera.pitch = glm::pi<float>() / 4.0F;  // 45 degrees

    const glm::vec3 pos = compute_camera_position(camera);

    EXPECT_NEAR(pos.x, 10.0F * std::cos(glm::pi<float>() / 4.0F), 0.01F);
    EXPECT_GT(pos.y, 0.0F);  // Positive pitch means looking up
    EXPECT_NEAR(pos.z, 0.0F, 0.01F);
}

TEST(CameraTest, ComputeCameraPositionYawRotation) {
    OrbitCamera camera{};
    camera.target = glm::vec3(0.0F, 0.0F, 0.0F);
    camera.distance = 10.0F;
    camera.yaw = glm::pi<float>();  // 180 degrees
    camera.pitch = 0.0F;

    const glm::vec3 pos = compute_camera_position(camera);

    EXPECT_NEAR(pos.x, -10.0F, 0.01F);
    EXPECT_NEAR(pos.y, 0.0F, 0.01F);
    EXPECT_NEAR(pos.z, 0.0F, 0.01F);
}

TEST(CameraTest, ComputeCameraPositionPitchRotation) {
    OrbitCamera camera{};
    camera.target = glm::vec3(0.0F, 0.0F, 0.0F);
    camera.distance = 10.0F;
    camera.yaw = 0.0F;
    camera.pitch = glm::pi<float>() / 2.0F;  // 90 degrees (straight up)

    const glm::vec3 pos = compute_camera_position(camera);

    EXPECT_NEAR(pos.x, 0.0F, 0.01F);
    EXPECT_NEAR(pos.y, 10.0F, 0.01F);
    EXPECT_NEAR(pos.z, 0.0F, 0.01F);
}

TEST(CameraTest, ComputeCameraPositionDistance) {
    OrbitCamera camera{};
    camera.target = glm::vec3(5.0F, 10.0F, 5.0F);
    camera.distance = 20.0F;
    camera.yaw = 0.0F;
    camera.pitch = 0.0F;

    const glm::vec3 pos = compute_camera_position(camera);

    const float distance_from_target =
        glm::length(pos - camera.target);
    EXPECT_NEAR(distance_from_target, 20.0F, 0.01F);
}

TEST(CameraTest, ComputeCameraPositionTargetOffset) {
    OrbitCamera camera{};
    camera.target = glm::vec3(100.0F, 50.0F, 200.0F);
    camera.distance = 30.0F;
    camera.yaw = 0.0F;
    camera.pitch = 0.0F;

    const glm::vec3 pos = compute_camera_position(camera);

    EXPECT_NEAR(pos.x, 130.0F, 0.01F);  // target.x + distance
    EXPECT_NEAR(pos.y, 50.0F, 0.01F);
    EXPECT_NEAR(pos.z, 200.0F, 0.01F);
}

TEST(CameraTest, ComputeCameraPositionCombinedRotation) {
    OrbitCamera camera{};
    camera.target = glm::vec3(0.0F, 0.0F, 0.0F);
    camera.distance = 10.0F;
    camera.yaw = glm::pi<float>() / 4.0F;  // 45 degrees
    camera.pitch = glm::pi<float>() / 6.0F;  // 30 degrees

    const glm::vec3 pos = compute_camera_position(camera);

    const float distance = glm::length(pos - camera.target);
    EXPECT_NEAR(distance, 10.0F, 0.01F);
}

TEST(CameraTest, ComputeCameraPositionNegativePitch) {
    OrbitCamera camera{};
    camera.target = glm::vec3(0.0F, 0.0F, 0.0F);
    camera.distance = 10.0F;
    camera.yaw = 0.0F;
    camera.pitch = -glm::pi<float>() / 4.0F;  // -45 degrees

    const glm::vec3 pos = compute_camera_position(camera);

    EXPECT_LT(pos.y, 0.0F);  // Negative pitch means looking down
}

}  // namespace evolution::client::test

