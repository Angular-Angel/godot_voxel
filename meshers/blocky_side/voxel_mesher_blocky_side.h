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

	// TODO GDX: Resource::duplicate() cannot be overriden (while it can in modules).
	// This will lead to performance degradation and maybe unexpected behavior
#if defined(ZN_GODOT)
	Ref<Resource> duplicate(bool p_subresources = false) const override;
#elif defined(ZN_GODOT_EXTENSION)
	Ref<Resource> duplicate(bool p_subresources = false) const;
#endif

	Ref<Material> get_material_by_index(unsigned int index) const override;

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
