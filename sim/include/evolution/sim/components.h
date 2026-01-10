#pragma once

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include <entt/entt.hpp>

#include "evolution/sim/math_types.h"

namespace evolution::sim {

///
/// @brief Stores spatial information for an entity within the simulation world.
///
/// @note Currently encapsulates only position; rotation and scaling are deferred until morphology demands it.
/// @notthreadsafe Mutations require external synchronization in multithreaded contexts.
struct TransformComponent {
    /// @brief World-space position in meters.
    Vec3 position{0.0, 0.0, 0.0};
};

///
/// @brief Contains velocity and force accumulation for simple integration.
///
/// @note Applied forces are reset each tick after integration.
/// @warning Ensure inverse_mass is positive; zero implies immovable body.
/// @notthreadsafe Not inherently synchronized; protect when accessed from multiple threads.
struct KinematicsComponent {
    /// @brief Current linear velocity in meters per second.
    Vec3 linear_velocity{0.0, 0.0, 0.0};
    /// @brief Forces accumulated during a tick; cleared post-integration.
    Vec3 accumulated_force{0.0, 0.0, 0.0};
    /// @brief Reciprocal of mass; simplifies integration math.
    double inverse_mass{1.0};
    /// @brief Exponential damping factor applied each tick to linear velocity.
    /// @note Effective multiplier per tick is (1 - linear_damping * dt); values should remain in [0, 1].
    double linear_damping{0.05};
    /// @brief Coefficient of restitution used for collision response (0 = inelastic, 1 = perfectly elastic).
    double restitution{0.0};
    /// @brief Coulomb-style friction coefficient acting against tangential motion on contact.
    /// @warning Simplified implementation assumes isotropic friction; advanced models should replace it.
    double friction{0.6};
};

///
/// @brief Surface interaction coefficients used during collision resolution.
///
/// @note These values override the defaults stored on KinematicsComponent when a collider is present.
/// @notthreadsafe Treated as immutable after assignment; external synchronization required for changes at runtime.
///
struct PhysicsMaterial {
    /// @brief Dimensionless coefficient controlling tangential impulse limits.
    double friction{0.6};
    /// @brief Coefficient of restitution determining post-impact normal velocity ratio.
    double restitution{0.0};
};

///
/// @brief Collision filtering mask that controls which entities interact.
///
/// @note `category` identifies the collider's group; `mask` enumerates categories it will collide with.
///
struct CollisionFilter {
    /// @brief Category bits associated with this collider.
    std::uint32_t category{0x1};
    /// @brief Bitmask of categories this collider should overlap with.
    std::uint32_t mask{0xFFFFFFFF};
    /// @brief When true, contacts fire events but do not generate impulses.
    bool is_trigger{false};
};

///
/// @brief Enumerates supported analytic collider shape types.
///
enum class ShapeType {
    Sphere,   ///< Spherical collider defined by radius.
    Aabb,     ///< Axis-aligned bounding box.
    CapsuleY  ///< Upright capsule aligned with the world Y axis.
};

///
/// @brief Parameters for a spherical collider.
///
struct SphereCollider {
    /// @brief Radius measured in meters.
    double radius{0.5};
};

///
/// @brief Parameters for an axis-aligned box collider.
///
struct AabbCollider {
    /// @brief Half extents for each axis in meters.
    Vec3 half_extents{0.5, 0.5, 0.5};
};

///
/// @brief Parameters for an upright capsule aligned with the Y axis.
///
struct CapsuleYCollider {
    /// @brief Radius of the spherical caps in meters.
    double radius{0.3};
    /// @brief Half of the cylindrical section height in meters.
    double half_height{0.7};
};

///
/// @brief Collider definition combining shape data, materials, and filters.
///
/// @note The collider's world-space pose is derived by offsetting the owning entity's transform.
///
struct ColliderComponent {
    /// @brief Shape type enumerator selecting which payload is active.
    ShapeType type{ShapeType::Sphere};
    /// @brief Local-space offset applied relative to the entity's transform position.
    Vec3 offset{0.0, 0.0, 0.0};
    /// @brief Surface material parameters used for contact response.
    PhysicsMaterial material{};
    /// @brief Collision filtering and trigger settings.
    CollisionFilter filter{};
    /// @brief Sphere shape payload; valid when type == ShapeType::Sphere.
    SphereCollider sphere{};
    /// @brief AABB shape payload; valid when type == ShapeType::Aabb.
    AabbCollider aabb{};
    /// @brief Capsule shape payload; valid when type == ShapeType::CapsuleY.
    CapsuleYCollider capsule{};
};

///
/// @brief Optional rigid body flags controlling simulation behavior.
///
struct RigidbodyComponent {
    /// @brief Marks entities that should never move regardless of forces.
    bool is_static{false};
    /// @brief Marks bodies driven externally; collisions fire events but no impulses are applied.
    bool is_kinematic{false};
};

///
/// @brief Enumerates supported joint constraint types.
///
enum class JointType : std::uint8_t {
    Fixed,      ///< Locks relative position and rotation.
    Hinge,      ///< Allows rotation around a single axis.
    Spherical   ///< Allows rotation around a pivot point (ball-and-socket).
};

///
/// @brief Defines a physical constraint connecting two entities.
///
/// @note The solver uses this to apply corrective impulses or forces to maintain the constraint.
/// @warning Entities referenced must have valid Transform and Kinematics components.
///
struct JointComponent {
    /// @brief The parent entity in the kinematic chain.
    entt::entity parent{entt::null};
    /// @brief The child entity in the kinematic chain.
    entt::entity child{entt::null};
    /// @brief Anchor point in parent's local space.
    Vec3 local_anchor_parent{0.0, 0.0, 0.0};
    /// @brief Anchor point in child's local space.
    Vec3 local_anchor_child{0.0, 0.0, 0.0};
    /// @brief Axis of rotation in parent's local space (for Hinge).
    Vec3 axis_parent{1.0, 0.0, 0.0};
    /// @brief Axis of rotation in child's local space (for Hinge).
    Vec3 axis_child{1.0, 0.0, 0.0};
    /// @brief Constraint type.
    JointType type{JointType::Fixed};
    /// @brief Angular limits in radians (x=min, y=max).
    Vec3 limits{0.0, 0.0, 0.0};
    /// @brief Current angle/state for sensors (radians).
    double current_angle{0.0};
};

///
/// @brief Represents an edible plant organism that accumulates energy over time.
///
/// @details Plants consume nutrients from the soil grid to increase their energy reserves. When
///          depleted they are marked dead and eventually removed by cleanup systems.
/// @notthreadsafe Component writes require single-threaded access within the simulation pipeline.
///
struct PlantComponent {
    /// @brief Species identifier (0-4 for MVP).
    std::uint8_t species_id{0};
    /// @brief Current stored energy available for herbivores.
    double energy{10.0};
    /// @brief Upper bound for plant energy.
    double max_energy{20.0};
    /// @brief Conversion rate from soil nutrients to plant energy (energy per second).
    double growth_rate{2.0};
    /// @brief Effective radius of the plant used for feeding reach tests (meters).
    double radius{0.6};
    /// @brief Seconds between seed attempts.
    double seed_interval{20.0};
    /// @brief Internal accumulator used to schedule seeding.
    double seed_timer{0.0};
    /// @brief Tracks how long the energy has been non-positive (seconds).
    double time_since_depleted{0.0};
    /// @brief Optional delay before the plant is culled after depletion.
    double cleanup_delay{10.0};
    /// @brief Flag indicating whether the plant is considered alive.
    bool alive{true};
};

///
/// @brief Parameters that control how plants spawn seeds.
///
struct PlantSeedParams {
    /// @brief Minimum energy required before the parent can attempt seeding.
    double seed_min_energy{12.0};
    /// @brief Energy cost deducted from the parent when a seed is spawned.
    double seed_cost{4.0};
    /// @brief Radius in meters around the parent used to place seeds.
    double seed_radius{6.0};
    /// @brief Probability that a sampled location establishes a new plant.
    double establish_probability{0.65};
};

///
/// @brief Declares that an entity attempts to feed from nearby plants each tick.
///
/// @note For the initial milestone the `request_eat` flag is always treated as true.
///
struct FeedingIntent {
    /// @brief When true the feeding system will attempt to consume nearby plants.
    bool request_eat{true};
    /// @brief Maximum reach measured from the entity origin (meters).
    double reach{1.5};
    /// @brief Maximum energy transfer rate (units per second).
    double rate{6.0};
};

///
/// @brief Empty tag component marking herbivorous entities.
///
struct HerbivoreTag {};

///
/// @brief Empty tag component marking carnivorous entities.
///
struct CarnivoreTag {};

/**
 * @brief Combat state for predator entities.
 * 
 * Tracks attack cooldowns/timers and pursuit state for carnivore combat
 * mechanics. Used by the FeedingSystem to gate attack frequency and by
 * the brain to determine hunting state.
 * 
 * @complexity O(1) per tick update.
 * @note Attack timer decrements each tick; attack only allowed when timer <= 0.
 * @threadsafe @notthreadsafe Updates expected on main simulation thread.
 */
struct CombatComponent {
    /// @brief Duration between attack attempts (seconds).
    double attack_cooldown{1.0};
    /// @brief Time remaining before next attack is permitted (seconds).
    double attack_timer{0.0};
    /// @brief Current attack target entity handle.
    entt::entity target{entt::null};
    /// @brief Whether the predator is actively pursuing prey.
    bool pursuing{false};
    /// @brief Total damage dealt during current pursuit.
    double damage_dealt{0.0};
};

///
/// @brief Tracks the energetic state of an entity.
///
/// @note Energy values are expressed in arbitrary simulation units.
/// @notthreadsafe Concurrent reads/writes require external synchronization.
struct MetabolismComponent {
    /// @brief Current energy reserve.
    double energy{100.0};
    /// @brief Maximum energy capacity.
    double max_energy{100.0};
    /// @brief Basal metabolic consumption per second.
    double basal_rate{1.0};
};

/**
 * @brief Deterministic fitness metrics accumulated per entity.
 * @param None.
 * @return None.
 * @throws None.
 * @complexity O(1) storage and update per tick.
 * @note Age integrates simulation time while energy integral accumulates using the current tick's metabolic energy.
 * @warning Ensure systems update this component exactly once per tick to preserve determinism.
 * @threadsafe @notthreadsafe Updates occur on the simulation thread without external synchronization.
 */
struct FitnessComponent {
    /// @brief Simulation age expressed in seconds.
    double age_seconds{0.0};
    /// @brief Time integral of available energy (energy-seconds).
    double energy_int_accum{0.0};
    /// @brief Number of offspring spawned via reproduction systems.
    std::uint32_t offspring_count{0};
    /// @brief Latest scalar fitness value computed from accumulated metrics.
    double last_fitness{0.0};
};

///
/// @brief Associates an entity with a genome stored in external storage.
///
/// @note Genome lookup will retrieve data via this identifier in later milestones.
/// @notthreadsafe Treat as non-thread-safe due to expected mutations during reproduction.
struct GenomeHandleComponent {
    /// @brief Unique identifier referencing genome storage.
    std::uint64_t id{0};
};

///
/// @brief Optional human-readable name for debug visualization.
///
/// @notthreadsafe Typically manipulated on the main simulation thread.
struct NameComponent {
    /// @brief UTF-8 encoded entity label.
    std::string value;
};

/**
 * @brief Pending actuation commands produced by neural controllers.
 * @param None.
 * @return None.
 * @throws None.
 * @complexity O(1).
 * @note Reset every tick by the motor system after applying impulses.
 * @warning Fields are unnormalized; downstream systems must clamp as needed.
 * @threadsafe @notthreadsafe All writes expected on main simulation thread.
 */
struct ActuationComponent {
    /// @brief Desired planar impulse along the X axis (Newton seconds).
    double impulse_x{0.0};
    /// @brief Desired planar impulse along the Z axis (Newton seconds).
    double impulse_z{0.0};
    /// @brief Indicates whether a jump impulse should be applied.
    bool jump{false};
    /// @brief Request to perform feeding behaviour if available.
    bool eat{false};
    /// @brief Request to attack nearby prey (carnivores only).
    bool attack{false};
    /// @brief Brain-controlled throttle for skipping updates (frames).
    int update_skip{0};
};

/**
 * @brief Runtime metadata for the brain controller attached to an entity.
 * @param None.
 * @return None.
 * @throws None.
 * @complexity O(1).
 * @note Maintains timing information for inference scheduling.
 * @warning Acts as a handle into backend storage buffers; do not mutate indiscriminately.
 * @threadsafe @notthreadsafe.
 */
struct BrainComponent {
    /// @brief Enumerates supported brain execution backends.
    enum class Kind : std::uint8_t {
        MLP,
        NEAT,
    };

    /// @brief Selected backend for runtime inference.
    Kind kind{Kind::MLP};
    /// @brief Number of sensor inputs consumed by the brain.
    std::uint32_t input_count{0};
    /// @brief Number of actuation outputs produced by the brain.
    std::uint32_t output_count{0};
    /// @brief Target interval between inference updates in seconds.
    double update_interval{0.1};
    /// @brief Accumulated time since the last inference step.
    double accumulator{0.0};
    /// @brief Index into backend-specific storage buffers (future batching).
    std::uint32_t storage_index{std::numeric_limits<std::uint32_t>::max()};
};

/**
 * @brief Tracks lifecycle progression, stage state, and stage-driven gating data.
 * @param None.
 * @return None.
 * @throws None.
 * @complexity O(1) access; gate multipliers scale with module count.
 * @note Updated each tick to reflect age-based stage transitions.
 * @warning Gate multiplier vector resizes when module counts change.
 * @threadsafe @notthreadsafe Mutate only on the simulation thread.
 */
struct LifecycleComponent {
    /// @brief Accumulated age in simulation seconds.
    double age{0.0};
    /// @brief Baseline maximum energy prior to stage scaling.
    double base_max_energy{0.0};
    /// @brief Current energy scaling factor applied to metabolism capacities.
    double energy_scale{1.0};
    /// @brief Current size scaling factor (hint for future rendering/physics scaling).
    double size_scale{1.0};
    /// @brief Maximum stage age used for normalising age fraction (seconds).
    double max_stage_age{60.0};
    /// @brief Active life stage index derived from genome life stage table.
    std::uint32_t stage_index{0};
    /// @brief Indicates whether reproduction is permitted in the current stage.
    bool reproduction_allowed{true};
    /// @brief Per-module gate multiplier applied post-gating network.
    std::vector<double> gate_multipliers{};
};

/**
 * @brief Reproduction policy and cooldown state for an entity.
 * @param None.
 * @return None.
 * @throws None.
 * @complexity O(1).
 * @note Consumed by ReproductionSystem to gate mating opportunities.
 * @warning Timer must be advanced deterministically; keep updates within simulation thread.
 * @threadsafe @notthreadsafe.
 */
struct ReproductionComponent {
    /// @brief Minimum cooldown duration between reproduction attempts (seconds).
    double cooldown{5.0};
    /// @brief Time remaining before reproduction is allowed (seconds).
    double timer{0.0};
    /// @brief Radius within which potential mates are considered (meters).
    double mate_radius{3.0};
    /// @brief Minimum energy required to initiate reproduction.
    double energy_threshold{120.0};
};

///
/// @brief Enumerates entity types detectable by vision raycasting.
///
enum class VisionHitType : std::uint8_t {
    None = 0,      ///< No hit within range.
    Terrain = 1,   ///< Terrain/heightfield hit.
    Plant = 2,     ///< Plant entity hit.
    Herbivore = 3, ///< Herbivore agent hit.
    Carnivore = 4, ///< Carnivore agent hit.
    Unknown = 5    ///< Entity without recognizable tag.
};

///
/// @brief Enumerates supported diet types for entities.
///
enum class DietType : std::uint8_t {
    Herbivore = 0, ///< Consumes plants.
    Carnivore = 1  ///< Consumes other entities.
};

///
/// @brief Defines the feeding behaviour of an entity.
///
struct DietComponent {
    /// @brief The primary diet type.
    DietType type{DietType::Herbivore};
};

/**
 * @brief Raycast-based spatial sensing component for agent vision.
 * @param None.
 * @return None.
 * @throws None.
 * @complexity O(ray_count) per tick for raycasting.
 * @note Updated by VisionSystem before BrainInferenceSystem runs.
 * @warning ray_results and ray_types vectors resize to ray_count.
 * @threadsafe @notthreadsafe Updates expected on main simulation thread.
 */
struct VisionComponent {
    /// @brief Field of view in radians (symmetric around heading).
    float fov_radians{1.57f};
    /// @brief Number of rays to cast per update.
    std::uint8_t ray_count{5};
    /// @brief Maximum vision range in meters.
    float max_range{10.0f};
    /// @brief Enable/disable vision sensing.
    bool enabled{true};
    /// @brief Normalized ray distances [0=contact, 1=max_range].
    std::vector<float> ray_distances{};
    /// @brief Entity type detected by each ray.
    std::vector<VisionHitType> ray_hit_types{};
    /// @brief Entities hit by each ray (entt::null if terrain or miss).
    std::vector<entt::entity> ray_hit_entities{};
};

///
/// @brief Enumerates causes of agent death for telemetry analysis.
///
enum class DeathCause : std::uint8_t {
    Unknown = 0,    ///< Default, cause not determined.
    Starvation = 1, ///< Energy depleted via metabolism.
    Predation = 2,  ///< Energy drained to zero by a carnivore.
    OldAge = 3      ///< Future: lifespan limit reached.
};

/**
 * @brief Per-agent lifetime telemetry for analysis and debugging.
 * @note Updated by relevant systems (Feeding, Metabolism, Movement).
 * @complexity O(1) per update.
 */
struct TelemetryComponent {
    /// @brief Total energy gained from feeding (plants or prey).
    double total_energy_gained{0.0};
    /// @brief Total energy lost to metabolism.
    double total_energy_lost_metabolism{0.0};
    /// @brief Total energy lost to movement (future).
    double total_energy_lost_movement{0.0};
    /// @brief Total distance traveled in meters.
    double distance_traveled{0.0};
    /// @brief Number of successful feeding events.
    std::uint32_t successful_feeds{0};
    /// @brief Number of kills (carnivores only).
    std::uint32_t kill_count{0};
    /// @brief Cause of death (set just before destruction).
    DeathCause death_cause{DeathCause::Unknown};
    /// @brief Flag set by FeedingSystem when this entity is being killed by predation.
    bool killed_by_predation{false};
};

}  // namespace evolution::sim
