#ifndef VOXEL_SIDE_LIBRARY_BASE_H
#define VOXEL_SIDE_LIBRARY_BASE_H

#include "../../util/dynamic_bitset.h"
#include "../../util/godot/classes/resource.h"
#include "../../util/thread/rw_lock.h"
#include "voxel_side_model.h"

namespace zylann::voxel {

// Base class for libraries that can be used with VoxelMesherBlocky.
// A library provides a set of pre-processed models that can be efficiently batched into a voxel mesh.
// Depending on the type of library, these models are provided differently.
class VoxelSideLibrary : public Resource {
	GDCLASS(VoxelSideLibrary, Resource)

public:
	VoxelSideLibrary();
	~VoxelSideLibrary();

	// Limit based on maximum supported by VoxelMesherBlocky
	static const unsigned int MAX_MODELS = 65536;
	static const uint32_t NULL_INDEX = 0xFFFFFFFF;

	struct BakedData {
		// Lots of data can get moved but it's only on load.
		std::vector<VoxelSideModel::BakedData> models;

		// struct VariantInfo {
		// 	uint16_t type_index;
		// 	FixedArray<uint8_t, 4> attributes;
		// };

		// std::vector<VariantInfo> variant_infos;

		unsigned int indexed_materials_count = 0;

		inline bool has_model(uint32_t i) const {
			return i < models.size();
		}
	};

	void load_default();
	void clear();

	void bake();

	int get_model_index_from_resource_name(String resource_name) const;

	// Convenience method that returns the index of the added model
	int add_model(Ref<VoxelSideModel> model);

	const BakedData &get_baked_data() const {
		return _baked_data;
	}
	const RWLock &get_baked_data_rw_lock() const {
		return _baked_data_rw_lock;
	}

	Ref<Material> get_material_by_index(unsigned int index) const;

#ifdef TOOLS_ENABLED
	virtual void get_configuration_warnings(PackedStringArray &out_warnings) const;
#endif

private:
	bool _needs_baking = true;

	Ref<VoxelSideModel> _b_get_model(unsigned int id) const;

	TypedArray<VoxelSideModel> _b_get_models() const;
	void _b_set_models(TypedArray<VoxelSideModel> models);

	// Convenience method to get all indexed materials after baking,
	// which can be passed to VoxelMesher::build for testing
	TypedArray<Material> _b_get_materials() const;

	void _b_bake();

	bool _set(const StringName &p_name, const Variant &p_value);

	static void _bind_methods();

	std::vector<Ref<VoxelSideModel>> _side_models;

	RWLock _baked_data_rw_lock;
	BakedData _baked_data;
	// One of the entries can be null to represent "The default material". If all non-empty models have materials, there
	// won't be a null entry.
	std::vector<Ref<Material>> _indexed_materials;
};

} // namespace zylann::voxel

#endif // VOXEL_SIDE_LIBRARY_H
