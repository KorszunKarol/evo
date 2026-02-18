#include "renderer/camera_controller.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

namespace evolution::client {

namespace {

[[nodiscard]] glm::vec3 orbit_offset(float yaw, float pitch, float distance) {
    const float cp = std::cos(pitch);
    const float sp = std::sin(pitch);
    const float cy = std::cos(yaw);
    const float sy = std::sin(yaw);
    return glm::vec3(distance * cp * cy, distance * sp, distance * cp * sy);
}

[[nodiscard]] glm::vec3 free_forward(float yaw, float pitch) {
    const float cp = std::cos(pitch);
    return glm::normalize(glm::vec3(cp * std::cos(yaw), std::sin(pitch), cp * std::sin(yaw)));
}

}  // namespace

void CameraController::update(GLFWwindow* window, float dt, bool input_captured_by_ui) {
    const float rot_speed = glm::radians(70.0F);
    const float zoom_speed = 30.0F;
    if (input_captured_by_ui) {
        return;
    }

    if (mode_ == CameraMode::Orbit || mode_ == CameraMode::Follow) {
        if (mode_ == CameraMode::Follow) {
            target_ = glm::mix(target_, follow_target_, std::clamp(6.0F * dt, 0.0F, 1.0F));
        }
        const glm::vec3 fwd = glm::normalize(glm::vec3(std::cos(yaw_), 0.0F, std::sin(yaw_)));
        const glm::vec3 right = glm::normalize(glm::cross(fwd, glm::vec3(0.0F, 1.0F, 0.0F)));
        const float pan_speed = std::max(6.0F, distance_ * 0.45F);
        if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) {
            yaw_ -= rot_speed * dt;
        }
        if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) {
            yaw_ += rot_speed * dt;
        }
        if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) {
            pitch_ += rot_speed * dt;
        }
        if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) {
            pitch_ -= rot_speed * dt;
        }
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
            target_ += fwd * pan_speed * dt;
        }
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
            target_ -= fwd * pan_speed * dt;
        }
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
            target_ -= right * pan_speed * dt;
        }
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
            target_ += right * pan_speed * dt;
        }
        if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {
            distance_ -= zoom_speed * dt;
        }
        if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) {
            distance_ += zoom_speed * dt;
        }

        pitch_ = std::clamp(pitch_, glm::radians(-80.0F), glm::radians(80.0F));
        distance_ = std::clamp(distance_, 5.0F, 600.0F);
        return;
    }

    const float look_speed = glm::radians(80.0F);
    const float move_speed = 40.0F;
    if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) {
        free_yaw_ -= look_speed * dt;
    }
    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) {
        free_yaw_ += look_speed * dt;
    }
    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) {
        free_pitch_ += look_speed * dt;
    }
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) {
        free_pitch_ -= look_speed * dt;
    }
    free_pitch_ = std::clamp(free_pitch_, glm::radians(-85.0F), glm::radians(85.0F));

    const bool mouse_look_active = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
    double mouse_x = 0.0;
    double mouse_y = 0.0;
    glfwGetCursorPos(window, &mouse_x, &mouse_y);
    if (mouse_look_active) {
        if (!mouse_look_latch_) {
            mouse_look_latch_ = true;
            last_mouse_x_ = mouse_x;
            last_mouse_y_ = mouse_y;
        } else {
            const float dx = static_cast<float>(mouse_x - last_mouse_x_);
            const float dy = static_cast<float>(mouse_y - last_mouse_y_);
            constexpr float sensitivity = 0.0035F;
            free_yaw_ += dx * sensitivity;
            free_pitch_ -= dy * sensitivity;
            free_pitch_ = std::clamp(free_pitch_, glm::radians(-85.0F), glm::radians(85.0F));
            last_mouse_x_ = mouse_x;
            last_mouse_y_ = mouse_y;
        }
    } else {
        mouse_look_latch_ = false;
    }

    const glm::vec3 forward = free_forward(free_yaw_, free_pitch_);
    const glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0F, 1.0F, 0.0F)));

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        free_position_ += forward * move_speed * dt;
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        free_position_ -= forward * move_speed * dt;
    }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        free_position_ -= right * move_speed * dt;
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        free_position_ += right * move_speed * dt;
    }
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {
        free_position_.y -= move_speed * 0.6F * dt;
    }
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) {
        free_position_.y += move_speed * 0.6F * dt;
    }
}

glm::vec3 CameraController::eye() const noexcept {
    if (mode_ == CameraMode::Orbit || mode_ == CameraMode::Follow) {
        return target_ + orbit_offset(yaw_, pitch_, distance_);
    }
    return free_position_;
}

glm::mat4 CameraController::view_matrix() const noexcept {
    if (mode_ == CameraMode::Orbit || mode_ == CameraMode::Follow) {
        return glm::lookAt(eye(), target_, glm::vec3(0.0F, 1.0F, 0.0F));
    }
    const glm::vec3 forward = free_forward(free_yaw_, free_pitch_);
    return glm::lookAt(free_position_, free_position_ + forward, glm::vec3(0.0F, 1.0F, 0.0F));
}

}  // namespace evolution::client
