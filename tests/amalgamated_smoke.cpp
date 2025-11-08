#include <almond_voxel_single.hpp>

#include "test_framework.hpp"

using namespace almond::voxel;

TEST_CASE(amalgamated_header_smoke) {
    chunk_storage chunk;
    CHECK(chunk.volume() > 0);
}
