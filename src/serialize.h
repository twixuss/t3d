#pragma once
#include <t3d.h>
#include "entity.h"

inline void serialize_component(StringBuilder &builder, u32 component_type, void *data) {
	component_serializers[component_type](builder, data);
}

template <class Container>
struct ForIterator {
	Container &container;
	ForIterator(Container &container) : container(container) {}
	template <class Fn>
	void operator<<(Fn &&fn) {
		for_each(container, std::forward<Fn>(fn));
	}
};

#define foreach(collection) ForIterator(collection) << [&] (auto &it) 

List<u8> serialize_scene() {
	StringBuilder builder;
	builder.allocator = temporary_allocator;

	foreach(entities) {
		append(builder, it.name);
		append(builder, " {\n");
		append_format(builder, "\tposition % % %\n", it.position.x, it.position.y, it.position.z);
		append_format(builder, "\trotation % % % %\n", it.rotation.x, it.rotation.y, it.rotation.z, it.rotation.w);
		for (auto &component : it.components) {
			append(builder, "\t");
			append(builder, component_names[component.type]);
			append(builder, " {\n");
			serialize_component(builder, component.type, component_storages[component.type].get(component.index));
			append(builder, "\t}\n");
		}
		append(builder, "}\n");
	};

	return (List<u8>)to_string(builder, current_allocator);
}
