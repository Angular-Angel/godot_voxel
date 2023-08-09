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
                unsigned int side, const VoxelBlockyModel::BakedData &voxel, const Vector3f &pos,
                const VoxelBlockyModel::BakedData::Surface &surface) {
        VoxelMesherBlocky::generate_side_surface(out_arrays_per_material, collision_surface,
                        bake_occlusion, baked_occlusion_darkness, shaded_corner, index_offsets,
                        collision_surface_index_offset, side, voxel, pos, surface);

        
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
