#include <t3d/entity.h>
#include <t3d/app.h>
#include <t3d/scene.h>


void destroy_entity(Entity &entity) {
	for (auto &component_index : entity.components) {

		auto &info = app->component_infos.find(component_index.type_uid).get();
		auto &storage = entity.scene->component_storages.find(component_index.type_uid).get();

		if (info.free) {
			info.free(storage.get(component_index.storage_index));
		}

		storage.remove_at(component_index.storage_index);
	}
	free(entity.name);
	entity.scene->entities.remove(&entity);
}

Entity &get_entity_from_index(Scene *scene, u32 index) {
	return scene->entities.at(index);
}
u32 get_entity_index(Entity &entity) {
	return index_of(entity.scene->entities, &entity).value();
}

void remove_component(Entity &entity, ComponentIndex *component) {
	erase(entity.components, component);

	auto &info = app->component_infos.find(component->type_uid).get();
	auto &storage = entity.scene->component_storages.find(component->type_uid).get();

	if (info.free) {
		info.free(storage.get(component->storage_index));
	}

	storage.remove_at(component->storage_index);
}

void *add_component(Entity &entity, u32 entity_index, Uid component_type_uid) {
	auto &info = app->component_infos.find(component_type_uid).get();
	auto &storage = entity.scene->find_or_create_component_storage(component_type_uid, info);

	auto added = storage.add();

	ComponentIndex component_index = {
		.type_uid = component_type_uid,
		.storage_index = added.index,
		.entity_index = entity_index,
	};
	entity.components.add(component_index);

	info.construct(added.pointer);

	if (info.init)
		info.init(added.pointer);

	((Component *)added.pointer)->_entity = &entity;

	return added.pointer;
}
