#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string_view>

#include <entt/entt.hpp>

#include "evolution/sim/math_types.h"

namespace evolution::sim {

/**
 * @brief Low-level physics backend interface accessed by PhysicsSystem.
 *
 * @note Implementations must guarantee deterministic iteration order when invoked with identical inputs.
 */
class IPhysicsBackend {
public:
    /**
     * @brief Describes a single contact event emitted during a simulation tick.
     */
    struct ContactEvent {
        entt::entity entity_a{entt::null};  ///< First entity in contact pair.
        entt::entity entity_b{entt::null};  ///< Second entity in contact pair.
        bool begin{false};                  ///< True when contact starts this tick.
        bool stay{false};                   ///< True while contact persists.
        bool end{false};                    ///< True when contact ends this tick.
        Vec3 normal{0.0, 1.0, 0.0};         ///< Contact normal pointing from A to B.
        Vec3 point{0.0, 0.0, 0.0};          ///< Contact point in world coordinates.
        double penetration{0.0};            ///< Overlap depth in meters.
    };

    /**
     * @brief Diagnostic statistics reported by a backend after stepping.
     */
    struct Stats {
        std::size_t bodies{0};    ///< Dynamic body count processed.
        std::size_t pairs{0};     ///< Broad-phase candidate pair count.
        std::size_t contacts{0};  ///< Narrow-phase contact point count.
        int solver_iterations{0}; ///< Iterations executed in the constraint solver.
    };

    /**
     * @brief Describes configuration parameters applicable to most backends.
     */
    struct Config {
        double cell_size{1.0};         ///< Spatial hash cell size in world units.
        int solver_iterations{6};      ///< Number of sequential impulse iterations.
        double baumgarte{0.2};         ///< Positional correction factor β.
        double penetration_slop{0.01}; ///< Allowed penetration before correction (meters).
    };

    /// @brief Virtual destructor for polymorphic deletion.
    virtual ~IPhysicsBackend() = default;

    /**
     * @brief Applies configuration parameters to the backend.
     *
     * @param config const Config& Immutable configuration structure.
     * @note Backends may clamp values to maintain stability.
     */
    virtual void configure(const Config& config) = 0;

    /**
     * @brief Synchronizes internal caches with the ECS registry.
     *
     * @param registry entt::registry& Entity registry containing physics components.
     * @complexity O(N) where N is the number of bodies possessing physics components.
     */
    virtual void sync_from_registry(entt::registry& registry) = 0;

    /**
     * @brief Advances the simulation by a fixed timestep.
     *
     * @param registry entt::registry& Registry to be mutated with updated transforms and velocities.
     * @param dt double Timestep in seconds.
     * @warning Must be called after sync_from_registry within the same tick.
     * @complexity O(N + P) where P is number of contact pairs.
     */
    virtual void step(entt::registry& registry, double dt) = 0;

    /**
     * @brief Performs a raycast against the current physics scene.
     *
     * @param origin Vec3 World-space origin of the ray.
     * @param direction Vec3 Normalized ray direction.
     * @param max_distance double Maximum distance in meters to query.
     * @return std::optional<ContactEvent> Closest hit information when available.
     * @note Implementations may approximate or return std::nullopt if unsupported.
     */
    [[nodiscard]] virtual std::optional<ContactEvent> raycast(const Vec3& origin,
                                                              const Vec3& direction,
                                                              double max_distance) const = 0;

    /**
     * @brief Provides read-only access to contact events generated during the previous step.
     *
     * @return std::span<const ContactEvent> Contiguous view of contact events.
     * @warning The span becomes invalid after the next call to step().
     */
    [[nodiscard]] virtual std::span<const ContactEvent> contact_events() const = 0;

    /**
     * @brief Returns statistics collected during the last simulation step.
     *
     * @return Stats Aggregated counters for diagnostics.
     */
    [[nodiscard]] virtual Stats stats() const = 0;
};

}  // namespace evolution::sim


