#pragma once
#include "common.h"
#include <tl/quaternion.h>

using EntityFlags = u32;
enum : EntityFlags {
	Entity_editor_camera = 0x1,
	Entity_editor_mask   = 0x2 - 1,
};

struct Entity {
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

bool is_editor_entity(Entity &entity) {
	return entity.flags & Entity_editor_mask;
}

MaskedBlockList<Entity, 256> entities;

Entity &Component::entity() const {
	return entities[entity_index];
}

enum OwnershipFlags {
	Ownership_copy     = 0x0,
	Ownership_transfer = 0x1,
};

Entity &create_entity(List<utf8> name, OwnershipFlags flags = Ownership_copy) {
	auto &result = entities.add();
	if (flags & Ownership_transfer) {
		result.name = name;
	} else {
		result.name = copy(name);
	}
	return result;
}
Entity &create_entity(Span<utf8> name = u8"unnamed"s) {
	return create_entity(as_list(name));
}
Entity &create_entity(char const *name) {
	return create_entity((Span<utf8>)as_span(name));
}
template <class ...Args>
Entity &create_entity(utf8 const *fmt, Args const &...args) {
	return create_entity(format(fmt, args...), Ownership_transfer);
}
template <class ...Args>
Entity &create_entity(char const *fmt, Args const &...args) {
	return create_entity((utf8 *)fmt, args...);
}

void destroy(Entity &entity) {
	for (auto &component_index : entity.components) {
		auto &storage = component_storages[component_index.type];
		component_info[component_index.type].free(storage.get(component_index.index));
		storage.remove_at(component_index.index);
	}
	free(entity.name);
	entities.remove(entity);
}

template <class T>
T &add_component(Entity &entity, u32 entity_index) {
	static constexpr u32 component_type = get_component_type_index<T>();

	auto added = component_storages[component_type].add();

	ComponentIndex component_index = {
		.type = component_type,
		.index = added.index,
		.entity_index = entity_index,
	};
	entity.components.add(component_index);

	T &component = construct(*(T *)added.pointer);
	component.entity_index = entity_index;
	if constexpr (is_statically_overridden(init, T, ::Component)) {
		component.init();
	}

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
