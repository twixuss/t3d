#include <t3d/entity.h>
#include <t3d/shared.h>

Entity &create_entity() {
	auto &entity = shared->entities.add();
	entity.name = (List<utf8>)format("Entity %", index_of(shared->entities, &entity).get());
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
	shared->entities.remove(&entity);
}

Entity &get_entity_from_index(u32 index) {
	return shared->entities.at(index);
}
u32 get_entity_index(Entity &entity) {
	return index_of(shared->entities, &entity).get();
}
