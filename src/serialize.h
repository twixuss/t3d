#pragma once
#include <t3d.h>
#include "entity.h"

inline void serialize_component(StringBuilder &builder, u32 component_type, void *data) {
	component_serializers[component_type](builder, data);
}

List<u8> serialize_scene() {
	StringBuilder builder;
	builder.allocator = temporary_allocator;

	for_each(entities, [&](Entity &entity) {
		append(builder, entity.name);
		append(builder, " {\n");
		append_format(builder, "\tposition % % %\n", entity.position.x, entity.position.y, entity.position.z);
		append_format(builder, "\trotation % % % %\n", entity.rotation.x, entity.rotation.y, entity.rotation.z, entity.rotation.w);
		for (auto &component : entity.components) {
			append(builder, "\t");
			append(builder, component_names[component.type]);
			append(builder, " {\n");
			serialize_component(builder, component.type, component_storages[component.type].get(component.index));
			append(builder, "\t}\n");
		}
		append(builder, "}\n");
	});

	return (List<u8>)to_string(builder, current_allocator);
}
