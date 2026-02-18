#pragma once

#include <glm/glm.hpp>
#include <GLFW/glfw3.h>

namespace evolution::client {

enum class CameraMode {
    Orbit = 0,
    Free = 1,
    Follow = 2,
};

class CameraController {
public:
    void set_mode(CameraMode mode) noexcept { mode_ = mode; }
    [[nodiscard]] CameraMode mode() const noexcept { return mode_; }

    void set_target(const glm::vec3& target) noexcept { target_ = target; }
    [[nodiscard]] const glm::vec3& target() const noexcept { return target_; }

    void set_follow_target(const glm::vec3& position) noexcept { follow_target_ = position; }

    void set_free_position(const glm::vec3& position) noexcept { free_position_ = position; }
    void set_free_angles(float yaw, float pitch) noexcept {
        free_yaw_ = yaw;
        free_pitch_ = pitch;
    }

    void update(GLFWwindow* window, float dt, bool input_captured_by_ui);

    [[nodiscard]] glm::vec3 eye() const noexcept;
    [[nodiscard]] glm::mat4 view_matrix() const noexcept;

    [[nodiscard]] float distance() const noexcept { return distance_; }
    [[nodiscard]] float yaw() const noexcept { return yaw_; }
    [[nodiscard]] float pitch() const noexcept { return pitch_; }

private:
    CameraMode mode_{CameraMode::Orbit};

    glm::vec3 target_{0.0F, 10.0F, 0.0F};
    glm::vec3 follow_target_{0.0F, 10.0F, 0.0F};

    float distance_{80.0F};
    float yaw_{0.0F};
    float pitch_{0.55F};

    glm::vec3 free_position_{0.0F, 20.0F, 80.0F};
    float free_yaw_{0.0F};
    float free_pitch_{-0.2F};
    bool mouse_look_latch_{false};
    double last_mouse_x_{0.0};
    double last_mouse_y_{0.0};
};

}  // namespace evolution::client
