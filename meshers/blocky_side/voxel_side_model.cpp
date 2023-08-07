#include "voxel_side_model.h"
#include "../../util/godot/classes/array_mesh.h"
#include "../../util/godot/classes/base_material_3d.h"
#include "../../util/godot/classes/shader_material.h"
#include "../../util/godot/core/array.h"
#include "../../util/godot/core/string.h"
#include "../../util/godot/funcs.h"
#include "../../util/macros.h"
#include "../../util/math/conv.h"
#include "../../util/string_funcs.h"

namespace zylann::voxel {

VoxelSideModel::VoxelSideModel() {}

bool VoxelSideModel::_set(const StringName &p_name, const Variant &p_value) {
	return false;
}

bool VoxelSideModel::_get(const StringName &p_name, Variant &r_ret) const {
	return false;
}

void VoxelSideModel::_get_property_list(List<PropertyInfo> *p_list) const {
}

void VoxelSideModel::set_material(Ref<Material> material) {
	_material = material;
	emit_changed();
}

Ref<Material> VoxelSideModel::get_material() const {
	return _material;
}

void VoxelSideModel::bake(BakedData &baked_data, MaterialIndexer &materials) const {
        const unsigned int material_index = materials.get_or_create_index(get_material());
        baked_data.material_id = material_index;
}

bool VoxelSideModel::is_empty() const {
	return get_material().is_null();
}

void VoxelSideModel::copy_base_properties_from(const VoxelSideModel &src) {
	_material = src._material;
}

void VoxelSideModel::_bind_methods() {

	ClassDB::bind_method(
			D_METHOD("set_material", "material"), &VoxelSideModel::set_material);
	ClassDB::bind_method(D_METHOD("get_material"), &VoxelSideModel::get_material);

        ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "material", PROPERTY_HINT_RESOURCE_TYPE,
						 String("{0},{1}").format(
								 varray(BaseMaterial3D::get_class_static(), ShaderMaterial::get_class_static()))),
			"set_material", "get_material");

	BIND_ENUM_CONSTANT(SIDE_NEGATIVE_X);
	BIND_ENUM_CONSTANT(SIDE_POSITIVE_X);
	BIND_ENUM_CONSTANT(SIDE_NEGATIVE_Y);
	BIND_ENUM_CONSTANT(SIDE_POSITIVE_Y);
	BIND_ENUM_CONSTANT(SIDE_NEGATIVE_Z);
	BIND_ENUM_CONSTANT(SIDE_POSITIVE_Z);
	BIND_ENUM_CONSTANT(SIDE_COUNT);
}

} // namespace zylann::voxel
