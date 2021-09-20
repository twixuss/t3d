#define TGRAPHICS_IMPL
#define TL_IMPL
#define TL_MAIN
#define TL_GL_VALIDATE_EACH_CALL
#pragma comment(lib, "freetype.lib")
#include <freetype/freetype.h>
#include <t3d/entity.h>
#include <t3d/common.h>
#include <t3d/component.h>
#include <tl/masked_block_list.h>

#define A(ret, name, args, params) ret (*_##name) args;
T3D_APIS(A)
#undef A

MaskedBlockList<Entity, 256> entities;
HashMap<Span<utf8>, ComponentUID> component_name_to_uid;
HashMap<ComponentUID, ComponentInfo> component_infos;

void init_api() {
	_create_entity = []() -> Entity & {
		auto &entity = entities.add();
		entity.name = (List<utf8>)format("Entity %", index_of(entities, &entity).get());
		return entity;
	};
	_get_entity_by_index = [](u32 index) -> Entity & {
		return entities[index];
	};
	_get_entity_index = [](Entity &entity) {
		return (u32)index_of(entities, &entity).get();
	};
	_get_component_info = [](ComponentUID uid) -> ComponentInfo & {
		auto result = component_infos.find(uid);
		assert(result);
		return *result;
	};
	_free_component_storages = []() {
		for_each(component_infos, [&](ComponentUID uid, ComponentInfo &info) {
			auto &storage = info.storage;
			for (auto &block : storage.blocks) {
				storage.allocator.free(block);
			}
			free(storage.blocks);
		});
	};

}
