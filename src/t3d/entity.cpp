#include <t3d/entity.h>
#include <t3d/app.h>

Entity &create_entity() {
	auto &entity = app->entities.add();
	entity.name = (List<utf8>)format("Entity %", index_of(app->entities, &entity).get());
	return entity;
}

void destroy_entity(Entity &entity) {
	for (auto &component_index : entity.components) {
		auto &info = get_component_info(component_index.type);

		auto &storage = info.storage;

		if (info.free) {
			info.free(storage.get(component_index.index));
		}

		storage.remove_at(component_index.index);
	}
	free(entity.name);
	app->entities.remove(&entity);
}

Entity &get_entity_from_index(u32 index) {
	return app->entities.at(index);
}
u32 get_entity_index(Entity &entity) {
	return index_of(app->entities, &entity).get();
}
