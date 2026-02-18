#include "evolution/genetics/phenotype_builder.h"

#include <algorithm>
#include <limits>
#include <string>

#include <spdlog/spdlog.h>

#include "evolution/genetics/derived_traits.h"
#include "evolution/sim/components.h"

namespace evolution::genetics {

namespace {

[[nodiscard]] DerivedTraits TraitsFromCache(const evolution::genome::TraitsCache* cache) noexcept {
    if (cache == nullptr) {
        return {};
    }
    DerivedTraits traits{};
    traits.mass = static_cast<double>(cache->mass());
    traits.basal_rate = static_cast<double>(cache->basal_rate());
    traits.brain_cost = static_cast<double>(cache->brain_cost());
    traits.brain_params = static_cast<double>(cache->brain_params());
    traits.module_count = static_cast<std::size_t>(cache->module_count());
    if (const auto* latent = cache->trait_latent()) {
        const auto count = std::min<std::size_t>(latent->size(), traits.trait_latent.size());
        for (std::size_t i = 0; i < count; ++i) {
            traits.trait_latent[i] = static_cast<double>((*latent)[i]);
        }
    }
    return traits;
}

[[nodiscard]] double SafeUpdateInterval(float rate_hz) noexcept {
    const double hz = std::max(1.0f, rate_hz);
    return 1.0 / hz;
}

void ConfigureCollider(sim::ColliderComponent& collider, const evolution::genome::Body& body) {
    collider.material.friction = 0.8;
    collider.material.restitution = 0.05;
    collider.filter = sim::CollisionFilter{};

    const auto* size = body.size();
    if (size == nullptr) {
        collider.type = sim::ShapeType::Sphere;
        collider.sphere.radius = 0.5;
        return;
    }

    switch (body.shape()) {
        case evolution::genome::ShapeType::Sphere:
            collider.type = sim::ShapeType::Sphere;
            collider.sphere.radius = std::max(0.1, static_cast<double>(size->x()));
            break;
        case evolution::genome::ShapeType::CapsuleY:
            collider.type = sim::ShapeType::CapsuleY;
            collider.capsule.radius = std::max(0.1, static_cast<double>(size->x()));
            collider.capsule.half_height = std::max(0.1, static_cast<double>(size->y()));
            break;
        case evolution::genome::ShapeType::Box:
        default:
            collider.type = sim::ShapeType::Aabb;
            collider.aabb.half_extents.x = std::max(0.1, static_cast<double>(size->x()));
            collider.aabb.half_extents.y = std::max(0.1, static_cast<double>(size->y()));
            collider.aabb.half_extents.z = std::max(0.1, static_cast<double>(size->z()));
            break;
    }
}

void ConfigureBrainComponent(sim::BrainComponent& brain, const evolution::genome::Genome& genome) {
    switch (genome.brain_kind()) {
        case evolution::genome::BrainKind::MLP: {
            brain.kind = sim::BrainComponent::Kind::MLP;
            const auto* mlp = genome.mlp();
            brain.input_count = mlp ? mlp->input_count() : 0;
            brain.output_count = mlp ? mlp->output_count() : 0;
            brain.update_interval = mlp ? SafeUpdateInterval(mlp->update_rate_hz()) : 0.2;
            break;
        }
        case evolution::genome::BrainKind::NEAT:
        default: {
            brain.kind = sim::BrainComponent::Kind::NEAT;
            const auto* neat = genome.neat();
            brain.input_count = neat ? neat->input_count() : 0;
            brain.output_count = neat ? neat->output_count() : 0;
            brain.update_interval = neat ? SafeUpdateInterval(neat->update_rate_hz()) : 0.2;
            break;
        }
    }
    brain.accumulator = 0.0;
    brain.storage_index = 0;
}

void ConfigureMetabolism(sim::MetabolismComponent& metabolism, const DerivedTraits& traits) {
    metabolism.max_energy = std::max(50.0, traits.mass * 140.0);
    metabolism.energy = metabolism.max_energy;
    metabolism.basal_rate = std::max(0.05, traits.basal_rate + traits.brain_cost);
}

void ConfigureFeedingIntent(sim::FeedingIntent& feeding, const DerivedTraits& traits) {
    feeding.request_eat = true;
    feeding.reach = std::clamp(1.2 + traits.mass * 0.15 + traits.trait_latent[0] * 0.2, 1.0, 3.0);
    feeding.rate = std::clamp(4.0 + traits.mass * 0.6 + traits.trait_latent[1], 2.0, 14.0);
}

void ConfigureReproduction(sim::ReproductionComponent& reproduction, const DerivedTraits& traits) {
    reproduction.cooldown = std::max(5.0, traits.mass * 2.0);
    reproduction.timer = reproduction.cooldown;
    reproduction.mate_radius = std::clamp(traits.mass * 1.5, 2.0, 8.0);
    reproduction.energy_threshold = std::max(120.0, traits.mass * 100.0);
    reproduction.density_sensitivity = std::clamp(0.9 + traits.trait_latent[6] * 0.4, 0.1, 2.0);
    reproduction.ideal_local_density =
        std::clamp(6.0 + traits.trait_latent[1] * 2.0 + traits.mass * 0.15, 2.0, 20.0);
    reproduction.density_query_radius =
        std::clamp(3.5 + traits.trait_latent[0] * 0.8 + traits.mass * 0.1, 2.0, 8.0);
    reproduction.critical_density_pressure = 3.0;
}

void ConfigureVision(sim::VisionComponent& vision, const DerivedTraits& traits) {
    const double latent0 = traits.trait_latent[0];
    const double latent1 = traits.trait_latent[1];
    const double latent6 = traits.trait_latent[6];
    const double latent7 = traits.trait_latent[7];

    vision.fov_degrees = std::clamp(120.0 + latent0 * 30.0, 70.0, 170.0);
    const double ray_real = 10.0 + (latent7 + 1.0) * 3.0;
    vision.num_rays = static_cast<std::uint32_t>(std::clamp(ray_real, 8.0, 16.0));
    vision.max_range = std::clamp(12.0 + latent6 * 4.0 + latent1 * 2.0, 8.0, 20.0);
    vision.eye_height_offset = std::clamp(0.25 + traits.mass * 0.03, 0.2, 1.2);
}

void ConfigureCombat(sim::CombatComponent& combat, const DerivedTraits& traits) {
    const double latent0 = traits.trait_latent[0];
    const double latent6 = traits.trait_latent[6];
    combat.attack_cooldown = std::clamp(0.9 - latent6 * 0.2, 0.2, 1.5);
    combat.attack_timer = 0.0;
    combat.attack_power = std::clamp(10.0 + traits.mass * 2.0 + latent6 * 4.0, 4.0, 40.0);
    combat.attack_reach = std::clamp(1.2 + latent0 * 0.5 + traits.mass * 0.1, 0.8, 2.5);
    combat.conversion_efficiency = std::clamp(0.6 + latent6 * 0.15, 0.35, 0.9);
}

void ConfigureHealth(sim::HealthComponent& health, const DerivedTraits& traits) {
    health.max_health = std::max(40.0, traits.mass * 80.0);
    health.health = health.max_health;
    health.regen_rate = 0.0;
}

}  // namespace

PhenotypeBuildResult PhenotypeBuilder::build(GenomeId id,
                                             entt::registry& registry,
                                             entt::entity entity,
                                             const GenomeStorage& storage) noexcept {
    PhenotypeBuildResult result{};

    const auto* genome = storage.get(id);
    if (genome == nullptr) {
        result.ok = false;
        result.msg = "Genome not found in storage";
        return result;
    }

    if (const auto* cache = genome->cached()) {
        result.traits = TraitsFromCache(cache);
    } else {
        result.traits = ComputeDerivedTraits(*genome);
    }

    auto& transform = registry.emplace_or_replace<sim::TransformComponent>(entity);
    transform.position = sim::Vec3{0.0, 0.0, 0.0};

    auto& kinematics = registry.emplace_or_replace<sim::KinematicsComponent>(entity);
    kinematics.linear_velocity = sim::Vec3{0.0, 0.0, 0.0};
    kinematics.accumulated_force = sim::Vec3{0.0, 0.0, 0.0};
    kinematics.inverse_mass = (result.traits.mass > 1e-6) ? 1.0 / result.traits.mass : 0.0;
    kinematics.linear_damping = 0.18;
    kinematics.restitution = 0.05;
    kinematics.friction = 0.6;

    auto& collider = registry.emplace_or_replace<sim::ColliderComponent>(entity);
    if (const auto* body = genome->body()) {
        ConfigureCollider(collider, *body);
    } else {
        collider.type = sim::ShapeType::Sphere;
        collider.sphere.radius = 0.5;
    }

    registry.emplace_or_replace<sim::RigidbodyComponent>(entity);

    auto& metabolism = registry.emplace_or_replace<sim::MetabolismComponent>(entity);
    ConfigureMetabolism(metabolism, result.traits);
    auto& feeding = registry.emplace_or_replace<sim::FeedingIntent>(entity);
    ConfigureFeedingIntent(feeding, result.traits);

    registry.emplace_or_replace<sim::GenomeHandleComponent>(entity, sim::GenomeHandleComponent{id});
    registry.emplace_or_replace<sim::ActuationComponent>(entity);

    auto& brain = registry.emplace_or_replace<sim::BrainComponent>(entity);
    ConfigureBrainComponent(brain, *genome);
    auto& vision = registry.emplace_or_replace<sim::VisionComponent>(entity);
    ConfigureVision(vision, result.traits);
    registry.emplace_or_replace<sim::VisionResult>(entity);
    registry.emplace_or_replace<sim::HeadingComponent>(entity);
    registry.emplace_or_replace<sim::ContactSenseComponent>(entity);

    auto& reproduction = registry.emplace_or_replace<sim::ReproductionComponent>(entity);
    ConfigureReproduction(reproduction, result.traits);
    auto& health = registry.emplace_or_replace<sim::HealthComponent>(entity);
    ConfigureHealth(health, result.traits);

    auto& lifecycle = registry.emplace_or_replace<sim::LifecycleComponent>(entity);
    lifecycle.age = 0.0;
    lifecycle.base_max_energy = metabolism.max_energy;
    lifecycle.energy_scale = 1.0;
    lifecycle.size_scale = 1.0;
    lifecycle.max_stage_age = 60.0;
    lifecycle.stage_index = 0;
    lifecycle.reproduction_allowed = true;
    lifecycle.gate_multipliers.clear();

    const auto* life_stages = genome->life_stages();
    if (life_stages != nullptr && life_stages->size() > 0) {
        if (const auto* stage0 = life_stages->Get(0)) {
            lifecycle.energy_scale = stage0->energy_scale();
            lifecycle.size_scale = stage0->size_scale();
            lifecycle.reproduction_allowed = stage0->reproduction_allowed();
            if (const auto* gates = stage0->gate_multipliers()) {
                lifecycle.gate_multipliers.reserve(gates->size());
                for (const float value : *gates) {
                    lifecycle.gate_multipliers.push_back(static_cast<double>(value));
                }
            }
        }
        if (const auto* last_stage = life_stages->Get(life_stages->size() - 1)) {
            lifecycle.max_stage_age =
                std::max(60.0, static_cast<double>(last_stage->age()) + 10.0);
        }
    }

    if (lifecycle.gate_multipliers.empty()) {
        lifecycle.gate_multipliers.assign(1, 1.0);
    }
    metabolism.max_energy = lifecycle.base_max_energy * lifecycle.energy_scale;
    metabolism.energy = metabolism.max_energy;
    if (!lifecycle.reproduction_allowed) {
        reproduction.timer = std::numeric_limits<double>::infinity();
    }

    auto& fitness = registry.emplace_or_replace<sim::FitnessComponent>(entity);
    fitness.age_seconds = 0.0;
    fitness.energy_int_accum = 0.0;
    fitness.offspring_count = 0;
    fitness.last_fitness = 0.0;

    const bool is_carnivore = ((id & 0x3ULL) == 0ULL);
    if (is_carnivore) {
        registry.emplace_or_replace<sim::CarnivoreTag>(entity);
        registry.remove<sim::HerbivoreTag>(entity);
        registry.emplace_or_replace<sim::DietComponent>(entity,
                                                        sim::DietComponent{sim::DietType::Carnivore});
        auto& combat = registry.emplace_or_replace<sim::CombatComponent>(entity);
        ConfigureCombat(combat, result.traits);
        registry.emplace_or_replace<sim::PursuitComponent>(entity);
    } else {
        registry.emplace_or_replace<sim::HerbivoreTag>(entity);
        registry.remove<sim::CarnivoreTag>(entity);
        registry.emplace_or_replace<sim::DietComponent>(entity,
                                                        sim::DietComponent{sim::DietType::Herbivore});
        registry.remove<sim::CombatComponent>(entity);
        registry.remove<sim::PursuitComponent>(entity);
    }

    const std::size_t required_inputs = 8 + 4 + 1 + 2 + (vision.num_rays * 3);
    brain.input_count = static_cast<std::uint32_t>(
        std::max<std::size_t>(static_cast<std::size_t>(brain.input_count), required_inputs));

    result.ok = true;
    result.msg = "ok";
    return result;
}

}  // namespace evolution::genetics
