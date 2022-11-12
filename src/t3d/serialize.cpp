#include "serialize.h"
#include <t3d/entity.h>
#include <t3d/app.h>

void serialize_binary(StringBuilder &builder, f32 value) {
	append_bytes(builder, value);
}

void serialize_binary(StringBuilder &builder, v3f value) {
	append_bytes(builder, value);
}

void serialize_binary(StringBuilder &builder, Texture2D *value) {
	if (value) {
		append_bytes(builder, (u32)value->name.count);
		append_bytes(builder, value->name);
	} else {
		append_bytes(builder, (u32)0);
	}
}

void serialize_binary(StringBuilder &builder, Mesh *value) {
	if (value) {
		append_bytes(builder, (u32)value->name.count);
		append_bytes(builder, value->name);
	} else {
		append_bytes(builder, (u32)0);
	}
}

List<u8> serialize_scene_binary(Scene *scene, HashMap<Uid, Uid> component_type_uid_remap) {
	StringBuilder builder;
	builder.allocator = temporary_allocator;

	for_each(scene->entities, [&](Entity &entity) {
		if (is_editor_entity(entity)) {
			return;
		}

		append_bytes(builder, (u32)entity.name.count);
		append_bytes(builder, entity.name);
		append_bytes(builder, entity.position);
		append_bytes(builder, entity.rotation);
		append_bytes(builder, entity.scale);

		append_bytes(builder, (u32)entity.components.count);
		for (auto &component : entity.components) {
			append_bytes(builder, component_type_uid_remap.find(component.type_uid).get());
			auto &info = get_component_info(component.type_uid);
			info.serialize(builder, app->current_scene->get_component_data(component), true);
		}
	});

	return (List<u8>)to_string(builder, current_allocator);
}

void escape_string(StringBuilder &builder, Span<utf8> string) {
	append(builder, '"');
	for (auto c : string) {
		if (c == '"') append(builder, '\\');
		if (c == '\\') append(builder, '\\');
		append(builder, c);
	}
	append(builder, '"');
}

Optional<List<utf8>> unescape_string(Span<utf8> literal) {
	List<utf8> unescaped_name;
	unescaped_name.reserve(literal.count);
	for (umm i = 0; i < literal.count; ++i) {
		auto c = literal.data[i];
		if (c == '\\' && (i + 1 < literal.count) && (literal.data[i + 1] == '"' || literal.data[i + 1] == '\\')) {
			continue;
		}
		unescaped_name.add(c);
	}
	return unescaped_name;
}

void serialize_text(StringBuilder &builder, f32 value) {
	append(builder, FormatFloat{.value = value, .precision = 9});
}

void serialize_text(StringBuilder &builder, v3f value) {
	append(builder, value.x);
	append(builder, ' ');
	append(builder, value.y);
	append(builder, ' ');
	append(builder, value.z);
}

void serialize_text(StringBuilder &builder, Texture2D *value) {
	if (value) {
		escape_string(builder, value->name);
	} else {
		append(builder, "null");
	}
}

void serialize_text(StringBuilder &builder, Mesh *value) {
	if (value) {
		escape_string(builder, value->name);
	} else {
		append(builder, "null");
	}
}

List<u8> serialize_scene_text(Scene *scene) {
	StringBuilder builder;
	builder.allocator = temporary_allocator;


	for_each(scene->entities, [&](Entity &entity) {
		if (is_editor_entity(entity)) {
			return;
		}

		append(builder, "entity ");
		escape_string(builder, entity.name);

		append(builder, " {\n\tposition ");
		serialize_text(builder, entity.position);
		append(builder, ";\n");

		auto angles = degrees(to_euler_angles(entity.rotation));
		append_format(builder, "\trotation {} {} {};\n\tscale ", angles.x, angles.y, angles.z);

		serialize_text(builder, entity.scale);
		append(builder, ";\n");

		for (auto &component : entity.components) {
			append(builder, "\t");
			auto &info = get_component_info(component.type_uid);
			append(builder, info.name);
			append(builder, " {\n");
			info.serialize(builder, scene->get_component_data(component), false);
			append(builder, "\t}\n");
		}
		append(builder, "}\n");
	});

	return (List<u8>)to_string(builder, current_allocator);
}

Scene *deserialize_scene_text(Span<utf8> path) {
	auto source = (Span<utf8>)with(temporary_allocator, read_entire_file(to_pathchars(path, true)));

	if (!source.data) {
		print(Print_error, "Failed to read scene file '{}'\n", path);
		return 0;
	}

	auto got_tokens = parse_tokens(source);
	if (!got_tokens) {
		return 0;
	}
	List<Token> tokens = got_tokens.value();

	Token *t = tokens.data;
	Token *end = tokens.end();

	List<Entity *> added_entities;
	bool success = false;
	defer {
		if (!success) {
			for (auto entity : added_entities) {
				destroy_entity(*entity);
			}
		}
	};

	auto scene = default_allocator.allocate<Scene>();

	while (t != end) {

		if (t->string != u8"entity"s) {
			print(Print_error, "Expected 'entity' keyword, but got '{}'\n", t->string);
			return 0;
		}

		t += 1;

		if (t == end) {
			print(Print_error, "Expected entity name in quotes after 'entity' keyword, but got end of file\n");
			return 0;
		}
		if (t->kind != '"') {
			print(Print_error, "Expected entity name in quotes, but got '{}'\n", t->string);
			return 0;
		}

		auto successfully_unescaped_name = with(temporary_allocator, unescape_string(t->string));
		if (!successfully_unescaped_name) {
			print(Print_error, "Failed to unescape string '{}'\n", t->string);
			return 0;
		}
		List<utf8> unescaped_name = successfully_unescaped_name.value();

		++t;

		if (t == end) {
			print(Print_error, "Expected '{' after entity name, but got end of file\n");
			return 0;
		}
		if (t->kind != '{') {
			print(Print_error, "Expected '{' after entity name, but got '{}'\n", t->string);
			return 0;
		}
		++t;

		auto &entity = scene->create_entity(unescaped_name);
		added_entities.add(&entity);

		auto entity_index = (u32)index_of(scene->entities, &entity).value();

		while (t != end && t->kind != '}') {
			if (t->kind != Token_identifier) {
				print(Print_error, "Expected position, rotation, scale or component name, but got '{}'\n", t->string);
				return 0;
			}

			auto parse_float = [&] (f32 &result) {
				if (t == end) {
					print(Print_error, "Expected a number after position, but got end of file\n");
					return false;
				}
				if (t->kind != Token_number) {
					print(Print_error, "Expected a number after position, but got '{}'\n", t->string);
					return false;
				}
				auto parsed = parse_f32(t->string);

				if (!parsed) {
					print(Print_error, "Failed to parse a number\n");
					return false;
				}
				t += 1;

				result = parsed.value();
				return true;
			};

			auto started_from = t;

			if (t->string == u8"position"s) {
				t += 1;
				if (!parse_float(entity.position.x)) return 0;
				if (!parse_float(entity.position.y)) return 0;
				if (!parse_float(entity.position.z)) return 0;
				if (t->kind == ';') {
					++t;
				} else {
					print(Print_error, "Error while parsing \"{}\"'s position. Expected ';' at the end of line instead of {}.", t->string);
					go_to_next_property(started_from, t, end);
				}
			} else if (t->string == u8"rotation"s) {
				t += 1;
				v3f angles;
				if (!parse_float(angles.x)) return 0;
				if (!parse_float(angles.y)) return 0;
				if (!parse_float(angles.z)) return 0;
				entity.rotation = quaternion_from_euler(radians(angles));
				if (t->kind == ';') {
					++t;
				} else {
					print(Print_error, "Error while parsing \"{}\"'s rotation. Expected ';' at the end of line instead of {}.", t->string);
					go_to_next_property(started_from, t, end);
				}
			} else if (t->string == u8"scale"s) {
				t += 1;
				if (!parse_float(entity.scale.x)) return 0;
				if (!parse_float(entity.scale.y)) return 0;
				if (!parse_float(entity.scale.z)) return 0;
				if (t->kind == ';') {
					++t;
				} else {
					print(Print_error, "Error while parsing \"{}\"'s scale. Expected ';' at the end of line instead of {}.", t->string);
					go_to_next_property(started_from, t, end);
				}
			} else {
				ComponentInfo *found_info = 0;
				Uid component_type_uid;
				for_each(app->component_infos, [&](Uid uid, ComponentInfo &info) {
					if (info.name == t->string) {
						found_info = &info;
						component_type_uid = uid;
						for_each_break;
					}
					for_each_continue;
				});

				if (found_info) {
					auto &info = *found_info;
					auto &storage = scene->find_or_create_component_storage(component_type_uid, info);
					auto component_name = info.name;

					t += 1;
					if (t == end) {
						print(Print_error, "Expected '{' after component name, but got end of file\n");
						return 0;
					}
					if (t->kind != '{') {
						print(Print_error, "Expected '{' after component name, but got '{}'\n",  t->string);
						return 0;
					}
					t += 1;
					if (t == end) {
						print(Print_error, "Unclosed body of {} component\n", component_name);
						return 0;
					}

					// TODO: similar code is in `add_component(Entity &, u32, Uid)`
					auto added = storage.add();

					info.construct(added.pointer);
					((Component *)added.pointer)->_entity = &entity;

					entity.components.add(ComponentIndex{
						.type_uid = component_type_uid,
						.storage_index = added.index,
						.entity_index = entity_index,
					});

					if (!info.deserialize_text(t, end, added.pointer)) {
						return 0;
					}
					if (info.init) {
						info.init(added.pointer);
					}
				} else {
					print(Print_error, "Unexpected token '{}'. There is no component with this name.\n", t->string);
					return 0;
				}
			}
		}

		if (t == end) {
			print(Print_error, "Unclosed entity block body\n");
			return 0;
		}

		t += 1;
	}

	success = true;

	//for (auto &token : tokens) {
	//	print("{}\n", token.string);
	//}

	return scene;
}

bool deserialize_text(f32 &value, Token *&from, Token *end) {
	auto parsed = parse_f32(from->string);

	if (!parsed) {
		print(Print_error, "Failed to parse a number\n");
		return false;
	}
	from += 1;

	value = parsed.value();
	return true;
}

bool deserialize_text(v3f &value, Token *&from, Token *end) {
	if (!deserialize_text(value.x, from, end)) return false;
	if (!deserialize_text(value.y, from, end)) return false;
	if (!deserialize_text(value.z, from, end)) return false;
	return true;
}

bool deserialize_text(Texture2D *&value, Token *&from, Token *end) {
	if (from->kind == Token_null) {
		value = 0;
		from += 1;
		return true;
	}

	auto successful_path = with(temporary_allocator, unescape_string(from->string));
	if (!successful_path) {
		return false;
	}

	value = app->assets.get_texture_2d(successful_path.value());

	from += 1;

	return true;
}

bool deserialize_text(Mesh *&value, Token *&from, Token *end) {
	if (from->kind == Token_null) {
		value = 0;
		from += 1;
		return true;
	}

	auto successful_path = with(temporary_allocator, unescape_string(from->string));
	if (!successful_path) {
		return false;
	}

	value = app->assets.get_mesh(successful_path.value());

	from += 1;

	return true;
}

Scene *deserialize_scene_binary(Span<u8> data) {
	auto scene = default_allocator.allocate<Scene>();

	auto cursor = data.data;
	auto end = data.end();

	while(cursor != end) {
		auto &entity = scene->create_entity();
		auto entity_index = (u32)index_of(scene->entities, &entity).value();

		u32 name_size;
		if (cursor + sizeof(name_size) > end) {
			print(Print_error, "Failed to deserialize scene: reached data end too soon (name_size)\n");
			return 0;
		}
		name_size = *(u32 *)cursor;
		cursor += sizeof(name_size);


		if (cursor + name_size > end) {
			print(Print_error, "Failed to deserialize scene: reached data end too soon (name)\n");
			return 0;
		}
		entity.name.set({(utf8 *)cursor, name_size});
		cursor += name_size;


		if (cursor + sizeof(entity.position) > end) {
			print(Print_error, "Failed to deserialize scene: reached data end too soon (entity.position)\n");
			return 0;
		}
		entity.position = *(v3f *)cursor;
		cursor += sizeof(entity.position);


		if (cursor + sizeof(entity.rotation) > end) {
			print(Print_error, "Failed to deserialize scene: reached data end too soon (entity.rotation)\n");
			return 0;
		}
		entity.rotation = *(quaternion *)cursor;
		cursor += sizeof(entity.rotation);


		if (cursor + sizeof(entity.scale) > end) {
			print(Print_error, "Failed to deserialize scene: reached data end too soon (entity.scale)\n");
			return 0;
		}
		entity.scale = *(v3f *)cursor;
		cursor += sizeof(entity.scale);


		u32 component_count;
		if (cursor + sizeof(component_count) > end) {
			print(Print_error, "Failed to deserialize scene: reached data end too soon (component_count)\n");
			return 0;
		}
		component_count = *(u32 *)cursor;
		cursor += sizeof(component_count);

		for (u32 component_index = 0; component_index < component_count; component_index += 1) {
			Uid component_type_uid;
			if (cursor + sizeof(component_type_uid) > end) {
				print(Print_error, "Failed to deserialize scene: reached data end too soon (component_type_uid)\n");
				return 0;
			}
			component_type_uid = *(Uid *)cursor;
			cursor += sizeof(component_type_uid);

			if (!app->component_infos.find(component_type_uid)) {
				print(Print_error, "Failed to deserialize scene: component type uid is not present ({})\n", component_type_uid);
				return 0;
			}

			auto &info = get_component_info(component_type_uid);
			auto &storage = scene->find_or_create_component_storage(component_type_uid, info);

			auto added = storage.add();

			// TODO: similar code is in `add_component(Entity &, u32, Uid)`

			info.construct(added.pointer);
			((Component *)added.pointer)->_entity = &entity;

			entity.components.add(ComponentIndex{
				.type_uid = component_type_uid,
				.storage_index = added.index,
				.entity_index = entity_index,
			});

			if (!info.deserialize_binary(cursor, end, added.pointer)) {
				return 0;
			}
			if (info.init)
				info.init(added.pointer);
		}
	}

	return scene;
}
bool deserialize_binary(f32 &value, u8 *&from, u8 *end) {
	if (from + sizeof(value) > end) {
		print(Print_error, "Failed to deserialize `f32`: reached data end too soon\n");
		return false;
	}

	value = *(f32 *)from;
	from += sizeof(value);
	return true;
}

bool deserialize_binary(v3f &value, u8 *&from, u8 *end) {
	if (from + sizeof(value) > end) {
		print(Print_error, "Failed to deserialize `v3f`: reached data end too soon\n");
		return false;
	}

	value = *(v3f *)from;
	from += sizeof(value);
	return true;
}

bool deserialize_binary(Texture2D *&value, u8 *&from, u8 *end) {
	u32 path_size;
	if (from + sizeof(path_size) > end) {
		print(Print_error, "Failed to deserialize `Texture2D *`: reached data end too soon (path_size)\n");
		return false;
	}
	path_size = *(u32 *)from;
	from += sizeof(path_size);

	if (path_size == 0) {
		value = 0;
		return true;;
	}

	if (from + path_size > end) {
		print(Print_error, "Failed to deserialize `Texture2D *`: reached data end too soon (path)\n");
		return false;
	}

	value = app->assets.get_texture_2d(Span((utf8 *)from, path_size));
	from += path_size;

	return true;
}

bool deserialize_binary(Mesh *&value, u8 *&from, u8 *end) {
	u32 path_size;
	if (from + sizeof(path_size) > end) {
		print(Print_error, "Failed to deserialize `Mesh *`: reached data end too soon (path_size)\n");
		return false;
	}
	path_size = *(u32 *)from;
	from += sizeof(path_size);

	if (path_size == 0) {
		value = 0;
		return true;;
	}

	if (from + path_size > end) {
		print(Print_error, "Failed to deserialize `Mesh *`: reached data end too soon (path)\n");
		return false;
	}

	value = app->assets.get_mesh(Span((utf8 *)from, path_size));
	from += path_size;

	return true;
}
