#include "evolution/sim/brain_inference_system.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <utility>

#include <spdlog/spdlog.h>

#include "evolution/sim/environment/environment.h"

namespace evolution::sim {

namespace {

constexpr double kImpulseClamp = 1.0;
constexpr std::size_t kContextFeatureCount = 6;
constexpr std::size_t kCoreProprioFeatureCount = 8;
constexpr std::size_t kContactFeatureCount = 4;
constexpr std::size_t kThreatFeatureCount = 1;
constexpr std::size_t kPursuitFeatureCount = 2;

[[nodiscard]] double SafeDivide(double numerator, double denominator, double default_value) noexcept {
    if (std::abs(denominator) < 1e-9) {
        return default_value;
    }
    return numerator / denominator;
}

[[nodiscard]] double SafeFinite(double value, double fallback = 0.0) noexcept {
    return std::isfinite(value) ? value : fallback;
}

[[nodiscard]] double ComputeSlopeMagnitude(const TransformComponent& transform,
                                           const Terrain* terrain) noexcept {
    if (terrain == nullptr) {
        return 0.0;
    }
    const Vec3 normal = terrain->normal(transform.position.x, transform.position.z);
    const double vertical = std::clamp(normal.y, -1.0, 1.0);
    return std::clamp(std::sqrt(std::max(0.0, 1.0 - vertical * vertical)), 0.0, 1.0);
}

[[nodiscard]] double SampleSoilNutrient(const TransformComponent& transform,
                                        const SoilGrid* soil) noexcept {
    if (soil == nullptr) {
        return 0.0;
    }
    const float raw = soil->sample(transform.position.x, transform.position.z);
    const float max_value = std::max(soil->config().max_nutrient, 1.0F);
    return std::clamp(static_cast<double>(raw) / static_cast<double>(max_value), 0.0, 1.0);
}

[[nodiscard]] bool IsOnGround(const TransformComponent& transform,
                              const Terrain* terrain) noexcept {
    const double ground_y = terrain ? terrain->height(transform.position.x, transform.position.z)
                                    : 0.0;
    return transform.position.y - ground_y <= 0.2;
}

[[nodiscard]] double SpeedMagnitudeNormalized(const KinematicsComponent& kinematics) noexcept {
    const double vx = kinematics.linear_velocity.x;
    const double vz = kinematics.linear_velocity.z;
    const double speed = std::sqrt(vx * vx + vz * vz);
    return std::clamp(speed / 12.0, 0.0, 1.0);
}

void EnsureGateMultipliers(LifecycleComponent& lifecycle, std::size_t module_count) noexcept {
    if (lifecycle.gate_multipliers.size() != module_count) {
        lifecycle.gate_multipliers.assign(module_count, 1.0);
    }
}

void ApplyLifeStage(const evolution::genome::LifeStage& stage,
                    LifecycleComponent& lifecycle,
                    MetabolismComponent& metabolism,
                    std::size_t module_count) {
    lifecycle.energy_scale = stage.energy_scale();
    lifecycle.size_scale = stage.size_scale();
    lifecycle.reproduction_allowed = stage.reproduction_allowed();
    lifecycle.gate_multipliers.assign(module_count, 1.0);
    if (const auto* gates = stage.gate_multipliers()) {
        const std::size_t count = std::min<std::size_t>(gates->size(), module_count);
        for (std::size_t i = 0; i < count; ++i) {
            lifecycle.gate_multipliers[i] = std::clamp(static_cast<double>((*gates)[i]), 0.0, 2.0);
        }
    }
    if (lifecycle.base_max_energy > 0.0) {
        const double ratio =
            SafeDivide(metabolism.energy, std::max(1e-6, metabolism.max_energy), 1.0);
        metabolism.max_energy = std::max(1.0, lifecycle.base_max_energy * lifecycle.energy_scale);
        metabolism.energy = std::clamp(ratio * metabolism.max_energy, 0.0, metabolism.max_energy);
    }
}

[[nodiscard]] std::pair<double, double> ComputePlasticityAdjust(
    const evolution::genome::ModulePlasticity* plasticity,
    const std::array<double, kContextFeatureCount>& features) noexcept {
    if (plasticity == nullptr) {
        return {0.0, 1.0};
    }
    double bias_adjust = 0.0;
    if (const auto* bias = plasticity->bias_coeffs()) {
        const std::size_t count = std::min<std::size_t>(bias->size(), features.size());
        for (std::size_t i = 0; i < count; ++i) {
            bias_adjust += static_cast<double>((*bias)[i]) * features[i];
        }
    }
    double scale_delta = 0.0;
    if (const auto* scale = plasticity->scale_coeffs()) {
        const std::size_t count = std::min<std::size_t>(scale->size(), features.size());
        for (std::size_t i = 0; i < count; ++i) {
            scale_delta += static_cast<double>((*scale)[i]) * features[i];
        }
    }
    const double scale = std::clamp(1.0 + scale_delta, 0.1, 3.0);
    return {bias_adjust, scale};
}

}  // namespace

BrainInferenceSystem::BrainInferenceSystem(genetics::GenomeStorage& storage) noexcept
    : storage_(storage) {}

void BrainInferenceSystem::tick(SimulationContext& context) {
    auto& registry = context.registry();
    auto view = registry.view<BrainComponent,
                              ActuationComponent,
                              GenomeHandleComponent,
                              MetabolismComponent,
                              KinematicsComponent,
                              TransformComponent,
                              LifecycleComponent>();

    const Terrain* terrain = registry.ctx().find<Terrain>();
    const SoilGrid* soil = registry.ctx().find<SoilGrid>();
    const auto* plant_index = registry.ctx().find<PlantSpatialIndex>();
    const double global_density =
        std::clamp(static_cast<double>(registry.alive()) / 250.0, 0.0, 1.0);
    const double dt = context.fixed_dt();

    for (auto entity : view) {
        auto& brain = view.get<BrainComponent>(entity);
        auto& actuation = view.get<ActuationComponent>(entity);
        const auto& handle = view.get<GenomeHandleComponent>(entity);
        auto& metabolism = view.get<MetabolismComponent>(entity);
        const auto& kinematics = view.get<KinematicsComponent>(entity);
        const auto& transform = view.get<TransformComponent>(entity);
        auto& lifecycle = view.get<LifecycleComponent>(entity);
        auto* reproduction = registry.try_get<ReproductionComponent>(entity);

        brain.accumulator += dt;
        if (brain.update_interval <= 0.0) {
            brain.update_interval = dt;
        }
        if (brain.accumulator + 1e-9 < brain.update_interval) {
            continue;
        }
        const double brain_step_dt = std::max(dt, brain.accumulator);

        const auto* genome = storage_.get(handle.id);
        if (genome == nullptr) {
            spdlog::warn("BrainInferenceSystem: missing genome id {}", handle.id);
            continue;
        }

        const auto* modules_fb = genome->modules();
        const std::size_t module_count =
            (modules_fb != nullptr && modules_fb->size() > 0)
                ? modules_fb->size()
                : 1;

        // Update lifecycle age and stage selection.
        lifecycle.age += brain_step_dt;
        const auto* life_stages = genome->life_stages();
        if (life_stages != nullptr && life_stages->size() > 0) {
            std::uint32_t desired_index = lifecycle.stage_index;
            for (std::uint32_t idx = 0; idx < life_stages->size(); ++idx) {
                const auto* stage = life_stages->Get(idx);
                if (stage != nullptr &&
                    lifecycle.age + 1e-6 >= static_cast<double>(stage->age())) {
                    desired_index = idx;
                }
            }
            if (desired_index != lifecycle.stage_index || lifecycle.gate_multipliers.empty()) {
                lifecycle.stage_index = desired_index;
                const auto* stage = life_stages->Get(desired_index);
                if (stage != nullptr) {
                    ApplyLifeStage(*stage, lifecycle, metabolism, module_count);
                }
            }
        } else {
            EnsureGateMultipliers(lifecycle, module_count);
        }

        if (reproduction != nullptr) {
            if (!lifecycle.reproduction_allowed) {
                reproduction->timer = std::numeric_limits<double>::infinity();
            } else if (!std::isfinite(reproduction->timer)) {
                reproduction->timer = std::max(0.0, reproduction->cooldown);
            }
        }

        const double age_frac =
            lifecycle.max_stage_age > 0.0
                ? std::clamp(lifecycle.age / lifecycle.max_stage_age, 0.0, 1.0)
                : 0.0;

        const double energy_frac =
            std::clamp(SafeDivide(metabolism.energy, std::max(1e-6, metabolism.max_energy), 0.0),
                       0.0,
                       1.0);
        const double hunger_signal = 1.0 - energy_frac;
        const double speed_norm = SpeedMagnitudeNormalized(kinematics);
        const double slope = ComputeSlopeMagnitude(transform, terrain);
        const double soil_factor = SampleSoilNutrient(transform, soil);
        const double density = global_density;
        const bool on_ground = IsOnGround(transform, terrain);

        std::array<double, kContextFeatureCount> context_features{
            energy_frac,
            age_frac,
            slope,
            soil_factor,
            density,
            on_ground ? 1.0 : 0.0,
        };

        const auto* vision = registry.try_get<VisionResult>(entity);
        const std::size_t vision_feature_count = vision ? vision->buffer.size() : 0;

        const auto* contact = registry.try_get<ContactSenseComponent>(entity);
        const double contact_count_norm =
            contact ? std::clamp(static_cast<double>(contact->contact_count) / 6.0, 0.0, 1.0) : 0.0;
        const Vec3 contact_normal = contact ? contact->contact_normal_sum : Vec3{0.0, 0.0, 0.0};
        const double contact_force_norm =
            contact ? std::clamp(contact->contact_force_magnitude / 200.0, 0.0, 1.0) : 0.0;

        const auto* threat = registry.try_get<ThreatComponent>(entity);
        const double threat_signal = threat ? 1.0 : 0.0;

        double pursuit_distance_norm = 0.0;
        double pursuit_bearing = 0.0;
        if (const auto* pursuit = registry.try_get<PursuitComponent>(entity);
            pursuit != nullptr && pursuit->target_entity != entt::null &&
            registry.valid(pursuit->target_entity)) {
            if (const auto* target_transform = registry.try_get<TransformComponent>(pursuit->target_entity)) {
                const Vec3 delta = target_transform->position - transform.position;
                const double planar_dist = std::sqrt(delta.x * delta.x + delta.z * delta.z);
                const double max_dist = std::max(0.1, pursuit->engage_distance);
                pursuit_distance_norm = std::clamp(planar_dist / max_dist, 0.0, 1.0);
                pursuit_bearing = std::clamp(std::atan2(delta.z, delta.x) / 3.14159265358979323846,
                                             -1.0,
                                             1.0);
            }
        }

        const std::size_t required_sensor_count = kCoreProprioFeatureCount +
                                                  kContactFeatureCount +
                                                  kThreatFeatureCount +
                                                  kPursuitFeatureCount +
                                                  vision_feature_count;
        const std::size_t sensor_count = std::max<std::size_t>(static_cast<std::size_t>(brain.input_count),
                                                                required_sensor_count);
        input_buffer_.assign(sensor_count, 0.0);

        std::size_t cursor = 0;
        auto write_feature = [&](double value) {
            if (cursor < input_buffer_.size()) {
                input_buffer_[cursor] = value;
            }
            ++cursor;
        };

        write_feature(energy_frac);
        write_feature(hunger_signal);
        write_feature(age_frac);
        write_feature(speed_norm);
        write_feature(slope);
        write_feature(soil_factor);
        write_feature(density);
        write_feature(on_ground ? 1.0 : 0.0);

        write_feature(contact_count_norm);
        write_feature(std::clamp(contact_normal.x, -1.0, 1.0));
        write_feature(std::clamp(contact_normal.z, -1.0, 1.0));
        write_feature(contact_force_norm);

        write_feature(threat_signal);

        write_feature(pursuit_distance_norm);
        write_feature(pursuit_bearing);

        if (vision != nullptr) {
            for (const double value : vision->buffer) {
                write_feature(value);
            }
        }

        const std::size_t output_count = static_cast<std::size_t>(brain.output_count);
        auto outputs = ensure_output_buffer(output_count);
        std::fill(outputs.begin(), outputs.end(), 0.0);

        // Evaluate gating network if available.
        gating_buffer_.assign(module_count, 1.0);
        double gate_epsilon = 0.0;
        if (const auto* gating = genome->gating()) {
            gate_epsilon = gating->epsilon();
            const auto* gating_mlp = gating->mlp();
            const std::uint32_t context_size = gating->context_size();
            if (gating_mlp != nullptr && context_size > 0) {
                context_buffer_.assign(context_size, 0.0);
                for (std::size_t i = 0; i < context_buffer_.size() && i < context_features.size();
                     ++i) {
                    context_buffer_[i] = context_features[i];
                }
                gating_buffer_.assign(gating_mlp->output_count(), 0.0);
                genetics::BrainMlp::Evaluate(*gating_mlp,
                                             std::span<const double>(context_buffer_),
                                             std::span<double>(gating_buffer_));
                if (gating_buffer_.size() != module_count) {
                    const std::size_t previous = gating_buffer_.size();
                    gating_buffer_.resize(module_count, 0.0);
                    for (std::size_t i = previous; i < gating_buffer_.size(); ++i) {
                        gating_buffer_[i] = (i < lifecycle.gate_multipliers.size())
                                                ? lifecycle.gate_multipliers[i]
                                                : 1.0;
                    }
                }
                for (std::size_t i = 0; i < gating_buffer_.size(); ++i) {
                    double gate = std::clamp((gating_buffer_[i] + 1.0) * 0.5, 0.0, 1.0);
                    const double stage_multiplier =
                        (i < lifecycle.gate_multipliers.size())
                            ? lifecycle.gate_multipliers[i]
                            : 1.0;
                    gating_buffer_[i] = std::clamp(gate * stage_multiplier, 0.0, 1.0);
                }
            } else {
                for (std::size_t i = 0; i < gating_buffer_.size(); ++i) {
                    const double stage_multiplier =
                        (i < lifecycle.gate_multipliers.size())
                            ? lifecycle.gate_multipliers[i]
                            : 1.0;
                    gating_buffer_[i] = std::clamp(stage_multiplier, 0.0, 1.0);
                }
            }
        } else {
            EnsureGateMultipliers(lifecycle, module_count);
            for (std::size_t i = 0; i < gating_buffer_.size(); ++i) {
                gating_buffer_[i] =
                    (i < lifecycle.gate_multipliers.size()) ? lifecycle.gate_multipliers[i] : 1.0;
            }
        }

        module_buffer_.assign(output_count, 0.0);

        if (modules_fb != nullptr && modules_fb->size() > 0) {
            for (std::size_t idx = 0; idx < modules_fb->size(); ++idx) {
                const auto* module = modules_fb->Get(idx);
                if (module == nullptr) {
                    continue;
                }
                const double gate_value =
                    (idx < gating_buffer_.size()) ? gating_buffer_[idx] : 1.0;
                if (gate_value <= gate_epsilon) {
                    continue;
                }
                std::size_t module_output_size = output_count;
                bool evaluated = false;
                switch (module->kind()) {
                    case evolution::genome::BrainKind::MLP: {
                        if (const auto* mlp = module->mlp()) {
                            module_output_size = std::max<std::size_t>(1, mlp->output_count());
                            module_buffer_.assign(module_output_size, 0.0);
                            genetics::BrainMlp::Evaluate(*mlp,
                                                         std::span<const double>(input_buffer_),
                                                         std::span<double>(module_buffer_));
                            evaluated = true;
                        }
                        break;
                    }
                    case evolution::genome::BrainKind::NEAT:
                    default: {
                        if (const auto* neat = module->neat()) {
                            module_output_size = std::max<std::size_t>(1, neat->output_count());
                            module_buffer_.assign(module_output_size, 0.0);
                            auto& runtime =
                                fetch_neat_runtime(handle.id, static_cast<std::uint32_t>(idx), *neat);
                            runtime.evaluate(std::span<const double>(input_buffer_),
                                             std::span<double>(module_buffer_));
                            evaluated = true;
                        }
                        break;
                    }
                }
                if (!evaluated) {
                    continue;
                }
                const auto plasticity_adjust =
                    ComputePlasticityAdjust(module->plasticity(), context_features);
                const std::size_t contribution_count =
                    std::min(output_count, module_buffer_.size());
                for (std::size_t out = 0; out < contribution_count; ++out) {
                    const double adjusted = (module_buffer_[out] + plasticity_adjust.first) *
                                            plasticity_adjust.second;
                    outputs[out] += gate_value * adjusted;
                }
            }
        } else {
            switch (brain.kind) {
                case BrainComponent::Kind::MLP: {
                    const auto* mlp = genome->mlp();
                    if (mlp != nullptr) {
                        genetics::BrainMlp::Evaluate(*mlp,
                                                     std::span<const double>(input_buffer_),
                                                     outputs);
                    }
                    break;
                }
                case BrainComponent::Kind::NEAT:
                default: {
                    const auto* neat = genome->neat();
                    if (neat != nullptr) {
                        auto& runtime = fetch_neat_runtime(handle.id, 0, *neat);
                        runtime.evaluate(std::span<const double>(input_buffer_), outputs);
                    }
                    break;
                }
            }
        }

        double impulse_x = 0.0;
        double impulse_z = 0.0;
        bool jump = false;
        bool eat = false;
        bool attack = false;

        if (!outputs.empty()) {
            impulse_x = std::clamp(SafeFinite(outputs[0]), -kImpulseClamp, kImpulseClamp);
        }
        if (outputs.size() > 1) {
            impulse_z = std::clamp(SafeFinite(outputs[1]), -kImpulseClamp, kImpulseClamp);
        }
        if (outputs.size() > 2) {
            jump = SafeFinite(outputs[2]) > 0.5;
        }
        if (outputs.size() > 3) {
            eat = SafeFinite(outputs[3]) > 0.5;
        }
        if (outputs.size() > 4) {
            attack = SafeFinite(outputs[4]) > 0.5;
        }

        if (hunger_signal > 0.15 && plant_index != nullptr) {
            const auto* diet = registry.try_get<DietComponent>(entity);
            if (diet != nullptr && diet->type == DietType::Herbivore) {
                entt::entity nearest_plant = entt::null;
                double nearest_dist_sq = std::numeric_limits<double>::max();
                constexpr double kForageRadius = 512.0;
                plant_index->for_each_in_radius(
                    registry,
                    transform.position,
                    kForageRadius,
                    [&](entt::entity plant_entity, double dist_sq) {
                        if (dist_sq < nearest_dist_sq) {
                            nearest_dist_sq = dist_sq;
                            nearest_plant = plant_entity;
                        }
                    });

                if (nearest_plant != entt::null) {
                    const auto* plant_transform = registry.try_get<TransformComponent>(nearest_plant);
                    if (plant_transform != nullptr) {
                        const Vec3 delta = plant_transform->position - transform.position;
                        const double len_sq = delta.x * delta.x + delta.z * delta.z;
                        if (len_sq > 1e-9) {
                            const double inv_len = 1.0 / std::sqrt(len_sq);
                            const double target_x = delta.x * inv_len;
                            const double target_z = delta.z * inv_len;
                            constexpr double kSteerBlend = 0.7;
                            impulse_x = std::clamp(impulse_x * (1.0 - kSteerBlend) + target_x * kSteerBlend,
                                                   -kImpulseClamp,
                                                   kImpulseClamp);
                            impulse_z = std::clamp(impulse_z * (1.0 - kSteerBlend) + target_z * kSteerBlend,
                                                   -kImpulseClamp,
                                                   kImpulseClamp);
                            eat = true;
                        }
                    }
                }
            }
        }

        actuation.impulse_x = impulse_x;
        actuation.impulse_z = impulse_z;
        actuation.jump = jump;
        actuation.eat = eat;
        actuation.attack = attack;
        actuation.update_skip = 0;

        brain.accumulator = std::fmod(brain.accumulator, brain.update_interval);
    }
}

std::span<double> BrainInferenceSystem::ensure_output_buffer(std::size_t count) {
    output_buffer_.resize(count);
    return std::span<double>(output_buffer_);
}

genetics::BrainNeat& BrainInferenceSystem::fetch_neat_runtime(genetics::GenomeId id,
                                                              std::uint32_t module_index,
                                                              const evolution::genome::NEAT& neat) {
    for (auto& slot : neat_slots_) {
        if (slot.genome_id == id && slot.module_index == module_index) {
            return slot.runtime;
        }
    }
    NeatSlot slot{.genome_id = id, .module_index = module_index, .runtime = genetics::BrainNeat(neat)};
    slot.runtime.reset_state();
    neat_slots_.push_back(std::move(slot));
    return neat_slots_.back().runtime;
}

}  // namespace evolution::sim
