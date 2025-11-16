#include "evolution/genetics/derived_traits.h"

#include <algorithm>
#include <cmath>
#include <span>

namespace evolution::genetics {

namespace {

constexpr double kPi = 3.141592653589793238462643383279502884;

[[nodiscard]] double ExtractVolume(const evolution::genome::Body& body) noexcept {
    const auto* size = body.size();
    if (size == nullptr) {
        return 1.0;
    }
    const double x = static_cast<double>(size->x());
    const double y = static_cast<double>(size->y());
    const double z = static_cast<double>(size->z());

    switch (body.shape()) {
        case evolution::genome::ShapeType::Sphere: {
            const double radius = std::max(0.05, x);
            return (4.0 / 3.0) * kPi * radius * radius * radius;
        }
        case evolution::genome::ShapeType::CapsuleY: {
            const double radius = std::max(0.05, x);
            const double half_height = std::max(0.05, y);
            const double cylinder_height = 2.0 * half_height;
            const double sphere_volume = (4.0 / 3.0) * kPi * radius * radius * radius;
            const double cylinder_volume = kPi * radius * radius * cylinder_height;
            return sphere_volume + cylinder_volume;
        }
        case evolution::genome::ShapeType::Box:
        default:
            return 8.0 * std::max(0.05, x) * std::max(0.05, y) * std::max(0.05, z);
    }
}

[[nodiscard]] double ExtractVolume(const evolution::genome::BodyT& body) noexcept {
    const auto* size = body.size.get();
    if (size == nullptr) {
        return 1.0;
    }
    const double x = static_cast<double>(size->x);
    const double y = static_cast<double>(size->y);
    const double z = static_cast<double>(size->z);

    switch (body.shape) {
        case evolution::genome::ShapeType::Sphere: {
            const double radius = std::max(0.05, x);
            return (4.0 / 3.0) * kPi * radius * radius * radius;
        }
        case evolution::genome::ShapeType::CapsuleY: {
            const double radius = std::max(0.05, x);
            const double half_height = std::max(0.05, y);
            const double cylinder_height = 2.0 * half_height;
            const double sphere_volume = (4.0 / 3.0) * kPi * radius * radius * radius;
            const double cylinder_volume = kPi * radius * radius * cylinder_height;
            return sphere_volume + cylinder_volume;
        }
        case evolution::genome::ShapeType::Box:
        default:
            return 8.0 * std::max(0.05, x) * std::max(0.05, y) * std::max(0.05, z);
    }
}

[[nodiscard]] double EstimateBrainCost(const evolution::genome::MLP* mlp) noexcept {
    if (mlp == nullptr) {
        return 0.0;
    }
    const auto* weights = mlp->weights();
    const auto* biases = mlp->biases();
    const double weight_count = weights ? static_cast<double>(weights->size()) : 0.0;
    const double bias_count = biases ? static_cast<double>(biases->size()) : 0.0;
    const double base_cost = weight_count * 0.0006 + bias_count * 0.0003;
    const double update_scale = std::max(mlp->update_rate_hz(), 1.0f) / 5.0;
    return base_cost * static_cast<double>(update_scale);
}

[[nodiscard]] double EstimateBrainCost(const evolution::genome::NEAT* neat) noexcept {
    if (neat == nullptr) {
        return 0.0;
    }
    const auto* conns = neat->conns();
    const double edge_count = conns ? static_cast<double>(conns->size()) : 0.0;
    const double recurrent_count = [&]() {
        if (conns == nullptr) {
            return 0.0;
        }
        double count = 0.0;
        for (const auto* conn : *conns) {
            if (conn != nullptr && conn->recurrent()) {
                count += 1.0;
            }
        }
        return count;
    }();
    const double base_cost = edge_count * 0.0008 + recurrent_count * 0.0005;
    const double update_scale = std::max(neat->update_rate_hz(), 1.0f) / 5.0;
    return base_cost * static_cast<double>(update_scale);
}

[[nodiscard]] double EstimateBrainCost(const evolution::genome::MLPT* mlp) noexcept {
    if (mlp == nullptr) {
        return 0.0;
    }
    const double weight_count = static_cast<double>(mlp->weights.size());
    const double bias_count = static_cast<double>(mlp->biases.size());
    const double base_cost = weight_count * 0.0006 + bias_count * 0.0003;
    const double update_scale = std::max(mlp->update_rate_hz, 1.0f) / 5.0;
    return base_cost * static_cast<double>(update_scale);
}

[[nodiscard]] double EstimateBrainCost(const evolution::genome::NEATT* neat) noexcept {
    if (!neat) {
        return 0.0;
    }
    const double edge_count = static_cast<double>(neat->conns.size());
    const double recurrent_count = [&]() {
        double count = 0.0;
        for (const auto& conn : neat->conns) {
            if (conn && conn->recurrent) {
                count += 1.0;
            }
        }
        return count;
    }();
    const double base_cost = edge_count * 0.0008 + recurrent_count * 0.0005;
    const double update_scale = std::max(neat->update_rate_hz, 1.0f) / 5.0;
    return base_cost * static_cast<double>(update_scale);
}

[[nodiscard]] double CountMlpParams(const evolution::genome::MLP* mlp) noexcept {
    if (mlp == nullptr) {
        return 0.0;
    }
    const double weight_count = mlp->weights() ? static_cast<double>(mlp->weights()->size()) : 0.0;
    const double bias_count = mlp->biases() ? static_cast<double>(mlp->biases()->size()) : 0.0;
    return weight_count + bias_count;
}

[[nodiscard]] double CountMlpParams(const evolution::genome::MLPT* mlp) noexcept {
    if (mlp == nullptr) {
        return 0.0;
    }
    return static_cast<double>(mlp->weights.size() + mlp->biases.size());
}

[[nodiscard]] double CountNeatParams(const evolution::genome::NEAT* neat) noexcept {
    if (neat == nullptr) {
        return 0.0;
    }
    const double weight_count = neat->conns() ? static_cast<double>(neat->conns()->size()) : 0.0;
    double bias_count = 0.0;
    if (const auto* nodes = neat->nodes()) {
        for (const auto* node : *nodes) {
            if (node != nullptr && node->type() != evolution::genome::NodeType::Input) {
                bias_count += 1.0;
            }
        }
    }
    return weight_count + bias_count;
}

[[nodiscard]] double CountNeatParams(const evolution::genome::NEATT* neat) noexcept {
    if (neat == nullptr) {
        return 0.0;
    }
    const double weight_count = static_cast<double>(neat->conns.size());
    double bias_count = 0.0;
    for (const auto& node : neat->nodes) {
        if (node && node->type != evolution::genome::NodeType::Input) {
            bias_count += 1.0;
        }
    }
    return weight_count + bias_count;
}

[[nodiscard]] double PlasticityParamCount(const evolution::genome::ModulePlasticity* plasticity) noexcept {
    if (plasticity == nullptr) {
        return 0.0;
    }
    const double bias_coeffs =
        plasticity->bias_coeffs() ? static_cast<double>(plasticity->bias_coeffs()->size()) : 0.0;
    const double scale_coeffs =
        plasticity->scale_coeffs() ? static_cast<double>(plasticity->scale_coeffs()->size()) : 0.0;
    // Always count trace gain/decay scalars when plasticity node is present.
    return bias_coeffs + scale_coeffs + 2.0;
}

[[nodiscard]] double PlasticityParamCount(const evolution::genome::ModulePlasticityT* plasticity) noexcept {
    if (plasticity == nullptr) {
        return 0.0;
    }
    const double bias_coeffs = static_cast<double>(plasticity->bias_coeffs.size());
    const double scale_coeffs = static_cast<double>(plasticity->scale_coeffs.size());
    return bias_coeffs + scale_coeffs + 2.0;
}

[[nodiscard]] double ModuleParamCount(const evolution::genome::BrainModule& module) noexcept {
    double total = 0.0;
    switch (module.kind()) {
        case evolution::genome::BrainKind::MLP:
            total += CountMlpParams(module.mlp());
            break;
        case evolution::genome::BrainKind::NEAT:
            total += CountNeatParams(module.neat());
            break;
        default:
            break;
    }
    total += PlasticityParamCount(module.plasticity());
    return total;
}

[[nodiscard]] double ModuleParamCount(const evolution::genome::BrainModuleT& module) noexcept {
    double total = 0.0;
    switch (module.kind) {
        case evolution::genome::BrainKind::MLP:
            total += CountMlpParams(module.mlp.get());
            break;
        case evolution::genome::BrainKind::NEAT:
            total += CountNeatParams(module.neat.get());
            break;
        default:
            break;
    }
    total += PlasticityParamCount(module.plasticity.get());
    return total;
}

[[nodiscard]] double ModuleBrainCost(const evolution::genome::BrainModule& module) noexcept {
    switch (module.kind()) {
        case evolution::genome::BrainKind::MLP:
            return EstimateBrainCost(module.mlp());
        case evolution::genome::BrainKind::NEAT:
            return EstimateBrainCost(module.neat());
        default:
            return 0.0;
    }
}

[[nodiscard]] double ModuleBrainCost(const evolution::genome::BrainModuleT& module) noexcept {
    switch (module.kind) {
        case evolution::genome::BrainKind::MLP:
            return EstimateBrainCost(module.mlp.get());
        case evolution::genome::BrainKind::NEAT:
            return EstimateBrainCost(module.neat.get());
        default:
            return 0.0;
    }
}

[[nodiscard]] std::array<double, 8> BuildTraitLatent(const evolution::genome::Body* body,
                                                    double brain_params,
                                                    std::size_t module_count) noexcept {
    std::array<double, 8> latent{};
    const auto normalise = [](double value, double min_v, double max_v) noexcept {
        const double clamped = std::clamp(value, min_v, max_v);
        const double t = (clamped - min_v) / (max_v - min_v);
        return std::clamp(t * 2.0 - 1.0, -1.0, 1.0);
    };
    if (body != nullptr) {
        double primary = 0.5;
        double secondary = 0.5;
        if (const auto* size = body->size()) {
            const double x = static_cast<double>(size->x());
            const double y = static_cast<double>(size->y());
            switch (body->shape()) {
                case evolution::genome::ShapeType::Sphere:
                    primary = std::max(0.1, x);
                    secondary = primary;
                    break;
                case evolution::genome::ShapeType::CapsuleY:
                    primary = std::max(0.1, x);
                    secondary = std::max(0.1, y);
                    break;
                case evolution::genome::ShapeType::Box:
                default:
                    primary = std::max(0.1, x);
                    secondary = std::max(0.1, y);
                    break;
            }
        }
        latent[0] = normalise(primary, 0.1, 4.0);
        latent[1] = normalise(secondary, 0.1, 5.0);
        latent[2] = normalise(static_cast<double>(body->mass_density()), 100.0, 1500.0);
        if (const auto* color = body->color()) {
            latent[3] = std::clamp(static_cast<double>(color->x()) * 2.0 - 1.0, -1.0, 1.0);
            latent[4] = std::clamp(static_cast<double>(color->y()) * 2.0 - 1.0, -1.0, 1.0);
            latent[5] = std::clamp(static_cast<double>(color->z()) * 2.0 - 1.0, -1.0, 1.0);
        }
    }
    latent[6] = std::tanh(std::log1p(std::max(0.0, brain_params)) * 0.35);
    latent[7] = std::tanh(static_cast<double>(std::max<std::size_t>(1, module_count)) * 0.3);
    return latent;
}

[[nodiscard]] std::array<double, 8> BuildTraitLatent(const evolution::genome::BodyT* body,
                                                    double brain_params,
                                                    std::size_t module_count) noexcept {
    std::array<double, 8> latent{};
    const auto normalise = [](double value, double min_v, double max_v) noexcept {
        const double clamped = std::clamp(value, min_v, max_v);
        const double t = (clamped - min_v) / (max_v - min_v);
        return std::clamp(t * 2.0 - 1.0, -1.0, 1.0);
    };
    if (body != nullptr) {
        double primary = 0.5;
        double secondary = 0.5;
        if (const auto* size = body->size.get()) {
            const double x = static_cast<double>(size->x);
            const double y = static_cast<double>(size->y);
            switch (body->shape) {
                case evolution::genome::ShapeType::Sphere:
                    primary = std::max(0.1, x);
                    secondary = primary;
                    break;
                case evolution::genome::ShapeType::CapsuleY:
                    primary = std::max(0.1, x);
                    secondary = std::max(0.1, y);
                    break;
                case evolution::genome::ShapeType::Box:
                default:
                    primary = std::max(0.1, x);
                    secondary = std::max(0.1, y);
                    break;
            }
        }
        latent[0] = normalise(primary, 0.1, 4.0);
        latent[1] = normalise(secondary, 0.1, 5.0);
        latent[2] = normalise(static_cast<double>(body->mass_density), 100.0, 1500.0);
        if (const auto* color = body->color.get()) {
            latent[3] = std::clamp(static_cast<double>(color->x) * 2.0 - 1.0, -1.0, 1.0);
            latent[4] = std::clamp(static_cast<double>(color->y) * 2.0 - 1.0, -1.0, 1.0);
            latent[5] = std::clamp(static_cast<double>(color->z) * 2.0 - 1.0, -1.0, 1.0);
        }
    }
    latent[6] = std::tanh(std::log1p(std::max(0.0, brain_params)) * 0.35);
    latent[7] = std::tanh(static_cast<double>(std::max<std::size_t>(1, module_count)) * 0.3);
    return latent;
}

[[nodiscard]] double ComputeMass(const evolution::genome::Body& body) noexcept {
    const double density = std::clamp(static_cast<double>(body.mass_density()), 50.0, 2000.0);
    const double volume = ExtractVolume(body);
    return std::max(0.5, density * volume * 0.001);
}

[[nodiscard]] double ComputeMass(const evolution::genome::BodyT& body) noexcept {
    const double density = std::clamp(static_cast<double>(body.mass_density), 50.0, 2000.0);
    const double volume = ExtractVolume(body);
    return std::max(0.5, density * volume * 0.001);
}

}  // namespace

DerivedTraits ComputeDerivedTraits(const evolution::genome::Genome& genome) noexcept {
    DerivedTraits traits{};

    if (const auto* body = genome.body()) {
        traits.mass = ComputeMass(*body);
    } else {
        traits.mass = 1.0;
    }

    const auto* modules_fb = genome.modules();
    const bool has_modules = modules_fb != nullptr && modules_fb->size() > 0;

    std::size_t module_count = 0;
    double module_params = 0.0;
    double module_cost = 0.0;

    if (has_modules) {
        module_count = modules_fb->size();
        for (const auto* module : *modules_fb) {
            if (module == nullptr) {
                continue;
            }
            module_params += ModuleParamCount(*module);
            module_cost += ModuleBrainCost(*module);
        }
    }

    if (!has_modules) {
        module_count = 1;
        switch (genome.brain_kind()) {
            case evolution::genome::BrainKind::MLP:
                module_params += CountMlpParams(genome.mlp());
                module_cost += EstimateBrainCost(genome.mlp());
                break;
            case evolution::genome::BrainKind::NEAT:
                module_params += CountNeatParams(genome.neat());
                module_cost += EstimateBrainCost(genome.neat());
                break;
            default:
                break;
        }
    }

    double gating_params = 0.0;
    double gating_cost = 0.0;
    if (const auto* gating = genome.gating()) {
        gating_params += CountMlpParams(gating->mlp());
        gating_cost += EstimateBrainCost(gating->mlp()) * 0.25;
    }

    double preference_params = 0.0;
    double preference_cost = 0.0;
    if (const auto* preference = genome.preference()) {
        preference_params += CountMlpParams(preference->mlp());
        preference_cost += EstimateBrainCost(preference->mlp()) * 0.1;
    }

    traits.module_count = std::max<std::size_t>(1, module_count);
    traits.brain_params = module_params + gating_params + preference_params;
    traits.brain_cost = module_cost + gating_cost + preference_cost;
    traits.basal_rate =
        std::max(0.1, 0.025 * std::pow(traits.mass, 0.75) + traits.brain_cost * 0.5);
    traits.trait_latent = BuildTraitLatent(genome.body(), traits.brain_params, traits.module_count);
    return traits;
}

DerivedTraits ComputeDerivedTraits(const evolution::genome::GenomeT& genome) noexcept {
    DerivedTraits traits{};
    if (const auto* body = genome.body.get()) {
        traits.mass = ComputeMass(*body);
    } else {
        traits.mass = 1.0;
    }

    const bool has_modules = !genome.modules.empty();
    std::size_t module_count = 0;
    double module_params = 0.0;
    double module_cost = 0.0;

    if (has_modules) {
        module_count = genome.modules.size();
        for (const auto& module_ptr : genome.modules) {
            if (!module_ptr) {
                continue;
            }
            module_params += ModuleParamCount(*module_ptr);
            module_cost += ModuleBrainCost(*module_ptr);
        }
    }

    if (!has_modules) {
        module_count = 1;
        switch (genome.brain_kind) {
            case evolution::genome::BrainKind::MLP:
                module_params += CountMlpParams(genome.mlp.get());
                module_cost += EstimateBrainCost(genome.mlp.get());
                break;
            case evolution::genome::BrainKind::NEAT:
                module_params += CountNeatParams(genome.neat.get());
                module_cost += EstimateBrainCost(genome.neat.get());
                break;
            default:
                break;
        }
    }

    double gating_params = 0.0;
    double gating_cost = 0.0;
    if (genome.gating && genome.gating->mlp) {
        gating_params += CountMlpParams(genome.gating->mlp.get());
        gating_cost += EstimateBrainCost(genome.gating->mlp.get()) * 0.25;
    }

    double preference_params = 0.0;
    double preference_cost = 0.0;
    if (genome.preference && genome.preference->mlp) {
        preference_params += CountMlpParams(genome.preference->mlp.get());
        preference_cost += EstimateBrainCost(genome.preference->mlp.get()) * 0.1;
    }

    traits.module_count = std::max<std::size_t>(1, module_count);
    traits.brain_params = module_params + gating_params + preference_params;
    traits.brain_cost = module_cost + gating_cost + preference_cost;
    traits.basal_rate =
        std::max(0.1, 0.025 * std::pow(traits.mass, 0.75) + traits.brain_cost * 0.5);
    traits.trait_latent =
        BuildTraitLatent(genome.body.get(), traits.brain_params, traits.module_count);
    return traits;
}

}  // namespace evolution::genetics

