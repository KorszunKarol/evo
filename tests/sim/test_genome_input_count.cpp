#include <gtest/gtest.h>

#include "evolution/genetics/genome_storage.h"
#include "evolution/sim/brain_io_layout.h"

namespace evolution::sim {

TEST(GenomeStorage, NewGenomesUseTotalInputCount) {
    genetics::GenomeStorage storage;
    const auto genome_id = storage.create_random(1234ULL);
    const auto* genome = storage.get(genome_id);
    ASSERT_NE(genome, nullptr);

    if (genome->brain_kind() == evolution::genome::BrainKind::MLP) {
        ASSERT_NE(genome->mlp(), nullptr);
        EXPECT_EQ(genome->mlp()->input_count(), kTotalInputCount);
    } else {
        ASSERT_NE(genome->neat(), nullptr);
        EXPECT_EQ(genome->neat()->input_count(), kTotalInputCount);
    }
}

}  // namespace evolution::sim
