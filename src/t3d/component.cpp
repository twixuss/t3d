#include <t3d/component.h>
#include <t3d/app.h>

ComponentInfo &get_component_info(ComponentUID uid) {
	auto result = app->component_infos.find(uid);
	assert(result);
	return *result;
}

void free_component_storages() {
	for_each(app->component_infos, [&](ComponentUID uid, ComponentInfo &info) {
		auto &storage = info.storage;
		for (auto &block : storage.blocks) {
			storage.allocator.free(block);
		}
		free(storage.blocks);
	});
}

ComponentInfo &component_infos_get_or_insert(ComponentUID uid) {
	return app->component_infos.get_or_insert(uid);
}

ComponentUID *component_name_to_uid_find(Span<utf8> name) {
	return app->component_name_to_uid.find(name);
}

ComponentUID get_new_component_uid() {
	return atomic_increment(&app->component_uid_counter);
}
