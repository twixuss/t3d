#pragma once
#include "common.h"
#include <t3d/component.h>
#include <tl/quaternion.h>
#include <tl/masked_block_list.h>

using EntityFlags = u32;
enum : EntityFlags {
	Entity_editor_camera = 0x1,
	Entity_editor_mask   = 0x2 - 1,
};

struct Entity {
	struct Scene *scene;
	v3f position = {};
	quaternion rotation = quaternion::identity();
	v3f scale = {1, 1, 1};
	EntityFlags flags;
	StaticList<ComponentIndex, 16> components;
	List<utf8> name;

	forceinline v3f   right() { return rotation * v3f{1,0,0}; }
	forceinline v3f      up() { return rotation * v3f{0,1,0}; }
	forceinline v3f    back() { return rotation * v3f{0,0,1}; }
	forceinline v3f    left() { return rotation * v3f{-1,0,0}; }
	forceinline v3f    down() { return rotation * v3f{0,-1,0}; }
	forceinline v3f forward() { return rotation * v3f{0,0,-1}; }
};

inline bool is_editor_entity(Entity &entity) {
	return entity.flags & Entity_editor_mask;
}

void destroy_entity(Entity &entity);

u32 get_entity_index(Entity &entity);


void *add_component(Entity &entity, u32 entity_index, Uid component_type_uid);
void remove_component(Entity &entity, ComponentIndex *component);

inline void remove_component(Entity &entity, ComponentIndex component) {
	remove_component(entity, find(entity.components, component));
}

inline void *add_component(Entity &entity, Uid component_uid) {
	return add_component(entity, get_entity_index(entity), component_uid);
}


template <class T>
T &add_component(Entity &entity, u32 entity_index) {
	return *(T *)add_component(entity, entity_index, component_name_to_uid(T::_t3d_component_name));
}
template <class T>
T &add_component(Entity &entity) {
	return add_component<T>(entity, get_entity_index(entity));
}


template <class T>
T *get_component(Entity &entity, u32 nth = 0) {
	static constexpr u32 component_type = get_component_type_index<T>();
	for (auto component : entity.components) {
		if (component.type == component_type) {
			if (nth == 0) {
				return get_component_info(component_type).storage.get(component.index);
			}
			--nth;
		}
	}
	return 0;
}
