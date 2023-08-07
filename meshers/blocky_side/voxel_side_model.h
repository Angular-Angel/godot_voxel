#ifndef VOXEL_SIDE_MODEL_H
#define VOXEL_SIDE_MODEL_H

#include "../../constants/cube_tables.h"
#include "../../util/fixed_array.h"
#include "../../util/godot/classes/material.h"
#include "../../util/godot/classes/mesh.h"
#include "../../util/macros.h"
#include "../../util/math/ortho_basis.h"
#include "../../util/math/vector2f.h"
#include "../../util/math/vector3f.h"

#include <vector>

namespace zylann::voxel {


// Material corresponding to a specific side value/state, for use with `VoxelMesherBlockySide`.
class VoxelSideModel : public Resource {
	GDCLASS(VoxelSideModel, Resource)

public:
	// Convention to mean "nothing".
	// Don't assign a non-empty model at this index.
	static const uint16_t EMPTY_ID = 0;

        
	struct BakedData {

                uint32_t material_id = 0;

		inline void clear() {
			material_id = 0;
		}
	};

	VoxelSideModel();

	enum Side {
		SIDE_NEGATIVE_X = Cube::SIDE_NEGATIVE_X,
		SIDE_POSITIVE_X = Cube::SIDE_POSITIVE_X,
		SIDE_NEGATIVE_Y = Cube::SIDE_NEGATIVE_Y,
		SIDE_POSITIVE_Y = Cube::SIDE_POSITIVE_Y,
		SIDE_NEGATIVE_Z = Cube::SIDE_NEGATIVE_Z,
		SIDE_POSITIVE_Z = Cube::SIDE_POSITIVE_Z,
		SIDE_COUNT = Cube::SIDE_COUNT
	};

	void set_material(Ref<Material> material);
	Ref<Material> get_material() const;

	void copy_base_properties_from(const VoxelSideModel &src);

        bool is_empty() const;

	struct MaterialIndexer {
		std::vector<Ref<Material>> &materials;

		unsigned int get_or_create_index(const Ref<Material> &p_material) {
			for (size_t i = 0; i < materials.size(); ++i) {
				const Ref<Material> &material = materials[i];
				if (material == p_material) {
					return i;
				}
			}
			const unsigned int ret = materials.size();
			materials.push_back(p_material);
			return ret;
		}
	};

        void bake(BakedData &baked_data, MaterialIndexer &materials) const;

protected:
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;

private:
	static void _bind_methods();

        Ref<Material> _material;
};

} // namespace zylann::voxel

VARIANT_ENUM_CAST(zylann::voxel::VoxelSideModel::Side)

#endif // VOXEL_SIDE_MODEL_H
