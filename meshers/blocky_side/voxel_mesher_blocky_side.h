#ifndef VOXEL_MESHER_BLOCKY_SIDE_H
#define VOXEL_MESHER_BLOCKY_SIDE_H

#include "../../util/godot/classes/mesh.h"
#include "../../util/thread/rw_lock.h"
#include "../voxel_mesher.h"
#include "../blocky/voxel_mesher_blocky.h"

#include <vector>

namespace zylann::voxel {

// Interprets voxel values as indexes to models in a VoxelBlockyLibrary, and batches them together.
// Overlapping faces are removed from the final mesh.
class VoxelMesherBlockySide : public VoxelMesherBlocky {
	GDCLASS(VoxelMesherBlockySide, VoxelMesherBlocky)

public:
	VoxelMesherBlockySide();
	~VoxelMesherBlockySide();

protected:
	static void _bind_methods();
};

} // namespace zylann::voxel

#endif // VOXEL_MESHER_BLOCKY_SIDE_H
