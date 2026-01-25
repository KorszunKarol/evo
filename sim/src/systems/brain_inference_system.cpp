#include "evolution/sim/brain_inference_system.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

#include <spdlog/spdlog.h>

#include "evolution/sim/environment/environment.h"
#include "evolution/sim/brain_io_layout.h"

namespace evolution::sim {

namespace {

constexpr double kImpulseClamp = 1.0;
constexpr std::size_t kContextFeatureCount = 6;

[[nodiscard]] double SafeDivide(double numerator, double denominator, double default_value) noexcept {
    if (std::abs(denominator) < 1e-9) {
        return default_value;
    }
    return numerator / denominator;
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
    : storage_(storage),
      input_buffer_{},
      gating_buffer_{},
      context_buffer_{},
      module_buffer_{},
      output_buffer_{} {
    constexpr std::size_t kMaxInputCapacity = 256;
    constexpr std::size_t kMaxModuleCapacity = 64;
    constexpr std::size_t kMaxContextCapacity = 128;
    constexpr std::size_t kMaxModuleOutputCapacity = 256;
    // Reserve capacity upfront to reduce per-entity allocations in tick loop
    input_buffer_.reserve(kMaxInputCapacity);
    gating_buffer_.reserve(kMaxModuleCapacity);
    context_buffer_.reserve(kMaxContextCapacity);
    module_buffer_.reserve(kMaxModuleOutputCapacity);
    output_buffer_.reserve(kMaxModuleOutputCapacity);
}

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

        brain.accumulator += dt;
        if (brain.update_interval <= 0.0) {
            brain.update_interval = dt;
        }
        if (brain.accumulator + 1e-9 < brain.update_interval) {
            continue;
        }

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
        lifecycle.age += dt;
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

        const double age_frac =
            lifecycle.max_stage_age > 0.0
                ? std::clamp(lifecycle.age / lifecycle.max_stage_age, 0.0, 1.0)
                : 0.0;

        const double energy_frac =
            std::clamp(SafeDivide(metabolism.energy, std::max(1e-6, metabolism.max_energy), 0.0),
                       0.0,
                       1.0);
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

        const std::size_t sensor_count = static_cast<std::size_t>(brain.input_count);
        input_buffer_.resize(sensor_count);
        // Zero out only the portion that's actually used (reserve capacity is fixed, so zero unused indices)
        std::fill(input_buffer_.begin(), input_buffer_.end(), 0.0);
        if (!input_buffer_.empty()) {
            input_buffer_[0] = context_features[0];
        }
        if (input_buffer_.size() > 1) {
            input_buffer_[1] = context_features[0];
        }
        if (input_buffer_.size() > 2) {
            input_buffer_[2] = std::clamp(kinematics.linear_velocity.y, -25.0, 25.0) / 25.0;
        }
        if (input_buffer_.size() > 3) {
            input_buffer_[3] = context_features[5];
        }
        if (input_buffer_.size() > 4) {
            input_buffer_[4] = context_features[2];
        }
        if (input_buffer_.size() > 5) {
            input_buffer_[5] = context_features[3];
        }
        if (input_buffer_.size() > 6) {
            input_buffer_[6] = context_features[4];
        }
        if (input_buffer_.size() > 7) {
            input_buffer_[7] = lifecycle.energy_scale;
        }
        // Clear remaining inputs to zero (preserves capacity, no reallocation)
        if (sensor_count > 7) {
            std::fill(input_buffer_.begin() + 7, input_buffer_.end(), 0.0);
        }
        if (input_buffer_.size() > 2) {
            input_buffer_[2] = context_features[5];
        }
        if (input_buffer_.size() > 3) {
            input_buffer_[3] = context_features[2];
        }
        if (input_buffer_.size() > 4) {
            input_buffer_[4] = context_features[3];
        }
        if (input_buffer_.size() > 5) {
            input_buffer_[5] = context_features[4];
        }
        if (input_buffer_.size() > 6) {
            input_buffer_[6] = context_features[1];
        }
        if (input_buffer_.size() > 7) {
            input_buffer_[7] = lifecycle.energy_scale;
        }

        // Append vision sensor inputs if entity has VisionComponent.
        const VisionComponent* vision = registry.try_get<VisionComponent>(entity);
        if (vision != nullptr && vision->enabled) {
            const std::size_t vision_start = kBaseSensorCount;
            const std::size_t ray_capacity = kVisionRayCapacity;
            const std::size_t ray_count = std::min(vision->ray_distances.size(), ray_capacity);

            // Ray distances (fixed-capacity block)
            for (std::size_t r = 0; r < ray_count && (vision_start + r) < sensor_count; ++r) {
                input_buffer_[vision_start + r] = static_cast<double>(vision->ray_distances[r]);
            }

            // Ray hit types encoded as: 0=none, 0.25=plant, 0.5=herbivore, 0.75=carnivore, 1=terrain
            const std::size_t hit_type_start = vision_start + ray_capacity;
            for (std::size_t r = 0; r < ray_count && (hit_type_start + r) < sensor_count; ++r) {
                double type_encoding = 0.0;
                switch (vision->ray_hit_types[r]) {
                    case VisionHitType::None: type_encoding = 0.0; break;
                    case VisionHitType::Plant: type_encoding = 0.25; break;
                    case VisionHitType::Herbivore: type_encoding = 0.5; break;
                    case VisionHitType::Carnivore: type_encoding = 0.75; break;
                    case VisionHitType::Terrain: type_encoding = 1.0; break;
                    case VisionHitType::Unknown: type_encoding = 0.0; break;
                }
                input_buffer_[hit_type_start + r] = type_encoding;
            }
        }

        const SocialSignalsComponent* social = registry.try_get<SocialSignalsComponent>(entity);
        if (social != nullptr) {
            const std::size_t social_start = kBaseSensorCount + 2 * kVisionRayCapacity;
            auto set_input = [&](std::size_t index, double value) {
                if (index < sensor_count) {
                    input_buffer_[index] = value;
                }
            };

            set_input(social_start + 0, std::clamp(social->cohesion_dir.x, -1.0, 1.0));
            set_input(social_start + 1, std::clamp(social->cohesion_dir.z, -1.0, 1.0));
            set_input(social_start + 2, std::clamp(social->alignment_dir.x, -1.0, 1.0));
            set_input(social_start + 3, std::clamp(social->alignment_dir.z, -1.0, 1.0));
            set_input(social_start + 4, std::clamp(social->separation_dir.x, -1.0, 1.0));
            set_input(social_start + 5, std::clamp(social->separation_dir.z, -1.0, 1.0));
            set_input(social_start + 6, std::clamp(social->neighbor_density, 0.0, 1.0));
            set_input(social_start + 7, std::clamp(social->territory_dist_norm, 0.0, 1.0));
            set_input(social_start + 8, std::clamp(social->intruder_density, 0.0, 1.0));
            set_input(social_start + 9, std::clamp(social->prey_dir.x, -1.0, 1.0));
            set_input(social_start + 10, std::clamp(social->prey_dir.z, -1.0, 1.0));
            set_input(social_start + 11, std::clamp(social->pack_density_near_prey, 0.0, 1.0));
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
            impulse_x = std::clamp(outputs[0], -kImpulseClamp, kImpulseClamp);
        }
        if (outputs.size() > 1) {
            impulse_z = std::clamp(outputs[1], -kImpulseClamp, kImpulseClamp);
        }
        if (outputs.size() > 2) {
            jump = outputs[2] > 0.5;
        }
        if (outputs.size() > 3) {
            eat = outputs[3] > 0.5;
        }
        if (outputs.size() > 4) {
            attack = outputs[4] > 0.0; // TEMPORARY: 0.0 threshold for easier initial predation
        }

        actuation.impulse_x = impulse_x;
        actuation.impulse_z = impulse_z;
        actuation.jump = jump;
        actuation.eat = eat;
        actuation.attack = attack;
        actuation.update_skip = 0;

        brain.accumulator = std::fmod(brain.accumulator, brain.update_interval);

        // Phase 4b: Neural Probe (Brain Inspection)
        if (auto* inspect = registry.try_get<BrainInspectComponent>(entity)) {
            inspect->input_snapshot = input_buffer_;
            inspect->output_snapshot = outputs.empty() ? std::vector<double>{} : 
                std::vector<double>(outputs.begin(), outputs.end());
            inspect->internal_state = module_buffer_;
            inspect->gating_snapshot = gating_buffer_;
            inspect->action_mask = static_cast<std::uint8_t>(
                (eat ? 1 : 0) | (jump ? 2 : 0) | (attack ? 4 : 0));
        }
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
