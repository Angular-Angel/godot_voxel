#include "voxel_side_library.h"
#include "../../constants/voxel_string_names.h"
#include "../../util/math/triangle.h"
#include "../../util/profiling.h"
#include "../../util/godot/classes/time.h"
#include "../../util/godot/core/string.h"
#include "../../util/string_funcs.h"
#include <bitset>

namespace zylann::voxel {

VoxelSideLibrary::VoxelSideLibrary() {}

VoxelSideLibrary::~VoxelSideLibrary() {}

void VoxelSideLibrary::clear() {
	_side_models.clear();
}

void VoxelSideLibrary::load_default() {
	clear();
}

void VoxelSideLibrary::bake() {
	ZN_PROFILE_SCOPE();

	RWLockWrite lock(_baked_data_rw_lock);

	const uint64_t time_before = Time::get_singleton()->get_ticks_usec();

	// This is the only place we modify the data.

	_indexed_materials.clear();
	VoxelSideModel::MaterialIndexer materials{ _indexed_materials };

	_baked_data.models.resize(_side_models.size());
	for (size_t i = 0; i < _side_models.size(); ++i) {
		Ref<VoxelSideModel> config = _side_models[i];
		if (config.is_valid()) {
			config->bake(_baked_data.models[i], materials);
		} else {
			_baked_data.models[i].clear();
		}
	}

	_baked_data.indexed_materials_count = _indexed_materials.size();

	uint64_t time_spent = Time::get_singleton()->get_ticks_usec() - time_before;
	ZN_PRINT_VERBOSE(
			format("Took {} us to bake SideLibrary, indexed {} materials", time_spent, _indexed_materials.size()));

	_needs_baking = false;
}

int VoxelSideLibrary::get_model_index_from_resource_name(String resource_name) const {
	for (unsigned int i = 0; i < _side_models.size(); ++i) {
		const Ref<VoxelSideModel> &model = _side_models[i];
		if (model.is_valid() && model->get_name() == resource_name) {
			return i;
		}
	}
	return -1;
}

int VoxelSideLibrary::add_model(Ref<VoxelSideModel> model) {
	const int index = _side_models.size();
	_side_models.push_back(model);
	return index;
}

bool VoxelSideLibrary::_set(const StringName &p_name, const Variant &p_value) {
	return false;
}

#ifdef TOOLS_ENABLED

void VoxelSideLibrary::get_configuration_warnings(PackedStringArray &out_warnings) const {
	std::vector<int> null_indices;

	bool has_visible_model = false;
	for (unsigned int i = 0; i < _side_models.size() && !has_visible_model; ++i) {
		Ref<VoxelSideModel> model = _side_models[i];
		if (model.is_null()) {
			null_indices.push_back(i);
			continue;
		}
		if (!model->is_empty()) {
			has_visible_model = true;
			break;
		}
	}
	if (!has_visible_model) {
		out_warnings.append(
				String(ZN_TTR("The {0} only has empty {2}s."))
						.format(varray(VoxelSideLibrary::get_class_static(), VoxelSideModel::get_class_static())));
	}

	if (null_indices.size() > 0) {
		const String indices_str = join_comma_separated<int>(to_span(null_indices));
		// Should we really consider it a problem?
		out_warnings.append(String(ZN_TTR("The {0} has null model entries: {2}"))
									.format(varray(VoxelSideLibrary::get_class_static(), indices_str)));
	}
}

#endif

Ref<VoxelSideModel> VoxelSideLibrary::_b_get_model(unsigned int id) const {
	ERR_FAIL_COND_V(id >= _side_models.size(), Ref<VoxelSideModel>());
	return _side_models[id];
}

TypedArray<VoxelSideModel> VoxelSideLibrary::_b_get_models() const {
	TypedArray<VoxelSideModel> array;
	array.resize(_side_models.size());
	for (size_t i = 0; i < _side_models.size(); ++i) {
		array[i] = _side_models[i];
	}
	return array;
}

void VoxelSideLibrary::_b_set_models(TypedArray<VoxelSideModel> models) {
	const size_t prev_size = _side_models.size();
	_side_models.resize(models.size());
	for (int i = 0; i < models.size(); ++i) {
		_side_models[i] = models[i];
	}
	_needs_baking = (_side_models.size() != prev_size);
}

TypedArray<Material> VoxelSideLibrary::_b_get_materials() const {
	// Note, if at least one non-empty side has no material, there will be one null entry in this list to represent
	// "The default material".
	TypedArray<Material> materials;
	materials.resize(_indexed_materials.size());
	for (size_t i = 0; i < _indexed_materials.size(); ++i) {
		materials[i] = _indexed_materials[i];
	}
	return materials;
}

void VoxelSideLibrary::_b_bake() {
	bake();
}

Ref<Material> VoxelSideLibrary::get_material_by_index(unsigned int index) const {
	ERR_FAIL_INDEX_V(index, _indexed_materials.size(), Ref<Material>());
	return _indexed_materials[index];
}

void VoxelSideLibrary::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_models"), &VoxelSideLibrary::_b_get_models);
	ClassDB::bind_method(D_METHOD("set_models"), &VoxelSideLibrary::_b_set_models);

	ClassDB::bind_method(D_METHOD("add_model"), &VoxelSideLibrary::add_model);

	ClassDB::bind_method(D_METHOD("get_model", "index"), &VoxelSideLibrary::_b_get_model);
	ClassDB::bind_method(D_METHOD("get_model_index_from_resource_name", "name"),
			&VoxelSideLibrary::get_model_index_from_resource_name);

	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "models", PROPERTY_HINT_ARRAY_TYPE,
						 MAKE_RESOURCE_TYPE_HINT(VoxelSideModel::get_class_static())),
			"set_models", "get_models");

	ClassDB::bind_method(D_METHOD("get_materials"), &VoxelSideLibrary::_b_get_materials);

	ClassDB::bind_method(D_METHOD("bake"), &VoxelSideLibrary::_b_bake);

	BIND_CONSTANT(MAX_MODELS);
}

} // namespace zylann::voxel
