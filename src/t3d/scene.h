#pragma once
#include <t3d/entity.h>

struct Scene {
	StaticMaskedBlockList<Entity, 256> entities;
	HashMap<Uid, ComponentStorage> component_storages;

	Scene() {
		entities.allocator = default_allocator;
		component_storages.allocator = default_allocator;
	}

	Entity &create_entity();

	Entity &create_entity(Span<utf8> name) {
		auto &result = create_entity();
		result.name.set(name);
		return result;
	}
	Entity &create_entity(Span<ascii> name) { return create_entity(as_utf8(name)); }
	Entity &create_entity(ascii const *name) { return create_entity(as_utf8(as_span(name))); }

	template <class Component, class Fn>
	void for_each_component(Fn &&fn) {
		auto found_storage = component_storages.find(component_name_to_uid(Component::_t3d_component_name));
		assert(found_storage);
		found_storage->for_each([&](void *component) {
			return fn(*(Component *)component);
		});
	}

	ComponentStorage &find_or_create_component_storage(Uid component_type_uid, ComponentInfo &info) {
		auto &storage = component_storages.get_or_insert(component_type_uid);
		if (!storage.bytes_per_entry) {
			storage.bytes_per_entry = info.size;
			storage.entry_alignment = info.alignment;
		}
		return storage;
	}

	void *get_component_data(ComponentIndex component) {
		return component_storages.find(component.type_uid).get().get(component.storage_index);
	}

	void free() {
		for_each(component_storages, [&](Uid uid, ComponentStorage &storage) {
			::free(storage);
		});
	}
};
