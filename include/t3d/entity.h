#pragma once
#include "common.h"
#include <tl/quaternion.h>
#include <tl/masked_block_list.h>

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

inline bool is_editor_entity(Entity &entity) {
	return entity.flags & Entity_editor_mask;
}

inline Entity &Component::entity() const {
	return get_entity_by_index(entity_index);
}

enum OwnershipFlags {
	Ownership_copy     = 0x0,
	Ownership_transfer = 0x1,
};

inline Entity &create_entity(Span<utf8> name) {
	auto &result = create_entity();
	result.name.set(name);
	return result;
}
inline Entity &create_entity(char const *name) {
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

inline void remove_component(Entity &entity, ComponentIndex *component) {
	erase(entity.components, component);

	auto &info = get_component_info(component->type);

	auto &storage = info.storage;

	if (info.free) {
		info.free(storage.get(component->index));
	}

	storage.remove_at(component->index);
}

inline void remove_component(Entity &entity, ComponentIndex component) {
	remove_component(entity, find(entity.components, component));
}

inline void destroy_entity(Entity &entity) {
	for (auto &component_index : entity.components) {
		auto &info = get_component_info(component_index.type);

		auto &storage = info.storage;

		if (info.free) {
			info.free(storage.get(component_index.index));
		}

		storage.remove_at(component_index.index);
	}
	free(entity.name);
	remove_entity(entity);
}

void *add_component(Entity &entity, u32 entity_index, u32 component_type) {
	auto &info = get_component_info(component_type);
	auto added = info.storage.add();

	auto component = added.pointer;

	ComponentIndex component_index = {
		.type = component_type,
		.index = added.index,
		.entity_index = entity_index,
	};
	entity.components.add(component_index);

	info.construct(component);

	((Component *)component)->entity_index = entity_index;

	if (info.init)
		info.init(component);

	return component;
}
void *add_component(Entity &entity, u32 component_type) {
	auto found = index_of(get_entities(), &entity);
	assert(found);
	return add_component(entity, found.get(), component_type);
}
void *add_component(u32 entity_index, u32 component_type) {
	return add_component(get_entities()[entity_index], entity_index, component_type);
}


template <class T>
T &add_component(Entity &entity, u32 entity_index) {
	u32 component_type = T::uid;

	auto &info = get_component_info(component_type);

	auto added = info.storage.add();

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
	auto found = index_of(get_entities(), &entity);
	assert(found);
	return add_component<T>(entity, found.get());
}
template <class T>
T &add_component(u32 entity_index) {
	return add_component<T>(get_entities()[entity_index], entity_index);
}


template <class T>
T *get_component(Entity &entity, u32 nth = 0) {
	static constexpr u32 component_type = get_component_type_index<T>();
	for (auto component : entity.components) {
		if (component.type == component_type) {
			return get_component_info(component_type).storage.get(component.index);
		}
	}
}
template <class T>
T *get_component(u32 entity_index, u32 nth = 0) {
	return get_component<T>(get_entities()[entity_index], nth);
}
