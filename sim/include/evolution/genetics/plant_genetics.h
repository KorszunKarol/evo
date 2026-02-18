#pragma once

#include <cstdint>

#include "evolution/genetics/genome_storage.h"
#include "evolution/genetics/rng.h"
#include "genome_generated.h"

namespace evolution::genetics {

[[nodiscard]] std::unique_ptr<::evolution::genome::PlantTraitsT> extract_plant_traits(
    const evolution::genome::GenomeT* genome) noexcept;

[[nodiscard]] evolution::genome::GenomeT create_base_plant_genome(
    std::uint64_t seed,
    std::uint8_t species_type = 0);

}  // namespace evolution::genetics
