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
		append_bytes(builder, (u32)value->name.size);
		append_bytes(builder, value->name);
	} else {
		append_bytes(builder, (u32)0);
	}
}

void serialize_binary(StringBuilder &builder, Mesh *value) {
	if (value) {
		append_bytes(builder, (u32)value->name.size);
		append_bytes(builder, value->name);
	} else {
		append_bytes(builder, (u32)0);
	}
}

List<u8> serialize_scene_binary() {
	StringBuilder builder;
	builder.allocator = temporary_allocator;

	for_each(app->entities, [&](Entity &entity) {
		if (is_editor_entity(entity)) {
			return;
		}

		append_bytes(builder, (u32)entity.name.size);
		append_bytes(builder, entity.name);
		append_bytes(builder, entity.position);
		append_bytes(builder, entity.rotation);
		append_bytes(builder, entity.scale);

		append_bytes(builder, (u32)entity.components.size);
		for (auto &component : entity.components) {
			append_bytes(builder, (u32)component.type);
			auto &info = get_component_info(component.type);
			info.serialize(builder, info.storage.get(component.index), true);
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
	unescaped_name.reserve(literal.size);
	for (umm i = 0; i < literal.size; ++i) {
		auto c = literal.data[i];
		if (c == '\\' && (i + 1 < literal.size) && (literal.data[i + 1] == '"' || literal.data[i + 1] == '\\')) {
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

List<u8> serialize_scene_text() {
	StringBuilder builder;
	builder.allocator = temporary_allocator;


	for_each(app->entities, [&](Entity &entity) {
		if (is_editor_entity(entity)) {
			return;
		}

		append(builder, "entity ");
		escape_string(builder, entity.name);

		append(builder, " {\n\tposition ");
		serialize_text(builder, entity.position);
		append(builder, ";\n");

		auto angles = degrees(to_euler_angles(entity.rotation));
		append_format(builder, "\trotation % % %;\n\tscale ", angles.x, angles.y, angles.z);

		serialize_text(builder, entity.scale);
		append(builder, ";\n");

		for (auto &component : entity.components) {
			append(builder, "\t");
			auto &info = get_component_info(component.type);
			append(builder, info.name);
			append(builder, " {\n");
			info.serialize(builder, info.storage.get(component.index), false);
			append(builder, "\t}\n");
		}
		append(builder, "}\n");
	});

	return (List<u8>)to_string(builder, current_allocator);
}

bool deserialize_scene_text(Span<utf8> path) {
	auto source = (Span<utf8>)with(temporary_allocator, read_entire_file(to_pathchars(path, true)));

	if (!source.data) {
		print(Print_error, "Failed to read scene file '%'\n", path);
		return false;
	}

	List<Token> tokens;
	tokens.allocator = temporary_allocator;

	HashMap<Span<utf8>, TokenKind> string_to_token_kind;
	string_to_token_kind.allocator = temporary_allocator;
	string_to_token_kind.get_or_insert(u8"null"s) = Token_null;

	auto current_char_p = source.data;
	auto next_char_p = current_char_p;
	auto end = source.end();

	utf32 c = 0;
	auto next_char = [&] {
		current_char_p = next_char_p;
		if (current_char_p >= end) {
			return false;
		}
		auto got = get_char_and_advance_utf8(&next_char_p);
		if (got.valid()) {
			c = got.get();
			return true;
		}
		return false;
	};

	next_char();

	while (current_char_p < end) {
		while (current_char_p != end && is_whitespace(c)) {
			next_char();
		}
		if (current_char_p == end) {
			break;
		}

		if (is_alpha(c) || c == '_') {
			Token token;
			token.string.data = current_char_p;

			while (next_char() && (is_alpha(c) || c == '_' || is_digit(c))) {
			}

			token.string.size = current_char_p - token.string.data;

			auto found = string_to_token_kind.find(token.string);
			if (found) {
				token.kind = *found;
			} else {
				token.kind = Token_identifier;
			}

			tokens.add(token);
		} else if (is_digit(c) || c == '-') {
			Token token;
			token.kind = Token_number;
			token.string.data = current_char_p;

			while (next_char() && is_digit(c)) {
			}

			if (current_char_p != end) {
				if (c == '.') {
					while (next_char() && is_digit(c)) {
					}
				}
			}

			token.string.size = current_char_p - token.string.data;
			tokens.add(token);
		} else {
			switch (c) {
				case '"': {
					Token token;
					token.kind = '"';
					token.string.data = current_char_p + 1;

				continue_search:
					while (next_char() && (c != '"')) {
					}

					if (current_char_p == end) {
						print(Print_error, "Unclosed string literal\n");
						return false;
					}

					if (current_char_p[-1] == '\\') {
						goto continue_search;
					}

					token.string.size = current_char_p - token.string.data;

					next_char();

					tokens.add(token);
					break;
				}
				case ';':
				case '{':
				case '}': {
					Token token;
					token.kind = c;
					token.string.data = current_char_p;
					token.string.size = 1;
					tokens.add(token);
					next_char();
					break;
				}
				default: {
					print(Print_error, "Failed to deserialize scene: invalid character '%'\n", c);
					return false;
				}
			}
		}
	}

	{
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

		while (t != end) {

			if (t->string != u8"entity"s) {
				print(Print_error, "Expected 'entity' keyword, but got '%'\n", t->string);
				return false;
			}

			t += 1;

			if (t == end) {
				print(Print_error, "Expected entity name in quotes after 'entity' keyword, but got end of file\n");
				return false;
			}
			if (t->kind != '"') {
				print(Print_error, "Expected entity name in quotes, but got '%'\n", t->string);
				return false;
			}

			auto successfully_unescaped_name = with(temporary_allocator, unescape_string(t->string));
			if (!successfully_unescaped_name) {
				print(Print_error, "Failed to unescape string '%'\n", t->string);
				return false;
			}
			List<utf8> unescaped_name = successfully_unescaped_name.get();

			++t;

			if (t == end) {
				print(Print_error, "Expected '{' after entity name, but got end of file\n");
				return false;
			}
			if (t->kind != '{') {
				print(Print_error, "Expected '{' after entity name, but got '%'\n", t->string);
				return false;
			}
			++t;

			print(unescaped_name);
			auto &entity = create_entity(unescaped_name);
			added_entities.add(&entity);

			auto found_entity_index = index_of(app->entities, &entity);
			assert(found_entity_index.valid());
			auto entity_index = (u32)found_entity_index.get();

			while (t != end && t->kind != '}') {
				if (t->kind != Token_identifier) {
					print(Print_error, "Expected position, rotation, scale or component name, but got '%'\n", t->string);
					return false;
				}

				auto parse_float = [&] (f32 &result) {
					if (t == end) {
						print(Print_error, "Expected a number after position, but got end of file\n");
						return false;
					}
					if (t->kind != Token_number) {
						print(Print_error, "Expected a number after position, but got '%'\n", t->string);
						return false;
					}
					auto parsed = parse_f32(t->string);

					if (!parsed) {
						print(Print_error, "Failed to parse a number\n");
						return false;
					}
					t += 1;

					result = parsed.get();
					return true;
				};

				auto started_from = t;

				if (t->string == u8"position"s) {
					t += 1;
					if (!parse_float(entity.position.x)) return false;
					if (!parse_float(entity.position.y)) return false;
					if (!parse_float(entity.position.z)) return false;
					if (t->kind == ';') {
						++t;
					} else {
						print(Print_error, "Error while parsing \"%\"'s position. Expected ';' at the end of line instead of %.", t->string);
						go_to_next_property(started_from, t, end);
					}
				} else if (t->string == u8"rotation"s) {
					t += 1;
					v3f angles;
					if (!parse_float(angles.x)) return false;
					if (!parse_float(angles.y)) return false;
					if (!parse_float(angles.z)) return false;
					entity.rotation = quaternion_from_euler(radians(angles));
					if (t->kind == ';') {
						++t;
					} else {
						print(Print_error, "Error while parsing \"%\"'s rotation. Expected ';' at the end of line instead of %.", t->string);
						go_to_next_property(started_from, t, end);
					}
				} else if (t->string == u8"scale"s) {
					t += 1;
					if (!parse_float(entity.scale.x)) return false;
					if (!parse_float(entity.scale.y)) return false;
					if (!parse_float(entity.scale.z)) return false;
					if (t->kind == ';') {
						++t;
					} else {
						print(Print_error, "Error while parsing \"%\"'s scale. Expected ';' at the end of line instead of %.", t->string);
						go_to_next_property(started_from, t, end);
					}
				} else {
					ComponentInfo *found_info = 0;
					ComponentUID component_type;
					for_each(app->component_infos, [&](ComponentUID uid, ComponentInfo &info) {
						if (info.name == t->string) {
							found_info = &info;
							component_type = uid;
							for_each_break;
						}
						for_each_continue;
					});

					if (found_info) {
						auto &info = *found_info;
						auto &storage = info.storage;
						auto component_name = info.name;

						t += 1;
						if (t == end) {
							print(Print_error, "Expected '{' after component name, but got end of file\n");
							return false;
						}
						if (t->kind != '{') {
							print(Print_error, "Expected '{' after component name, but got '%'\n",  t->string);
							return false;
						}
						t += 1;
						if (t == end) {
							print(Print_error, "Unclosed body of % component\n", component_name);
							return false;
						}

						auto added = storage.add();

						info.construct(added.pointer);
						((Component *)added.pointer)->entity_index = entity_index;

						entity.components.add(ComponentIndex{
							.type = component_type,
							.index = added.index,
							.entity_index = entity_index,
						});

						if (!info.deserialize_text(t, end, added.pointer)) {
							return false;
						}
						if (info.init) {
							info.init(added.pointer);
						}
					} else {
						print(Print_error, "Unexpected token '%'. There is no component with this name.\n", t->string);
						return false;
					}
				}
			}

			if (t == end) {
				print(Print_error, "Unclosed entity block body\n");
				return false;
			}

			t += 1;
		}

		success = true;
	}

	//for (auto &token : tokens) {
	//	print("%\n", token.string);
	//}

	return true;
}

bool deserialize_text(f32 &value, Token *&from, Token *end) {
	auto parsed = parse_f32(from->string);

	if (!parsed) {
		print(Print_error, "Failed to parse a number\n");
		return false;
	}
	from += 1;

	value = parsed.get();
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

	value = app->assets.get_texture_2d(successful_path.get());

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

	value = app->assets.get_mesh(successful_path.get());

	from += 1;

	return true;
}

bool deserialize_scene_binary(Span<u8> data) {
	auto cursor = data.data;
	auto end = data.end();

	while(cursor != end) {
		auto &entity = create_entity();
		auto entity_index = (u32)index_of(app->entities, &entity).get();

		u32 name_size;
		if (cursor + sizeof(name_size) > end) {
			print(Print_error, "Failed to deserialize scene: reached data end too soon (name_size)\n");
			return false;
		}
		name_size = *(u32 *)cursor;
		cursor += sizeof(name_size);


		if (cursor + name_size > end) {
			print(Print_error, "Failed to deserialize scene: reached data end too soon (name)\n");
			return false;
		}
		entity.name.set({(utf8 *)cursor, name_size});
		cursor += name_size;


		if (cursor + sizeof(entity.position) > end) {
			print(Print_error, "Failed to deserialize scene: reached data end too soon (entity.position)\n");
			return false;
		}
		entity.position = *(v3f *)cursor;
		cursor += sizeof(entity.position);


		if (cursor + sizeof(entity.rotation) > end) {
			print(Print_error, "Failed to deserialize scene: reached data end too soon (entity.rotation)\n");
			return false;
		}
		entity.rotation = *(quaternion *)cursor;
		cursor += sizeof(entity.rotation);


		if (cursor + sizeof(entity.scale) > end) {
			print(Print_error, "Failed to deserialize scene: reached data end too soon (entity.scale)\n");
			return false;
		}
		entity.scale = *(v3f *)cursor;
		cursor += sizeof(entity.scale);


		u32 component_count;
		if (cursor + sizeof(component_count) > end) {
			print(Print_error, "Failed to deserialize scene: reached data end too soon (component_count)\n");
			return false;
		}
		component_count = *(u32 *)cursor;
		cursor += sizeof(component_count);

		for (u32 component_index = 0; component_index < component_count; component_index += 1) {
			u32 component_type;
			if (cursor + sizeof(component_type) > end) {
				print(Print_error, "Failed to deserialize scene: reached data end too soon (component_type)\n");
				return false;
			}
			component_type = *(u32 *)cursor;
			cursor += sizeof(component_type);

			if (!app->component_infos.find(component_type)) {
				print(Print_error, "Failed to deserialize scene: component type is invalid (%)\n", component_type);
				return false;
			}

			auto &info = get_component_info(component_type);
			auto &storage = info.storage;

			auto added = storage.add();

			info.construct(added.pointer);
			((Component *)added.pointer)->entity_index = entity_index;

			entity.components.add(ComponentIndex{
				.type = component_type,
				.index = added.index,
				.entity_index = entity_index,
			});

			if (!info.deserialize_binary(cursor, end, added.pointer)) {
				return false;
			}
			if (info.init)
				info.init(added.pointer);
		}
	}



	return true;
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
