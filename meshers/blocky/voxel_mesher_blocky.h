#ifndef VOXEL_MESHER_BLOCKY_H
#define VOXEL_MESHER_BLOCKY_H

#include "../../util/godot/classes/mesh.h"
#include "../../util/thread/rw_lock.h"
#include "../voxel_mesher.h"
#include "voxel_blocky_library_base.h"

#include <vector>

namespace zylann::voxel {

// Interprets voxel values as indexes to models in a VoxelBlockyLibrary, and batches them together.
// Overlapping faces are removed from the final mesh.
class VoxelMesherBlocky : public VoxelMesher {
	GDCLASS(VoxelMesherBlocky, VoxelMesher)

public:
	static const int PADDING = 1;

	VoxelMesherBlocky();
	~VoxelMesherBlocky();

	void set_library(Ref<VoxelBlockyLibraryBase> library);
	Ref<VoxelBlockyLibraryBase> get_library() const;

	void set_occlusion_darkness(float darkness);
	float get_occlusion_darkness() const;

	void set_occlusion_enabled(bool enable);
	bool get_occlusion_enabled() const;

        virtual unsigned int get_material_count() const;

	void build(VoxelMesher::Output &output, const VoxelMesher::Input &input) override;

	// TODO GDX: Resource::duplicate() cannot be overriden (while it can in modules).
	// This will lead to performance degradation and maybe unexpected behavior
#if defined(ZN_GODOT)
	Ref<Resource> duplicate(bool p_subresources = false) const override;
#elif defined(ZN_GODOT_EXTENSION)
	Ref<Resource> duplicate(bool p_subresources = false) const;
#endif

	int get_used_channels_mask() const override;

	bool supports_lod() const override {
		return false;
	}

	Ref<Material> get_material_by_index(unsigned int index) const override;

	// Using std::vector because they make this mesher twice as fast than Godot Vectors.
	// See why: https://github.com/godotengine/godot/issues/24731
	struct Arrays {
		std::vector<Vector3f> positions;
		std::vector<Vector3f> normals;
		std::vector<Vector2f> uvs;
		std::vector<Color> colors;
		std::vector<int> indices;
		std::vector<float> tangents;

		void clear() {
			positions.clear();
			normals.clear();
			uvs.clear();
			colors.clear();
			indices.clear();
			tangents.clear();
		}
	};

        struct NeighborLUTs {
                FixedArray<int, Cube::SIDE_COUNT> sides;
                FixedArray<int, Cube::EDGE_COUNT> edges;
                FixedArray<int, Cube::CORNER_COUNT> corners;
        };

        NeighborLUTs generate_neighbor_luts(const int row_size, const int deck_size);

        template <typename Type_T>
        void shade_corners(const Span<Type_T> type_buffer, const VoxelBlockyLibraryBase::BakedData &library,
                VoxelMesherBlocky::NeighborLUTs &neighbor_luts, unsigned int side,
                const int voxel_index, int shaded_corner[]);

        void generate_side_collision_surface(VoxelMesher::Output::CollisionSurface *collision_surface,
                const unsigned int vertex_count, int &collision_surface_index_offset,
                const Vector3f &pos, const std::vector<Vector3f> &side_positions,
                const std::vector<int> &side_indices, const unsigned int index_count);

        void append_side_positions(VoxelMesherBlocky::Arrays &arrays, const Vector3f &pos,
                const std::vector<Vector3f> &side_positions, const unsigned int vertex_count);

        void append_side_uvs(VoxelMesherBlocky::Arrays &arrays, const std::vector<Vector2f> &side_uvs,
                const unsigned int vertex_count);

        void append_tangents(VoxelMesherBlocky::Arrays &arrays, const std::vector<float> &side_tangents,
                const unsigned int vertex_count);

        void append_side_normals(VoxelMesherBlocky::Arrays &arrays, const unsigned int vertex_count, unsigned int side);

        void append_side_colors(VoxelMesherBlocky::Arrays &arrays, const unsigned int vertex_count, unsigned int side,
                const std::vector<Vector3f> &side_positions, bool bake_occlusion, float baked_occlusion_darkness,
                const Color modulate_color, int shaded_corner[]);

        void append_side_indices(VoxelMesherBlocky::Arrays &arrays, int &index_offset, 
                const std::vector<int> &side_indices, const unsigned int index_count);

        void generate_side_surface(std::vector<VoxelMesherBlocky::Arrays> &out_arrays_per_material,
		VoxelMesher::Output::CollisionSurface *collision_surface,
		bool bake_occlusion, float baked_occlusion_darkness, int shaded_corner[],
                std::vector<int> &index_offsets, int &collision_surface_index_offset,
                unsigned int side, const VoxelBlockyModel::BakedData &voxel, const Vector3f &pos,
                const VoxelBlockyModel::BakedData::Surface &surface);

        template <typename Type_T>
        void generate_side_mesh(std::vector<VoxelMesherBlocky::Arrays> &out_arrays_per_material,
		VoxelMesher::Output::CollisionSurface *collision_surface, const Span<Type_T> type_buffer,
		const VoxelBlockyLibraryBase::BakedData &library, bool bake_occlusion, float baked_occlusion_darkness,
                std::vector<int> &index_offsets, int &collision_surface_index_offset, 
                NeighborLUTs &neighbor_luts, unsigned int x, unsigned int y, unsigned int z, unsigned int side,
                const int voxel_index, const VoxelBlockyModel::BakedData &voxel,
                const VoxelBlockyModel::BakedData::Model &model);

        void generate_inside_mesh(std::vector<VoxelMesherBlocky::Arrays> &out_arrays_per_material,
		VoxelMesher::Output::CollisionSurface *collision_surface,
                std::vector<int> &index_offsets, int &collision_surface_index_offset,
                unsigned int x, unsigned int y, unsigned int z, const VoxelBlockyModel::BakedData &voxel,
                const VoxelBlockyModel::BakedData::Surface &surface);

        template <typename Type_T>
        void generate_voxel_mesh(std::vector<VoxelMesherBlocky::Arrays> &out_arrays_per_material,
		VoxelMesher::Output::CollisionSurface *collision_surface, 
                std::vector<int> &index_offsets, int &collision_surface_index_offset,
                NeighborLUTs &neighbor_luts, const Span<Type_T> type_buffer, 
                const VoxelBlockyLibraryBase::BakedData &library, bool bake_occlusion,
                float baked_occlusion_darkness, const int voxel_index,
                unsigned int x, unsigned int y, unsigned int z);

        template <typename Type_T>
        void generate_blocky_mesh(std::vector<VoxelMesherBlocky::Arrays> &out_arrays_per_material,
		VoxelMesher::Output::CollisionSurface *collision_surface, const Span<Type_T> type_buffer,
		const Vector3i block_size, const VoxelBlockyLibraryBase::BakedData &library, bool bake_occlusion,
		float baked_occlusion_darkness);

#ifdef TOOLS_ENABLED
	void get_configuration_warnings(PackedStringArray &out_warnings) const override;
#endif

	bool is_generating_collision_surface() const override {
		return true;
	}

protected:
	struct Parameters {
		float baked_occlusion_darkness = 0.8;
		bool bake_occlusion = true;
		Ref<VoxelBlockyLibraryBase> library;
	};

	struct Cache {
		std::vector<Arrays> arrays_per_material;
	};

	// Parameters
	Parameters _parameters;
	RWLock _parameters_lock;

	// Work cache
	static Cache &get_tls_cache();

	static void _bind_methods();
};

} // namespace zylann::voxel

#endif // VOXEL_MESHER_BLOCKY_H
