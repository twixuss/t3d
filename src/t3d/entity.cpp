#include <t3d/entity.h>
#include <t3d/shared_data.h>

inline Entity &create_entity() {
	auto &entity = shared_data->entities.add();
	entity.name = (List<utf8>)format("Entity %", index_of(shared_data->entities, &entity).get());
	return entity;
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
	shared_data->entities.remove(&entity);
}

inline Entity &get_entity_from_index(u32 index) {
	return shared_data->entities.at(index);
}
inline u32 get_entity_index(Entity &entity) {
	return index_of(shared_data->entities, &entity).get();
}
