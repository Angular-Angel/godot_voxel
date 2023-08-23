#include "voxel_mesher_blocky_side.h"
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

VoxelMesherBlockySide::VoxelMesherBlockySide() : VoxelMesherBlocky() {}

VoxelMesherBlockySide::~VoxelMesherBlockySide() {}

void VoxelMesherBlockySide::set_side_library(Ref<VoxelSideLibrary> library) {
	RWLockWrite wlock(_side_parameters_lock);
	_side_parameters.library = library;
}

Ref<VoxelSideLibrary> VoxelMesherBlockySide::get_side_library() const {
	RWLockRead rlock(_side_parameters_lock);
	return _side_parameters.library;
}

Ref<Resource> VoxelMesherBlockySide::duplicate(bool p_subresources) const {
	Parameters params;
	{
		RWLockRead rlock(_parameters_lock);
		params = _parameters;
	}

	if (p_subresources && params.library.is_valid()) {
		params.library = params.library->duplicate(true);
	}

	SideParameters side_params;
	{
		RWLockRead side_rlock(_side_parameters_lock);
		side_params = _side_parameters;
	}

	if (p_subresources && side_params.library.is_valid()) {
		side_params.library = side_params.library->duplicate(true);
	}

	VoxelMesherBlockySide *c = memnew(VoxelMesherBlockySide);
	c->_parameters = params;
	c->_side_parameters = side_params;
	return c;
}

int VoxelMesherBlockySide::get_used_channels_mask() const {
	return (1 << VoxelBufferInternal::CHANNEL_TYPE) +
               (1 << VoxelBufferInternal::CHANNEL_SDF) +
               (1 << VoxelBufferInternal::CHANNEL_DATA7) +
               (1 << VoxelBufferInternal::CHANNEL_INDICES) +
               (1 << VoxelBufferInternal::CHANNEL_WEIGHTS) +
               (1 << VoxelBufferInternal::CHANNEL_DATA5) +
               (1 << VoxelBufferInternal::CHANNEL_DATA6);
}

inline bool contributes_to_ao(const VoxelBlockyLibraryBase::BakedData &lib, uint32_t voxel_id) {
	if (voxel_id < lib.models.size()) {
		const VoxelBlockyModel::BakedData &t = lib.models[voxel_id];
		return t.contributes_to_ao;
	}
	return true;
}

template <typename Type_T>
void VoxelMesherBlockySide::shade_corners(const Span<Type_T> type_buffer, const VoxelBlockyLibraryBase::BakedData &library,
                VoxelMesherBlocky::NeighborLUTs &neighbor_luts, unsigned int side,
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

unsigned int VoxelMesherBlockySide::get_material_count() const {
	Parameters params;
	{
		RWLockRead rlock(_parameters_lock);
		params = _parameters;
	}
        RWLockRead lock(params.library->get_baked_data_rw_lock());
        const VoxelBlockyLibraryBase::BakedData &library_baked_data = params.library->get_baked_data();

	SideParameters side_params;
	{
		RWLockRead side_rlock(_side_parameters_lock);
		side_params = _side_parameters;
	}
        RWLockRead side_lock(side_params.library->get_baked_data_rw_lock());
        const VoxelSideLibrary::BakedData &side_library_baked_data = side_params.library->get_baked_data();
        return library_baked_data.indexed_materials_count + side_library_baked_data.indexed_materials_count;
}

Ref<Material> VoxelMesherBlockySide::get_material_by_index(unsigned int index) const {
	Ref<VoxelBlockyLibraryBase> lib = get_library();
	if (lib.is_null()) {
		return Ref<Material>();
	}
        
        if (index < lib->get_material_count()) {
                return lib->get_material_by_index(index);
        } else {
                return get_side_library()->get_material_by_index(index - lib->get_material_count());
        }
}

void VoxelMesherBlockySide::generate_side_surface(std::vector<VoxelMesherBlocky::Arrays> &out_arrays_per_material,
		VoxelMesher::Output::CollisionSurface *collision_surface,
		bool bake_occlusion, float baked_occlusion_darkness, int shaded_corner[],
                std::vector<int> &index_offsets, int &collision_surface_index_offset,
                unsigned int side, const VoxelBlockyModel::BakedData &voxel, unsigned int side_material_offset,
                const VoxelSideModel::BakedData &side_model, const Vector3f &pos,
                const VoxelBlockyModel::BakedData::Surface &surface) {
        VoxelMesherBlocky::generate_side_surface(out_arrays_per_material, collision_surface,
                        bake_occlusion, baked_occlusion_darkness, shaded_corner, index_offsets,
                        collision_surface_index_offset, side, voxel, pos, surface);

        unsigned int material_id = side_model.material_id + side_material_offset;

        VoxelMesherBlocky::Arrays &arrays = out_arrays_per_material[material_id];

        ZN_ASSERT(material_id >= 0 && material_id < index_offsets.size());

        const std::vector<Vector3f> &side_positions = surface.side_positions[side];
        const unsigned int vertex_count = side_positions.size();

        // Append vertices of the faces in one go, don't use push_back

        append_side_positions(arrays, pos, side_positions, vertex_count);

        append_side_uvs(arrays, surface.side_uvs[side], vertex_count);

        append_tangents(arrays, surface.side_tangents[side], vertex_count);

        append_side_normals(arrays, vertex_count, side);

        append_side_colors(arrays, vertex_count, side, side_positions, bake_occlusion,
                        baked_occlusion_darkness, voxel.color, shaded_corner);

        int &index_offset = index_offsets[material_id];
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

template <typename Type_T>
void VoxelMesherBlockySide::generate_side_mesh(std::vector<VoxelMesherBlocky::Arrays> &out_arrays_per_material,
		VoxelMesher::Output::CollisionSurface *collision_surface, const Span<Type_T> type_buffer,
		const VoxelBlockyLibraryBase::BakedData &library, bool bake_occlusion, float baked_occlusion_darkness,
                std::vector<int> &index_offsets, int &collision_surface_index_offset, 
                NeighborLUTs &neighbor_luts, unsigned int x, unsigned int y, unsigned int z, unsigned int side,
                const int voxel_index, const VoxelBlockyModel::BakedData &voxel, 
                const VoxelSideModel::BakedData &side_model, const VoxelBlockyModel::BakedData::Model &voxel_model) {
        if ((voxel_model.empty_sides_mask & (1 << side)) != 0) {
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

        for (unsigned int surface_index = 0; surface_index < voxel_model.surface_count; ++surface_index) {
                generate_side_surface(out_arrays_per_material, collision_surface, bake_occlusion,
                                baked_occlusion_darkness, shaded_corner, index_offsets,
                                collision_surface_index_offset, side, voxel, library.indexed_materials_count,
                                side_model, pos, voxel_model.surfaces[surface_index]);
        }
}

int get_side_channel(unsigned int side) {
    switch (side) {
        case Cube::SIDE_TOP:
            return VoxelBufferInternal::CHANNEL_SDF;
        case Cube::SIDE_BOTTOM:
            return VoxelBufferInternal::CHANNEL_DATA7;
        case Cube::SIDE_BACK:
            return VoxelBufferInternal::CHANNEL_INDICES;
        case Cube::SIDE_FRONT:
            return VoxelBufferInternal::CHANNEL_WEIGHTS;
        case Cube::SIDE_RIGHT:
            return VoxelBufferInternal::CHANNEL_DATA5;
        case Cube::SIDE_LEFT:
            return VoxelBufferInternal::CHANNEL_DATA6;
        }
        
}

template <typename Type_T>
void VoxelMesherBlockySide::generate_side_mesh(std::vector<VoxelMesherBlocky::Arrays> &out_arrays_per_material,
		VoxelMesher::Output::CollisionSurface *collision_surface, const Span<Type_T> type_buffer,
		const VoxelBlockyLibraryBase::BakedData &library, bool bake_occlusion, float baked_occlusion_darkness,
                std::vector<int> &index_offsets, int &collision_surface_index_offset, 
                VoxelMesherBlocky::NeighborLUTs &neighbor_luts, unsigned int x, unsigned int y, unsigned int z, unsigned int side,
                const int voxel_index, const VoxelBlockyModel::BakedData &voxel,
                const VoxelBlockyModel::BakedData::Model &model) {
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
                VoxelMesherBlocky::generate_side_surface(out_arrays_per_material, collision_surface, bake_occlusion,
                                baked_occlusion_darkness, shaded_corner, index_offsets,
                                collision_surface_index_offset, side, voxel, pos, model.surfaces[surface_index]);
        }
}

template <typename Type_T>
void VoxelMesherBlockySide::generate_voxel_mesh(std::vector<VoxelMesherBlocky::Arrays> &out_arrays_per_material,
		VoxelMesher::Output::CollisionSurface *collision_surface, 
                std::vector<int> &index_offsets, int &collision_surface_index_offset,
                VoxelMesherBlocky::NeighborLUTs &neighbor_luts, const VoxelBufferInternal &voxels, const Span<Type_T> type_buffer, 
                const VoxelSideLibrary::BakedData &side_library, const VoxelBlockyLibraryBase::BakedData &voxel_library,
                bool bake_occlusion, float baked_occlusion_darkness, const int voxel_index,
                unsigned int x, unsigned int y, unsigned int z) {
        const int voxel_id = type_buffer[voxel_index];

        if (voxel_id == VoxelBlockyModel::AIR_ID || !voxel_library.has_model(voxel_id)) {
                return;
        }

        const VoxelBlockyModel::BakedData &voxel = voxel_library.models[voxel_id];
        const VoxelBlockyModel::BakedData::Model &voxel_model = voxel.model;

        // Hybrid approach: extract cube faces and decimate those that aren't visible,
        // and still allow voxels to have geometry that is not a cube.

        // Sides
        for (unsigned int side = 0; side < Cube::SIDE_COUNT; ++side) {
                const int channel = get_side_channel(side);
                
                int side_id = 0;
                
                if (voxels.get_channel_compression(channel) == VoxelBufferInternal::COMPRESSION_UNIFORM) {
                        // All voxels have the same type.
                        side_id = voxels.get_voxel(x, y, z, channel);

                } else if (voxels.get_channel_compression(channel) != VoxelBufferInternal::COMPRESSION_NONE) {
                        // No other form of compression is allowed
                        ERR_PRINT("VoxelMesherBlockySide received unsupported voxel compression for side: " + side);
                } else {

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
                        } else {

                                Span<uint32_t> processed_channel = raw_channel.reinterpret_cast_to<uint32_t>();

                                side_id = processed_channel[voxel_index];
                        }
                }
                if (side_id == VoxelSideModel::EMPTY_ID || !side_library.has_model(side_id)) {
                        if (side_id != VoxelSideModel::EMPTY_ID) {
                            std::string text = "Side ID: ";
                            text += std::to_string(side_id);
                            ERR_PRINT(text.c_str());
                        }
                        generate_side_mesh(out_arrays_per_material, collision_surface, type_buffer, 
                                voxel_library, bake_occlusion, baked_occlusion_darkness,
                                index_offsets, collision_surface_index_offset, 
                                neighbor_luts, x, y, z, side, voxel_index, voxel, voxel_model);
                } else {
                        const VoxelSideModel::BakedData &side_model = side_library.models[side_id];

                        generate_side_mesh(out_arrays_per_material, collision_surface, type_buffer, 
                                        voxel_library, bake_occlusion, baked_occlusion_darkness,
                                        index_offsets, collision_surface_index_offset, 
                                        neighbor_luts, x, y, z, side, voxel_index, voxel,
                                        side_model, voxel_model);
                }
        }

        // Inside
        for (unsigned int surface_index = 0; surface_index < voxel_model.surface_count; ++surface_index) {
                generate_inside_mesh(out_arrays_per_material, collision_surface, index_offsets,
                                collision_surface_index_offset, x, y, z, voxel, voxel_model.surfaces[surface_index]);
        }
}

std::vector<int> &get_tls_index_offsets() {
	static thread_local std::vector<int> tls_index_offsets;
	return tls_index_offsets;
}

template <typename Type_T>
void VoxelMesherBlockySide::generate_blocky_side_mesh(std::vector<VoxelMesherBlocky::Arrays> &out_arrays_per_material,
		VoxelMesher::Output::CollisionSurface *collision_surface, const VoxelBufferInternal &voxels, const Span<Type_T> type_buffer,
		const Vector3i block_size, const VoxelSideLibrary::BakedData &side_library,
                const VoxelBlockyLibraryBase::BakedData &voxel_library, bool bake_occlusion,
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

	VoxelMesherBlocky::NeighborLUTs neighbor_luts = generate_neighbor_luts(row_size, deck_size);

	// uint64_t time_prep = Time::get_singleton()->get_ticks_usec() - time_before;
	// time_before = Time::get_singleton()->get_ticks_usec();

	for (unsigned int z = min.z; z < (unsigned int)max.z; ++z) {
		for (unsigned int x = min.x; x < (unsigned int)max.x; ++x) {
			for (unsigned int y = min.y; y < (unsigned int)max.y; ++y) {
				// min and max are chosen such that you can visit 1 neighbor away from the current voxel without size
				// check

				const int voxel_index = y + x * row_size + z * deck_size;
				generate_voxel_mesh(out_arrays_per_material, collision_surface, index_offsets, 
                                                collision_surface_index_offset, neighbor_luts, voxels, type_buffer,
                                                side_library, voxel_library, bake_occlusion, baked_occlusion_darkness,
                                                voxel_index, x, y, z);
			}
		}
	}
}

void VoxelMesherBlockySide::build(VoxelMesher::Output &output, const VoxelMesher::Input &input) {
	const int channel = VoxelBufferInternal::CHANNEL_TYPE;
	Parameters params;
	{
		RWLockRead rlock(_parameters_lock);
		params = _parameters;
	}

	SideParameters side_params;
	{
		RWLockRead side_rlock(_side_parameters_lock);
		side_params = _side_parameters;
	}

	ERR_FAIL_COND(params.library.is_null());

	ERR_FAIL_COND(side_params.library.is_null());

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

		material_count = get_material_count();

		// We can only access baked data. Only this data is made for multithreaded access.
		RWLockRead lock(params.library->get_baked_data_rw_lock());
		const VoxelBlockyLibraryBase::BakedData &library_baked_data = params.library->get_baked_data();

                RWLockRead side_lock(side_params.library->get_baked_data_rw_lock());
		const VoxelSideLibrary::BakedData &side_library_baked_data = side_params.library->get_baked_data();

		if (arrays_per_material.size() < material_count) {
			arrays_per_material.resize(material_count);
		}

		switch (channel_depth) {
			case VoxelBufferInternal::DEPTH_8_BIT:
				generate_blocky_side_mesh(arrays_per_material, collision_surface, voxels, raw_channel, block_size,
						side_library_baked_data, library_baked_data, params.bake_occlusion, baked_occlusion_darkness);
				break;

			case VoxelBufferInternal::DEPTH_16_BIT:
				generate_blocky_side_mesh(arrays_per_material, collision_surface, voxels,
						raw_channel.reinterpret_cast_to<uint16_t>(), block_size, side_library_baked_data, library_baked_data,
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

#ifdef TOOLS_ENABLED

void VoxelMesherBlockySide::get_configuration_warnings(PackedStringArray &out_warnings) const {
	Ref<VoxelBlockyLibraryBase> library = get_library();

	if (library.is_null()) {
		out_warnings.append(String(ZN_TTR("{0} has no {1} assigned."))
									.format(varray(VoxelMesherBlocky::get_class_static(),
											VoxelBlockyLibraryBase::get_class_static())));
		return;
	}

	Ref<VoxelSideLibrary> side_library = get_side_library();

	if (side_library.is_null()) {
		out_warnings.append(String(ZN_TTR("{0} has no {1} assigned."))
									.format(varray(VoxelMesherBlocky::get_class_static(),
											VoxelSideLibrary::get_class_static())));
		return;
	}

	const VoxelBlockyLibraryBase::BakedData &baked_data = library->get_baked_data();
	RWLockRead rlock(library->get_baked_data_rw_lock());

	if (baked_data.models.size() == 0) {
		out_warnings.append(String(ZN_TTR("The {0} assigned to {1} has no baked models."))
									.format(varray(library->get_class(), VoxelMesherBlocky::get_class_static())));
		return;
	}

	const VoxelSideLibrary::BakedData &side_baked_data = side_library->get_baked_data();
	RWLockRead side_rlock(side_library->get_baked_data_rw_lock());

	if (side_baked_data.models.size() == 0) {
		out_warnings.append(String(ZN_TTR("The {0} assigned to {1} has no baked models."))
									.format(varray(side_library->get_class(), VoxelMesherBlocky::get_class_static())));
		return;
	}

	library->get_configuration_warnings(out_warnings);

	side_library->get_configuration_warnings(out_warnings);
}

#endif // TOOLS_ENABLED

void VoxelMesherBlockySide::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_side_library", "side_library"), &VoxelMesherBlockySide::set_side_library);
	ClassDB::bind_method(D_METHOD("get_side_library"), &VoxelMesherBlockySide::get_side_library);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "side_library", PROPERTY_HINT_RESOURCE_TYPE,
						 VoxelSideLibrary::get_class_static(), PROPERTY_USAGE_DEFAULT
						 // Sadly we can't use this hint because the property type is abstract... can't just choose a
						 // default child class. This hint becomes less and less useful everytime I come across it...
						 //| PROPERTY_USAGE_EDITOR_INSTANTIATE_OBJECT
						 ),
			"set_side_library", "get_side_library");
}

} // namespace zylann::voxel
