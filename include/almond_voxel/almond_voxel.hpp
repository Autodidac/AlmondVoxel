#pragma once

#if defined(ALMOND_VOXEL_USE_AMALGAMATED_HEADER)
#include "almond_voxel_single.hpp"
#else
#include "almond_voxel/chunk.hpp"
#include "almond_voxel/core.hpp"
#include "almond_voxel/editing/voxel_editing.hpp"
#include "almond_voxel/generation/noise.hpp"
#include "almond_voxel/meshing/greedy_mesher.hpp"
#include "almond_voxel/meshing/marching_cubes.hpp"
#include "almond_voxel/meshing/mesh_types.hpp"
#include "almond_voxel/serialization/region_io.hpp"
#include "almond_voxel/terrain/classic.hpp"
#include "almond_voxel/world.hpp"
#endif
