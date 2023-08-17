#ifndef VOXEL_MESHER_BLOCKY_SIDE_H
#define VOXEL_MESHER_BLOCKY_SIDE_H

#include "../../util/godot/classes/mesh.h"
#include "../../util/thread/rw_lock.h"
#include "../voxel_mesher.h"
#include "../blocky/voxel_mesher_blocky.h"
#include "voxel_side_library.h"

#include <vector>

namespace zylann::voxel {

// Interprets voxel values as indexes to models in a VoxelBlockyLibrary, and batches them together.
// Overlapping faces are removed from the final mesh.
class VoxelMesherBlockySide : public VoxelMesherBlocky {
	GDCLASS(VoxelMesherBlockySide, VoxelMesherBlocky)

public:
	VoxelMesherBlockySide();
	~VoxelMesherBlockySide();

	void set_side_library(Ref<VoxelSideLibrary> library);
	Ref<VoxelSideLibrary> get_side_library() const;

	void build(VoxelMesher::Output &output, const VoxelMesher::Input &input) override;

	// TODO GDX: Resource::duplicate() cannot be overriden (while it can in modules).
	// This will lead to performance degradation and maybe unexpected behavior
#if defined(ZN_GODOT)
	Ref<Resource> duplicate(bool p_subresources = false) const override;
#elif defined(ZN_GODOT_EXTENSION)
	Ref<Resource> duplicate(bool p_subresources = false) const;
#endif

        unsigned int get_material_count() const override;

	Ref<Material> get_material_by_index(unsigned int index) const override;

        template <typename Type_T>
        void shade_corners(const Span<Type_T> type_buffer, const VoxelBlockyLibraryBase::BakedData &library,
                VoxelMesherBlocky::NeighborLUTs &neighbor_luts, unsigned int side,
                const int voxel_index, int shaded_corner[]);

        void generate_side_surface(std::vector<VoxelMesherBlocky::Arrays> &out_arrays_per_material,
		VoxelMesher::Output::CollisionSurface *collision_surface,
		bool bake_occlusion, float baked_occlusion_darkness, int shaded_corner[],
                std::vector<int> &index_offsets, int &collision_surface_index_offset,
                unsigned int side, const VoxelBlockyModel::BakedData &voxel,
                const VoxelSideModel::BakedData &side_model, const Vector3f &pos,
                const VoxelBlockyModel::BakedData::Surface &surface);

        template <typename Type_T>
        void generate_side_mesh(std::vector<VoxelMesherBlocky::Arrays> &out_arrays_per_material,
		VoxelMesher::Output::CollisionSurface *collision_surface, const Span<Type_T> type_buffer,
		const VoxelBlockyLibraryBase::BakedData &library, bool bake_occlusion, float baked_occlusion_darkness,
                std::vector<int> &index_offsets, int &collision_surface_index_offset, 
                NeighborLUTs &neighbor_luts, unsigned int x, unsigned int y, unsigned int z, unsigned int side,
                const int voxel_index, const VoxelBlockyModel::BakedData &voxel,
                const VoxelBlockyModel::BakedData::Model &model);

        template <typename Type_T>
        void generate_side_mesh(std::vector<VoxelMesherBlocky::Arrays> &out_arrays_per_material,
		VoxelMesher::Output::CollisionSurface *collision_surface, const Span<Type_T> type_buffer,
		const VoxelBlockyLibraryBase::BakedData &library, bool bake_occlusion, float baked_occlusion_darkness,
                std::vector<int> &index_offsets, int &collision_surface_index_offset, 
                NeighborLUTs &neighbor_luts, unsigned int x, unsigned int y, unsigned int z, unsigned int side,
                const int voxel_index, const VoxelBlockyModel::BakedData &voxel, 
                const VoxelSideModel::BakedData &side_model, const VoxelBlockyModel::BakedData::Model &voxel_model);

        template <typename Type_T>
        void generate_voxel_mesh(std::vector<VoxelMesherBlocky::Arrays> &out_arrays_per_material,
		VoxelMesher::Output::CollisionSurface *collision_surface, 
                std::vector<int> &index_offsets, int &collision_surface_index_offset,
                VoxelMesherBlocky::NeighborLUTs &neighbor_luts, const VoxelBufferInternal &voxels, const Span<Type_T> type_buffer, 
                const VoxelSideLibrary::BakedData &side_library, const VoxelBlockyLibraryBase::BakedData &voxel_library,
                bool bake_occlusion, float baked_occlusion_darkness, const int voxel_index,
                unsigned int x, unsigned int y, unsigned int z);

        template <typename Type_T>
        void generate_blocky_side_mesh(std::vector<VoxelMesherBlocky::Arrays> &out_arrays_per_material,
		VoxelMesher::Output::CollisionSurface *collision_surface, const VoxelBufferInternal &voxels,
                const Span<Type_T> type_buffer, const Vector3i block_size, const VoxelSideLibrary::BakedData &side_library,
                const VoxelBlockyLibraryBase::BakedData &voxel_library, bool bake_occlusion, float baked_occlusion_darkness);

#ifdef TOOLS_ENABLED
	void get_configuration_warnings(PackedStringArray &out_warnings) const override;
#endif

protected:
	struct SideParameters {
		Ref<VoxelSideLibrary> library;
	};

	// Parameters
	SideParameters _side_parameters;
	RWLock _side_parameters_lock;

	static void _bind_methods();
};

} // namespace zylann::voxel

#endif // VOXEL_MESHER_BLOCKY_SIDE_H
