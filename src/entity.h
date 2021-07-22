#pragma once
#include <t3d.h>
#include <tl/quaternion.h>

struct Entity {
	v3f position = {};
	quaternion rotation = quaternion::identity();
	v3f scale = {1, 1, 1};
	StaticList<ComponentIndex, 16> components;
	List<utf8> debug_name;
};

MaskedBlockList<Entity, 256> entities;

Entity &Component::entity() const {
	return entities[entity_index];
}

void destroy(Entity &entity) {
	for (auto &component_index : entity.components) {
		auto &storage = component_storages[component_index.type];
		storage.remove_at(component_index.index);
	}
	free(entity.debug_name);
	entities.remove(entity);
}

template <class T>
T &add_component(Entity &entity, u32 entity_index) {
	static constexpr u32 component_type = get_component_type_index<T>();

	auto added = component_storages[component_type].add();
	T &component = construct(*(T *)added.pointer);

	ComponentIndex component_index = {
		.type = component_type,
		.index = added.index,
		.entity_index = entity_index,
	};
	entity.components.add(component_index);

	component.entity_index = entity_index;
	on_create<T>(component);

	return component;
}
template <class T>
T &add_component(Entity &entity) {
	auto found = index_of(entities, &entity);
	assert(found);
	return add_component<T>(entity, found.value);
}
template <class T>
T &add_component(u32 entity_index) {
	return add_component<T>(entities[entity_index], entity_index);
}


template <class T>
T *get_component(Entity &entity, u32 nth = 0) {
	static constexpr u32 component_type = get_component_type_index<T>();
	for (auto component : entity.components) {
		if (component.type == component_type) {
			return component_storages[component_type].get(component.index);
		}
	}
}
template <class T>
T *get_component(u32 entity_index, u32 nth = 0) {
	return get_component<T>(entities[entity_index], nth);
}
