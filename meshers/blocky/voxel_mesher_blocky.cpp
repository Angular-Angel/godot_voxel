#include "voxel_mesher_blocky.h"
#include "../../constants/cube_tables.h"
#include "../../storage/voxel_buffer_internal.h"
#include "../../util/godot/core/array.h"
#include "../../util/godot/funcs.h"
#include "../../util/macros.h"
#include "../../util/math/conv.h"
#include "../../util/span.h"
// TODO GDX: String has no `operator+=`
#include "../../util/godot/core/string.h"

namespace zylann::voxel {

// Utility functions
namespace {
const int g_opposite_side[6] = {
	Cube::SIDE_NEGATIVE_X, //
	Cube::SIDE_POSITIVE_X, //
	Cube::SIDE_POSITIVE_Y, //
	Cube::SIDE_NEGATIVE_Y, //
	Cube::SIDE_POSITIVE_Z, //
	Cube::SIDE_NEGATIVE_Z //
};

inline bool is_face_visible(const VoxelBlockyLibraryBase::BakedData &lib, const VoxelBlockyModel::BakedData &vt,
		uint32_t other_voxel_id, int side) {
	if (other_voxel_id < lib.models.size()) {
		const VoxelBlockyModel::BakedData &other_vt = lib.models[other_voxel_id];
		if (other_vt.empty || (other_vt.transparency_index > vt.transparency_index)) {
			return true;
		} else {
			const unsigned int ai = vt.model.side_pattern_indices[side];
			const unsigned int bi = other_vt.model.side_pattern_indices[g_opposite_side[side]];
			// Patterns are not the same, and B does not occlude A
			return (ai != bi) && !lib.get_side_pattern_occlusion(bi, ai);
		}
	}
	return true;
}

inline bool contributes_to_ao(const VoxelBlockyLibraryBase::BakedData &lib, uint32_t voxel_id) {
	if (voxel_id < lib.models.size()) {
		const VoxelBlockyModel::BakedData &t = lib.models[voxel_id];
		return t.contributes_to_ao;
	}
	return true;
}

std::vector<int> &get_tls_index_offsets() {
	static thread_local std::vector<int> tls_index_offsets;
	return tls_index_offsets;
}

} // namespace

struct NeighborLUTs {
        FixedArray<int, Cube::SIDE_COUNT> sides;
	FixedArray<int, Cube::EDGE_COUNT> edges;
	FixedArray<int, Cube::CORNER_COUNT> corners;
};

NeighborLUTs generate_neighbor_luts(const int row_size, const int deck_size) {
        NeighborLUTs neighbor_luts;

	neighbor_luts.sides[Cube::SIDE_LEFT] = row_size;
	neighbor_luts.sides[Cube::SIDE_RIGHT] = -row_size;
	neighbor_luts.sides[Cube::SIDE_BACK] = -deck_size;
	neighbor_luts.sides[Cube::SIDE_FRONT] = deck_size;
	neighbor_luts.sides[Cube::SIDE_BOTTOM] = -1;
	neighbor_luts.sides[Cube::SIDE_TOP] = 1;

	neighbor_luts.edges[Cube::EDGE_BOTTOM_BACK] =
			neighbor_luts.sides[Cube::SIDE_BOTTOM] + neighbor_luts.sides[Cube::SIDE_BACK];
	neighbor_luts.edges[Cube::EDGE_BOTTOM_FRONT] =
			neighbor_luts.sides[Cube::SIDE_BOTTOM] + neighbor_luts.sides[Cube::SIDE_FRONT];
	neighbor_luts.edges[Cube::EDGE_BOTTOM_LEFT] =
			neighbor_luts.sides[Cube::SIDE_BOTTOM] + neighbor_luts.sides[Cube::SIDE_LEFT];
	neighbor_luts.edges[Cube::EDGE_BOTTOM_RIGHT] =
			neighbor_luts.sides[Cube::SIDE_BOTTOM] + neighbor_luts.sides[Cube::SIDE_RIGHT];
	neighbor_luts.edges[Cube::EDGE_BACK_LEFT] = neighbor_luts.sides[Cube::SIDE_BACK] + neighbor_luts.sides[Cube::SIDE_LEFT];
	neighbor_luts.edges[Cube::EDGE_BACK_RIGHT] = neighbor_luts.sides[Cube::SIDE_BACK] + neighbor_luts.sides[Cube::SIDE_RIGHT];
	neighbor_luts.edges[Cube::EDGE_FRONT_LEFT] = neighbor_luts.sides[Cube::SIDE_FRONT] + neighbor_luts.sides[Cube::SIDE_LEFT];
	neighbor_luts.edges[Cube::EDGE_FRONT_RIGHT] =
			neighbor_luts.sides[Cube::SIDE_FRONT] + neighbor_luts.sides[Cube::SIDE_RIGHT];
	neighbor_luts.edges[Cube::EDGE_TOP_BACK] = neighbor_luts.sides[Cube::SIDE_TOP] + neighbor_luts.sides[Cube::SIDE_BACK];
	neighbor_luts.edges[Cube::EDGE_TOP_FRONT] = neighbor_luts.sides[Cube::SIDE_TOP] + neighbor_luts.sides[Cube::SIDE_FRONT];
	neighbor_luts.edges[Cube::EDGE_TOP_LEFT] = neighbor_luts.sides[Cube::SIDE_TOP] + neighbor_luts.sides[Cube::SIDE_LEFT];
	neighbor_luts.edges[Cube::EDGE_TOP_RIGHT] = neighbor_luts.sides[Cube::SIDE_TOP] + neighbor_luts.sides[Cube::SIDE_RIGHT];

	neighbor_luts.corners[Cube::CORNER_BOTTOM_BACK_LEFT] = neighbor_luts.sides[Cube::SIDE_BOTTOM] +
			neighbor_luts.sides[Cube::SIDE_BACK] + neighbor_luts.sides[Cube::SIDE_LEFT];

	neighbor_luts.corners[Cube::CORNER_BOTTOM_BACK_RIGHT] = neighbor_luts.sides[Cube::SIDE_BOTTOM] +
			neighbor_luts.sides[Cube::SIDE_BACK] + neighbor_luts.sides[Cube::SIDE_RIGHT];

	neighbor_luts.corners[Cube::CORNER_BOTTOM_FRONT_RIGHT] = neighbor_luts.sides[Cube::SIDE_BOTTOM] +
			neighbor_luts.sides[Cube::SIDE_FRONT] + neighbor_luts.sides[Cube::SIDE_RIGHT];

	neighbor_luts.corners[Cube::CORNER_BOTTOM_FRONT_LEFT] = neighbor_luts.sides[Cube::SIDE_BOTTOM] +
			neighbor_luts.sides[Cube::SIDE_FRONT] + neighbor_luts.sides[Cube::SIDE_LEFT];

	neighbor_luts.corners[Cube::CORNER_TOP_BACK_LEFT] =
			neighbor_luts.sides[Cube::SIDE_TOP] + neighbor_luts.sides[Cube::SIDE_BACK] + neighbor_luts.sides[Cube::SIDE_LEFT];

	neighbor_luts.corners[Cube::CORNER_TOP_BACK_RIGHT] = neighbor_luts.sides[Cube::SIDE_TOP] +
			neighbor_luts.sides[Cube::SIDE_BACK] + neighbor_luts.sides[Cube::SIDE_RIGHT];

	neighbor_luts.corners[Cube::CORNER_TOP_FRONT_RIGHT] = neighbor_luts.sides[Cube::SIDE_TOP] +
			neighbor_luts.sides[Cube::SIDE_FRONT] + neighbor_luts.sides[Cube::SIDE_RIGHT];

	neighbor_luts.corners[Cube::CORNER_TOP_FRONT_LEFT] = neighbor_luts.sides[Cube::SIDE_TOP] +
			neighbor_luts.sides[Cube::SIDE_FRONT] + neighbor_luts.sides[Cube::SIDE_LEFT];
        return neighbor_luts;
}

template <typename Type_T>
void shade_corners(const Span<Type_T> type_buffer, const VoxelBlockyLibraryBase::BakedData &library,
                NeighborLUTs &neighbor_luts, unsigned int side,
                const int voxel_index, int shaded_corner[]) {
        // Combinatory solution for
        // https://0fps.net/2013/07/03/ambient-occlusion-for-minecraft-like-worlds/ (inverted)
        //	function vertexAO(side1, side2, corner) {
        //	  if(side1 && side2) {
        //		return 0
        //	  }
        //	  return 3 - (side1 + side2 + corner)
        //	}

        for (unsigned int j = 0; j < 4; ++j) {
                const unsigned int edge = Cube::g_side_edges[side][j];
                const int edge_neighbor_id = type_buffer[voxel_index + neighbor_luts.edges[edge]];
                if (contributes_to_ao(library, edge_neighbor_id)) {
                        ++shaded_corner[Cube::g_edge_corners[edge][0]];
                        ++shaded_corner[Cube::g_edge_corners[edge][1]];
                }
        }
        for (unsigned int j = 0; j < 4; ++j) {
                const unsigned int corner = Cube::g_side_corners[side][j];
                if (shaded_corner[corner] == 2) {
                        shaded_corner[corner] = 3;
                } else {
                        const int corner_neigbor_id = type_buffer[voxel_index + neighbor_luts.corners[corner]];
                        if (contributes_to_ao(library, corner_neigbor_id)) {
                                ++shaded_corner[corner];
                        }
                }
        }

}

Color get_surface_occlusion(const std::vector<Vector3f> &side_positions, const unsigned int vertex_count, unsigned int side, 
                int shaded_corner[], float baked_occlusion_darkness, const Color modulate_color, int i) {
        const Vector3f vertex_pos = side_positions[i];

        // General purpose occlusion colouring.
        // TODO Optimize for cubes
        // TODO Fix occlusion inconsistency caused by triangles orientation? Not sure if
        // worth it
        float shade = 0;
        for (unsigned int j = 0; j < 4; ++j) {
                unsigned int corner = Cube::g_side_corners[side][j];
                if (shaded_corner[corner]) {
                        float s = baked_occlusion_darkness *
                                        static_cast<float>(shaded_corner[corner]);
                        // float k = 1.f - Cube::g_corner_position[corner].distance_to(v);
                        float k = 1.f -
                                        math::distance_squared(Cube::g_corner_position[corner], vertex_pos);
                        if (k < 0.0) {
                                k = 0.0;
                        }
                        s *= k;
                        if (s > shade) {
                                shade = s;
                        }
                }
        }
        const float gs = 1.0 - shade;
        return Color(gs, gs, gs) * modulate_color;
}

void generate_side_collision_surface(VoxelMesher::Output::CollisionSurface *collision_surface,
                const unsigned int vertex_count, int &collision_surface_index_offset,
                const Vector3f &pos, const std::vector<Vector3f> &side_positions,
                const std::vector<int> &side_indices, const unsigned int index_count) {
        std::vector<Vector3f> &dst_positions = collision_surface->positions;
        std::vector<int> &dst_indices = collision_surface->indices;

        {
                const unsigned int append_index = dst_positions.size();
                dst_positions.resize(dst_positions.size() + vertex_count);
                Vector3f *w = dst_positions.data() + append_index;
                for (unsigned int i = 0; i < vertex_count; ++i) {
                        w[i] = side_positions[i] + pos;
                }
        }

        {
                int i = dst_indices.size();
                dst_indices.resize(dst_indices.size() + index_count);
                int *w = dst_indices.data();
                for (unsigned int j = 0; j < index_count; ++j) {
                        w[i++] = collision_surface_index_offset + side_indices[j];
                }
        }

        collision_surface_index_offset += vertex_count;
}

void append_side_positions(VoxelMesherBlocky::Arrays &arrays, const Vector3f &pos,
                const std::vector<Vector3f> &side_positions, const unsigned int vertex_count) {
        
        const int append_index = arrays.positions.size();
        arrays.positions.resize(arrays.positions.size() + vertex_count);
        Vector3f *w = arrays.positions.data() + append_index;
        for (unsigned int i = 0; i < vertex_count; ++i) {
                w[i] = side_positions[i] + pos;
        }
}

template <typename Type_T>
void append_array_data(std::vector<Type_T> &target_array, const std::vector<Type_T> &source_array,
                const unsigned int vertex_count, const unsigned int data_size) {

        const int append_index = target_array.size();
        target_array.resize(target_array.size() + vertex_count);
        memcpy(target_array.data() + append_index, source_array.data(), vertex_count * data_size);
}

template <typename Type_T>
void append_array_data_from_function(std::vector<Type_T> &target_array, std::function<Type_T(int)> &source_function,
                const unsigned int vertex_count) {

        const int append_index = target_array.size();
        target_array.resize(target_array.size() + vertex_count);
        Type_T *w = target_array.data() + append_index;
        for (unsigned int i = 0; i < vertex_count; ++i)
                w[i] = source_function(i);
}

void append_side_uvs(VoxelMesherBlocky::Arrays &arrays, const std::vector<Vector2f> &side_uvs,
                const unsigned int vertex_count) {
        append_array_data(arrays.uvs, side_uvs, vertex_count, sizeof(Vector2f));
}

void append_side_tangents(VoxelMesherBlocky::Arrays &arrays, const std::vector<float> &side_tangents,
                const unsigned int vertex_count) {
        if (side_tangents.size() > 0)
                append_array_data(arrays.tangents, side_tangents, vertex_count * 4, sizeof(float));
}

void append_side_normals(VoxelMesherBlocky::Arrays &arrays, const unsigned int vertex_count, unsigned int side) {
        std::function<Vector3f(int)> source_function = [side] (int i) { return to_vec3f(Cube::g_side_normals[side]); };
        append_array_data_from_function(arrays.normals, source_function, vertex_count);
}

void append_side_colors(VoxelMesherBlocky::Arrays &arrays, const unsigned int vertex_count, unsigned int side,
                const std::vector<Vector3f> &side_positions, bool bake_occlusion, float baked_occlusion_darkness,
                const Color modulate_color, int shaded_corner[]) {

        std::function<Color(int)> source_function;

        if (bake_occlusion) {
                source_function = [&] (int i ) {
                        return get_surface_occlusion(side_positions, vertex_count, side, shaded_corner,
                                baked_occlusion_darkness, modulate_color, i);
                };
        } else {
                source_function = [&] (int i ) {
                        return modulate_color;
                };
        }

        append_array_data_from_function(arrays.colors, source_function, vertex_count);
}

void append_side_indices(VoxelMesherBlocky::Arrays &arrays, int &index_offset, 
                const std::vector<int> &side_indices, const unsigned int index_count) {

        std::function<int(int)> source_function = [&] (int i ) {
                return index_offset + side_indices[i];
        };

        append_array_data_from_function(arrays.indices, source_function, index_count);
}

void generate_side_surface(std::vector<VoxelMesherBlocky::Arrays> &out_arrays_per_material,
		VoxelMesher::Output::CollisionSurface *collision_surface,
		bool bake_occlusion, float baked_occlusion_darkness, int shaded_corner[],
                std::vector<int> &index_offsets, int &collision_surface_index_offset,
                unsigned int side, const VoxelBlockyModel::BakedData &voxel, const Vector3f &pos,
                const VoxelBlockyModel::BakedData::Surface &surface) {

        VoxelMesherBlocky::Arrays &arrays = out_arrays_per_material[surface.material_id];

        ZN_ASSERT(surface.material_id >= 0 && surface.material_id < index_offsets.size());

        const std::vector<Vector3f> &side_positions = surface.side_positions[side];
        const unsigned int vertex_count = side_positions.size();

        // Append vertices of the faces in one go, don't use push_back

        append_side_positions(arrays, pos, side_positions, vertex_count);

        append_side_uvs(arrays, surface.side_uvs[side], vertex_count);

        append_side_tangents(arrays, surface.side_tangents[side], vertex_count);

        append_side_normals(arrays, vertex_count, side);

        append_side_colors(arrays, vertex_count, side, side_positions, bake_occlusion,
                        baked_occlusion_darkness, voxel.color, shaded_corner);

        int &index_offset = index_offsets[surface.material_id];
        const std::vector<int> &side_indices = surface.side_indices[side];
        const unsigned int index_count = side_indices.size();

        append_side_indices(arrays, index_offset, side_indices, index_count);

        if (collision_surface != nullptr && surface.collision_enabled) {
                generate_side_collision_surface(collision_surface, vertex_count, 
                                collision_surface_index_offset, pos, side_positions,
                                side_indices, index_count);
        }

        index_offset += vertex_count;
}

template <typename Type_T>
void generate_side_mesh(std::vector<VoxelMesherBlocky::Arrays> &out_arrays_per_material,
		VoxelMesher::Output::CollisionSurface *collision_surface, const Span<Type_T> type_buffer,
		const VoxelBlockyLibraryBase::BakedData &library, bool bake_occlusion, float baked_occlusion_darkness,
                std::vector<int> &index_offsets, int &collision_surface_index_offset, 
                NeighborLUTs &neighbor_luts,
                unsigned int x, unsigned int y, unsigned int z, unsigned int side, const int voxel_index,
                const VoxelBlockyModel::BakedData &voxel, const VoxelBlockyModel::BakedData::Model &model) {
        if ((model.empty_sides_mask & (1 << side)) != 0) {
                // This side is empty
                return;
        }

        const uint32_t neighbor_voxel_id = type_buffer[voxel_index + neighbor_luts.sides[side]];

        if (!is_face_visible(library, voxel, neighbor_voxel_id, side)) {
                return;
        }

        // The face is visible

        int shaded_corner[8] = { 0 };

        if (bake_occlusion) {
                shade_corners(type_buffer, library, neighbor_luts,
                                side, voxel_index, shaded_corner);
        }

        // Subtracting 1 because the data is padded
        Vector3f pos(x - 1, y - 1, z - 1);

        for (unsigned int surface_index = 0; surface_index < model.surface_count; ++surface_index) {
                generate_side_surface(out_arrays_per_material, collision_surface, bake_occlusion,
                                baked_occlusion_darkness, shaded_corner, index_offsets,
                                collision_surface_index_offset, side, voxel, pos, model.surfaces[surface_index]);
        }
}


void generate_inside_mesh(std::vector<VoxelMesherBlocky::Arrays> &out_arrays_per_material,
		VoxelMesher::Output::CollisionSurface *collision_surface,
                std::vector<int> &index_offsets, int &collision_surface_index_offset,
                unsigned int x, unsigned int y, unsigned int z,
                const VoxelBlockyModel::BakedData &voxel, const VoxelBlockyModel::BakedData::Surface &surface) {
        if (surface.positions.size() == 0) {
                return;
        }
        // TODO Get rid of push_backs

        VoxelMesherBlocky::Arrays &arrays = out_arrays_per_material[surface.material_id];

        ZN_ASSERT(surface.material_id >= 0 && surface.material_id < index_offsets.size());
        int &index_offset = index_offsets[surface.material_id];

        const std::vector<Vector3f> &positions = surface.positions;
        const unsigned int vertex_count = positions.size();
        const Color modulate_color = voxel.color;

        const std::vector<Vector3f> &normals = surface.normals;
        const std::vector<Vector2f> &uvs = surface.uvs;
        const std::vector<float> &tangents = surface.tangents;

        const Vector3f pos(x - 1, y - 1, z - 1);

        if (tangents.size() > 0) {
                const int append_index = arrays.tangents.size();
                arrays.tangents.resize(arrays.tangents.size() + vertex_count * 4);
                memcpy(arrays.tangents.data() + append_index, tangents.data(),
                                (vertex_count * 4) * sizeof(float));
        }

        for (unsigned int i = 0; i < vertex_count; ++i) {
                arrays.normals.push_back(normals[i]);
                arrays.uvs.push_back(uvs[i]);
                arrays.positions.push_back(positions[i] + pos);
                // TODO handle ambient occlusion on inner parts
                arrays.colors.push_back(modulate_color);
        }

        const std::vector<int> &indices = surface.indices;
        const unsigned int index_count = indices.size();

        for (unsigned int i = 0; i < index_count; ++i) {
                arrays.indices.push_back(index_offset + indices[i]);
        }

        if (collision_surface != nullptr && surface.collision_enabled) {
                std::vector<Vector3f> &dst_positions = collision_surface->positions;
                std::vector<int> &dst_indices = collision_surface->indices;

                for (unsigned int i = 0; i < vertex_count; ++i) {
                        dst_positions.push_back(positions[i] + pos);
                }
                for (unsigned int i = 0; i < index_count; ++i) {
                        dst_indices.push_back(collision_surface_index_offset + indices[i]);
                }

                collision_surface_index_offset += vertex_count;
        }

        index_offset += vertex_count;

}

template <typename Type_T>
void generate_voxel_mesh(std::vector<VoxelMesherBlocky::Arrays> &out_arrays_per_material,
		VoxelMesher::Output::CollisionSurface *collision_surface, 
                std::vector<int> &index_offsets, int &collision_surface_index_offset,
                NeighborLUTs &neighbor_luts,
                const Span<Type_T> type_buffer, const VoxelBlockyLibraryBase::BakedData &library,
                bool bake_occlusion, float baked_occlusion_darkness, const int voxel_index,
                unsigned int x, unsigned int y, unsigned int z) {
        const int voxel_id = type_buffer[voxel_index];

        if (voxel_id == VoxelBlockyModel::AIR_ID || !library.has_model(voxel_id)) {
                return;
        }

        const VoxelBlockyModel::BakedData &voxel = library.models[voxel_id];
        const VoxelBlockyModel::BakedData::Model &model = voxel.model;

        // Hybrid approach: extract cube faces and decimate those that aren't visible,
        // and still allow voxels to have geometry that is not a cube.

        // Sides
        for (unsigned int side = 0; side < Cube::SIDE_COUNT; ++side) {
                generate_side_mesh(out_arrays_per_material, collision_surface, type_buffer, 
                                library, bake_occlusion, baked_occlusion_darkness,
                                index_offsets, collision_surface_index_offset, 
                                neighbor_luts,
                                x, y, z, side, voxel_index, voxel, model);
        }

        // Inside
        for (unsigned int surface_index = 0; surface_index < model.surface_count; ++surface_index) {
                generate_inside_mesh(out_arrays_per_material, collision_surface, index_offsets,
                                collision_surface_index_offset, x, y, z, voxel, model.surfaces[surface_index]);
        }
}

template <typename Type_T>
void generate_blocky_mesh(std::vector<VoxelMesherBlocky::Arrays> &out_arrays_per_material,
		VoxelMesher::Output::CollisionSurface *collision_surface, const Span<Type_T> type_buffer,
		const Vector3i block_size, const VoxelBlockyLibraryBase::BakedData &library, bool bake_occlusion,
		float baked_occlusion_darkness) {
	// TODO Optimization: not sure if this mandates a template function. There is so much more happening in this
	// function other than reading voxels, although reading is on the hottest path. It needs to be profiled. If
	// changing makes no difference, we could use a function pointer or switch inside instead to reduce executable size.

	ERR_FAIL_COND(block_size.x < static_cast<int>(2 * VoxelMesherBlocky::PADDING) ||
			block_size.y < static_cast<int>(2 * VoxelMesherBlocky::PADDING) ||
			block_size.z < static_cast<int>(2 * VoxelMesherBlocky::PADDING));

	// Build lookup tables so to speed up voxel access.
	// These are values to add to an address in order to get given neighbor.

	const int row_size = block_size.y;
	const int deck_size = block_size.x * row_size;

	// Data must be padded, hence the off-by-one
	const Vector3i min = Vector3iUtil::create(VoxelMesherBlocky::PADDING);
	const Vector3i max = block_size - Vector3iUtil::create(VoxelMesherBlocky::PADDING);

	std::vector<int> &index_offsets = get_tls_index_offsets();
	index_offsets.clear();
	index_offsets.resize(out_arrays_per_material.size(), 0);

	int collision_surface_index_offset = 0;

	NeighborLUTs neighbor_luts = generate_neighbor_luts(row_size, deck_size);

	// uint64_t time_prep = Time::get_singleton()->get_ticks_usec() - time_before;
	// time_before = Time::get_singleton()->get_ticks_usec();

	for (unsigned int z = min.z; z < (unsigned int)max.z; ++z) {
		for (unsigned int x = min.x; x < (unsigned int)max.x; ++x) {
			for (unsigned int y = min.y; y < (unsigned int)max.y; ++y) {
				// min and max are chosen such that you can visit 1 neighbor away from the current voxel without size
				// check

				const int voxel_index = y + x * row_size + z * deck_size;
				generate_voxel_mesh(out_arrays_per_material, collision_surface, index_offsets, 
                                                collision_surface_index_offset, neighbor_luts, type_buffer,
                                                library, bake_occlusion, baked_occlusion_darkness,
                                                voxel_index, x, y, z);
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

VoxelMesherBlocky::VoxelMesherBlocky() {
	set_padding(PADDING, PADDING);
}

VoxelMesherBlocky::~VoxelMesherBlocky() {}

VoxelMesherBlocky::Cache &VoxelMesherBlocky::get_tls_cache() {
	thread_local Cache cache;
	return cache;
}

void VoxelMesherBlocky::set_library(Ref<VoxelBlockyLibraryBase> library) {
	RWLockWrite wlock(_parameters_lock);
	_parameters.library = library;
}

Ref<VoxelBlockyLibraryBase> VoxelMesherBlocky::get_library() const {
	RWLockRead rlock(_parameters_lock);
	return _parameters.library;
}

void VoxelMesherBlocky::set_occlusion_darkness(float darkness) {
	RWLockWrite wlock(_parameters_lock);
	_parameters.baked_occlusion_darkness = math::clamp(darkness, 0.0f, 1.0f);
}

float VoxelMesherBlocky::get_occlusion_darkness() const {
	RWLockRead rlock(_parameters_lock);
	return _parameters.baked_occlusion_darkness;
}

void VoxelMesherBlocky::set_occlusion_enabled(bool enable) {
	RWLockWrite wlock(_parameters_lock);
	_parameters.bake_occlusion = enable;
}

bool VoxelMesherBlocky::get_occlusion_enabled() const {
	RWLockRead rlock(_parameters_lock);
	return _parameters.bake_occlusion;
}

void VoxelMesherBlocky::build(VoxelMesher::Output &output, const VoxelMesher::Input &input) {
	const int channel = VoxelBufferInternal::CHANNEL_TYPE;
	Parameters params;
	{
		RWLockRead rlock(_parameters_lock);
		params = _parameters;
	}

	ERR_FAIL_COND(params.library.is_null());

	Cache &cache = get_tls_cache();

	std::vector<Arrays> &arrays_per_material = cache.arrays_per_material;
	for (unsigned int i = 0; i < arrays_per_material.size(); ++i) {
		Arrays &a = arrays_per_material[i];
		a.clear();
	}

	float baked_occlusion_darkness = 0;
	if (params.bake_occlusion) {
		baked_occlusion_darkness = params.baked_occlusion_darkness / 3.0f;
	}

	// The technique is Culled faces.
	// Could be improved with greedy meshing: https://0fps.net/2012/06/30/meshing-in-a-minecraft-game/
	// However I don't feel it's worth it yet:
	// - Not so much gain for organic worlds with lots of texture variations
	// - Works well with cubes but not with any shape
	// - Slower
	// => Could be implemented in a separate class?

	const VoxelBufferInternal &voxels = input.voxels;
#ifdef TOOLS_ENABLED
	if (input.lod_index != 0) {
		WARN_PRINT("VoxelMesherBlocky received lod != 0, it is not supported");
	}
#endif

	// Iterate 3D padded data to extract voxel faces.
	// This is the most intensive job in this class, so all required data should be as fit as possible.

	// The buffer we receive MUST be dense (i.e not compressed, and channels allocated).
	// That means we can use raw pointers to voxel data inside instead of using the higher-level getters,
	// and then save a lot of time.

	if (voxels.get_channel_compression(channel) == VoxelBufferInternal::COMPRESSION_UNIFORM) {
		// All voxels have the same type.
		// If it's all air, nothing to do. If it's all cubes, nothing to do either.
		// TODO Handle edge case of uniform block with non-cubic voxels!
		// If the type of voxel still produces geometry in this situation (which is an absurd use case but not an
		// error), decompress into a backing array to still allow the use of the same algorithm.
		return;

	} else if (voxels.get_channel_compression(channel) != VoxelBufferInternal::COMPRESSION_NONE) {
		// No other form of compression is allowed
		ERR_PRINT("VoxelMesherBlocky received unsupported voxel compression");
		return;
	}

	Span<uint8_t> raw_channel;
	if (!voxels.get_channel_raw(channel, raw_channel)) {
		/*       _
		//      | \
		//     /\ \\
		//    / /|\\\
		//    | |\ \\\
		//    | \_\ \\|
		//    |    |  )
		//     \   |  |
		//      \    /
		*/
		// Case supposedly handled before...
		ERR_PRINT("Something wrong happened");
		return;
	}

	const Vector3i block_size = voxels.get_size();
	const VoxelBufferInternal::Depth channel_depth = voxels.get_channel_depth(channel);

	VoxelMesher::Output::CollisionSurface *collision_surface = nullptr;
	if (input.collision_hint) {
		collision_surface = &output.collision_surface;
	}

	unsigned int material_count = 0;
	{
		// We can only access baked data. Only this data is made for multithreaded access.
		RWLockRead lock(params.library->get_baked_data_rw_lock());
		const VoxelBlockyLibraryBase::BakedData &library_baked_data = params.library->get_baked_data();

		material_count = library_baked_data.indexed_materials_count;

		if (arrays_per_material.size() < material_count) {
			arrays_per_material.resize(material_count);
		}

		switch (channel_depth) {
			case VoxelBufferInternal::DEPTH_8_BIT:
				generate_blocky_mesh(arrays_per_material, collision_surface, raw_channel, block_size,
						library_baked_data, params.bake_occlusion, baked_occlusion_darkness);
				break;

			case VoxelBufferInternal::DEPTH_16_BIT:
				generate_blocky_mesh(arrays_per_material, collision_surface,
						raw_channel.reinterpret_cast_to<uint16_t>(), block_size, library_baked_data,
						params.bake_occlusion, baked_occlusion_darkness);
				break;

			default:
				ERR_PRINT("Unsupported voxel depth");
				return;
		}
	}

	// TODO Optimization: we could return a single byte array and use Mesh::add_surface down the line?
	// That API does not seem to exist yet though.

	for (unsigned int material_index = 0; material_index < material_count; ++material_index) {
		const Arrays &arrays = arrays_per_material[material_index];

		if (arrays.positions.size() != 0) {
			Array mesh_arrays;
			mesh_arrays.resize(Mesh::ARRAY_MAX);

			{
				PackedVector3Array positions;
				PackedVector2Array uvs;
				PackedVector3Array normals;
				PackedColorArray colors;
				PackedInt32Array indices;

				copy_to(positions, arrays.positions);
				copy_to(uvs, arrays.uvs);
				copy_to(normals, arrays.normals);
				copy_to(colors, arrays.colors);
				copy_to(indices, arrays.indices);

				mesh_arrays[Mesh::ARRAY_VERTEX] = positions;
				mesh_arrays[Mesh::ARRAY_TEX_UV] = uvs;
				mesh_arrays[Mesh::ARRAY_NORMAL] = normals;
				mesh_arrays[Mesh::ARRAY_COLOR] = colors;
				mesh_arrays[Mesh::ARRAY_INDEX] = indices;

				if (arrays.tangents.size() > 0) {
					PackedFloat32Array tangents;
					copy_to(tangents, arrays.tangents);
					mesh_arrays[Mesh::ARRAY_TANGENT] = tangents;
				}
			}

			output.surfaces.push_back(Output::Surface());
			Output::Surface &surface = output.surfaces.back();
			surface.arrays = mesh_arrays;
			surface.material_index = material_index;
		}
		//  else {
		// 	// Empty
		// 	output.surfaces.push_back(Output::Surface());
		// }
	}

	output.primitive_type = Mesh::PRIMITIVE_TRIANGLES;
}

Ref<Resource> VoxelMesherBlocky::duplicate(bool p_subresources) const {
	Parameters params;
	{
		RWLockRead rlock(_parameters_lock);
		params = _parameters;
	}

	if (p_subresources && params.library.is_valid()) {
		params.library = params.library->duplicate(true);
	}

	VoxelMesherBlocky *c = memnew(VoxelMesherBlocky);
	c->_parameters = params;
	return c;
}

int VoxelMesherBlocky::get_used_channels_mask() const {
	return (1 << VoxelBufferInternal::CHANNEL_TYPE);
}

Ref<Material> VoxelMesherBlocky::get_material_by_index(unsigned int index) const {
	Ref<VoxelBlockyLibraryBase> lib = get_library();
	if (lib.is_null()) {
		return Ref<Material>();
	}
	return lib->get_material_by_index(index);
}

#ifdef TOOLS_ENABLED

void VoxelMesherBlocky::get_configuration_warnings(PackedStringArray &out_warnings) const {
	Ref<VoxelBlockyLibraryBase> library = get_library();

	if (library.is_null()) {
		out_warnings.append(String(ZN_TTR("{0} has no {1} assigned."))
									.format(varray(VoxelMesherBlocky::get_class_static(),
											VoxelBlockyLibraryBase::get_class_static())));
		return;
	}

	const VoxelBlockyLibraryBase::BakedData &baked_data = library->get_baked_data();
	RWLockRead rlock(library->get_baked_data_rw_lock());

	if (baked_data.models.size() == 0) {
		out_warnings.append(String(ZN_TTR("The {0} assigned to {1} has no baked models."))
									.format(varray(library->get_class(), VoxelMesherBlocky::get_class_static())));
		return;
	}

	library->get_configuration_warnings(out_warnings);
}

#endif // TOOLS_ENABLED

void VoxelMesherBlocky::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_library", "voxel_library"), &VoxelMesherBlocky::set_library);
	ClassDB::bind_method(D_METHOD("get_library"), &VoxelMesherBlocky::get_library);

	ClassDB::bind_method(D_METHOD("set_occlusion_enabled", "enable"), &VoxelMesherBlocky::set_occlusion_enabled);
	ClassDB::bind_method(D_METHOD("get_occlusion_enabled"), &VoxelMesherBlocky::get_occlusion_enabled);

	ClassDB::bind_method(D_METHOD("set_occlusion_darkness", "value"), &VoxelMesherBlocky::set_occlusion_darkness);
	ClassDB::bind_method(D_METHOD("get_occlusion_darkness"), &VoxelMesherBlocky::get_occlusion_darkness);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "library", PROPERTY_HINT_RESOURCE_TYPE,
						 VoxelBlockyLibraryBase::get_class_static(), PROPERTY_USAGE_DEFAULT
						 // Sadly we can't use this hint because the property type is abstract... can't just choose a
						 // default child class. This hint becomes less and less useful everytime I come across it...
						 //| PROPERTY_USAGE_EDITOR_INSTANTIATE_OBJECT
						 ),
			"set_library", "get_library");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "occlusion_enabled"), "set_occlusion_enabled", "get_occlusion_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "occlusion_darkness", PROPERTY_HINT_RANGE, "0,1,0.01"),
			"set_occlusion_darkness", "get_occlusion_darkness");
}

} // namespace zylann::voxel
