#pragma once

#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

#include "evolution/sim/environment/environment.h"
#include "evolution/sim/physics/backend.h"
#include "evolution/sim/physics/broad_phase.h"
#include "evolution/sim/physics/narrow_phase.h"
#include "evolution/sim/physics/physics_types.h"
#include "evolution/sim/physics/solver.h"

namespace evolution::sim {

/**
 * @brief Configuration bundle for the simple CPU-based physics backend.
 */
struct SimplePhysicsConfig {
    double gravity{-9.81};        ///< Constant gravity along Y axis.
    double ground_height{0.0};    ///< Static ground plane height.
    IPhysicsBackend::Config core; ///< Generic backend configuration.
    bool enable_heightfield{true};///< When true, sample terrain from registry context.
};

/**
 * @brief Deterministic CPU physics backend performing broad-phase, narrow-phase, and constraint solving.
 *
 * @note This implementation is intended for headless simulation and serves as the reference for alternative backends.
 */
class SimplePhysicsBackend final : public IPhysicsBackend {
public:
    /**
     * @brief Constructs the backend with a configuration structure.
     *
     * @param config const SimplePhysicsConfig& Initial configuration.
     */
    explicit SimplePhysicsBackend(const SimplePhysicsConfig& config);

    void configure(const Config& config) override;
    void sync_from_registry(entt::registry& registry) override;
    void step(entt::registry& registry, double dt) override;
    [[nodiscard]] std::optional<ContactEvent> raycast(const Vec3& origin,
                                                      const Vec3& direction,
                                                      double max_distance) const override;
    [[nodiscard]] std::span<const ContactEvent> contact_events() const override;
    [[nodiscard]] Stats stats() const override;

    /**
     * @brief Updates gravity vector applied during integration.
     *
     * @param gravity Vec3 Gravity acceleration in meters per second squared.
     */
    void set_gravity(const Vec3& gravity) noexcept;

    /**
     * @brief Sets the height of the infinite ground plane.
     *
     * @param ground_y double Ground plane Y coordinate in meters.
     */
    void set_ground_height(double ground_y) noexcept;

private:
    struct BodyRecord {
        entt::entity entity{entt::null};
        Vec3 position{0.0, 0.0, 0.0};
        double inverse_mass{0.0};
        TransformComponent* transform{nullptr};
        ColliderComponent* collider{nullptr};
        KinematicsComponent* kinematics{nullptr};
        RigidbodyComponent* body_flags{nullptr};
        ContactManifold manifold{};
    };

    void rebuild_body_records(entt::registry& registry);
    void generate_broad_phase_pairs();
    void run_narrow_phase();
    void integrate_forces(double dt);
    void integrate_velocities(double dt);
    void dispatch_contact_events();

    SimplePhysicsConfig config_{};
    Vec3 gravity_{0.0, -9.81, 0.0};
    const Terrain* terrain_{nullptr};
    SpatialHash spatial_hash_{};
    std::vector<BodyRecord> bodies_{};
    std::vector<std::pair<entt::entity, entt::entity>> candidate_pairs_{};
    std::vector<ContactManifold> manifolds_{};
    std::vector<ContactEvent> contact_events_{};
    std::vector<ContactEvent> previous_contacts_{};
    std::unordered_map<entt::entity, std::size_t> body_lookup_{};
    Stats stats_{};
};

}  // namespace evolution::sim


