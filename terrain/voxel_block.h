#ifndef VOXEL_BLOCK_H
#define VOXEL_BLOCK_H

#include "../voxel_buffer.h"

#include <scene/3d/mesh_instance.h>
#include <scene/3d/physics_body.h>

// Internal structure holding a reference to mesh visuals, physics and a block of voxel data.
class VoxelBlock {
public:
	enum MeshState {
		MESH_NEVER_UPDATED = 0,
		MESH_UP_TO_DATE,
		MESH_UPDATE_NOT_SENT, // The mesh is out of date, but no update request have been sent
		MESH_UPDATE_SENT // The mesh is out of date, and an update request is pending
	};

	Ref<VoxelBuffer> voxels;
	Vector3i position;
	unsigned int lod_index = 0;

	static VoxelBlock *create(Vector3i bpos, Ref<VoxelBuffer> buffer, unsigned int size, unsigned int p_lod_index);

	~VoxelBlock();

	void set_mesh(Ref<Mesh> mesh, Ref<World> world);
	bool has_mesh() const;

	void set_mesh_state(MeshState ms);
	MeshState get_mesh_state() const;

	void mark_been_meshed();
	bool has_been_meshed() const;

	void enter_world(World *world);
	void exit_world();
	void set_visible(bool visible);
	bool is_visible() const;

	inline bool is_mesh_update_scheduled() {
		return _mesh_state == MESH_UPDATE_NOT_SENT || _mesh_state == MESH_UPDATE_SENT;
	}

private:
	VoxelBlock();

	Vector3i _position_in_voxels;

	Ref<Mesh> _mesh;
	RID _mesh_instance;
	int _mesh_update_count = 0;
	bool _visible = true;

	MeshState _mesh_state = MESH_NEVER_UPDATED;

	// The mesh might be null, but we don't know if it's actually empty or if it's loading.
	// This boolean tells if we attempted to mesh this block at least once.
	bool _has_been_meshed = false;
};

#endif // VOXEL_BLOCK_H
