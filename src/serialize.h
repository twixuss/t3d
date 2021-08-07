#pragma once
#include "tl.h"
#include "entity.h"
#include "material.h"
#include "texture.h"

inline void serialize_component(StringBuilder &builder, u32 component_type, void *data) {
	component_info[component_type].serialize(builder, data);
}

void escape_string(StringBuilder &builder, Span<utf8> string) {
	append(builder, '"');
	for (auto c : string) {
		if (c == '"')
			append(builder, '\\');
		append(builder, c);
	}
	append(builder, '"');
}

void serialize(StringBuilder &builder, f32 value) {
	append(builder, value);
}

void serialize(StringBuilder &builder, v3f value) {
	append(builder, value.x);
	append(builder, ' ');
	append(builder, value.y);
	append(builder, ' ');
	append(builder, value.z);
}

void serialize(StringBuilder &builder, Texture2D *value) {
	if (value) {
		escape_string(builder, value->name);
	} else {
		append(builder, "null");
	}
}

void serialize(StringBuilder &builder, Mesh *value) {
	if (value) {
		escape_string(builder, value->name);
	} else {
		append(builder, "null");
	}
}

List<utf8> serialize_scene() {
	StringBuilder builder;
	builder.allocator = temporary_allocator;

	//for_each(textures, [&] (Texture &texture) {
	//	if (!texture.serializable) {
	//		return;
	//	}
	//});

	//for (auto &material : materials) {
	//	
	//}

	for_each(entities, [&](Entity &entity) {
		if (is_editor_entity(entity)) {
			return;
		}

		append(builder, "entity ");
		escape_string(builder, entity.name);
		
		append(builder, " {\n\tposition ");
		serialize(builder, entity.position);
		append(builder, '\n');
		
		auto angles = degrees(to_euler_angles(entity.rotation));
		append_format(builder, "\trotation % % %\n\tscale ", angles.x, angles.y, angles.z);
		
		serialize(builder, entity.scale);
		append(builder, '\n');

		for (auto &component : entity.components) {
			append(builder, "\t");
			append(builder, component_info[component.type].name);
			append(builder, " {\n");
			serialize_component(builder, component.type, component_storages[component.type].get(component.index));
			append(builder, "\t}\n");
		}
		append(builder, "}\n");
	});

	return (List<utf8>)to_string(builder, current_allocator);
}

constexpr auto window_layout_path = tl_file_string("window_layout"s);

void serialize_window_layout() {
	scoped_allocator(temporary_allocator);

	StringBuilder builder;

	main_window->serialize(builder);
	
	write_entire_file(window_layout_path, as_bytes(to_string(builder)));
}

Optional<List<utf8>> unescape_string(Span<utf8> literal) {
	List<utf8> unescaped_name;
	for (umm i = 0; i < literal.size; ++i) {
		auto c = literal.data[i];
		if (c == '\\' && (i + 1 < literal.size) && literal.data[i + 1] == '"') {
			continue;
		}
		unescaped_name.add(c);
	}
	return unescaped_name;
}

bool deserialize_scene(Span<utf8> path) {
	auto source = (Span<utf8>)with(temporary_allocator, read_entire_file(to_pathchars(path, true)));

	if (!source.data) {
		print(Print_error, "Failed to read scene file '%'\n", path);
		return false;
	}

	List<Token> tokens;
	tokens.allocator = temporary_allocator;

	utf8 *c = source.data;
	utf8 *end = source.end();

	HashMap<Span<utf8>, TokenKind> string_to_token_kind;
	string_to_token_kind.allocator = temporary_allocator;
	string_to_token_kind.get_or_insert(u8"null"s)   = Token_null;
	string_to_token_kind.get_or_insert(u8"entity"s) = Token_entity;

	while (c != end) {
		while (c != end && is_whitespace(*c)) {
			c += 1;
		}
		if (c == end) {
			break;
		}

		if (is_alpha(*c) || *c == '_') {
			Token token;
			token.string.data = c;

			c += 1;
			while (c != end && (is_alpha(*c) || *c == '_' || is_digit(*c))) {
				c += 1;
			}

			token.string.size = c - token.string.data;
			
			auto found = string_to_token_kind.find(token.string);
			if (found) {
				token.kind = *found;
			} else {
				token.kind = Token_identifier;
			}

			tokens.add(token);
		} else if (is_digit(*c) || *c == '-') {
			Token token;
			token.kind = Token_number;
			token.string.data = c;

			c += 1;
			while (c != end && (is_digit(*c))) {
				c += 1;
			}

			if (c != end) {
				if (*c == '.') {
					c += 1;
					while (c != end && (is_digit(*c))) {
						c += 1;
					}
				}
			}

			token.string.size = c - token.string.data;
			tokens.add(token);
		} else {
			switch (*c) {
				case '"': {
					Token token;
					token.kind = '"';
					token.string.data = c + 1;

				continue_search:
					c += 1;
					while (c != end && (*c != '"')) {
						c += 1;
					}

					if (c == end) {
						print(Print_error, "Unclosed string literal\n");
						return false;
					}

					if (c[-1] == '\\') {
						goto continue_search;
					}

					token.string.size = c - token.string.data;

					c += 1;
					
					tokens.add(token);
					break;
				}
				case '{':
				case '}': {
					Token token;
					token.kind = *c;
					token.string.data = c;
					token.string.size = 1;
					tokens.add(token);
					c += 1;
					break;
				}
				default: {
					print(Print_error, "Failed to deserialize scene: invalid character '%'\n", *c);
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
					destroy(*entity);
				}
			}
		};

		while (t != end) {

			if (t->kind != Token_entity) {
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
			List<utf8> unescaped_name = successfully_unescaped_name.value;

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

			auto found_entity_index = index_of(entities, &entity);
			assert(found_entity_index.has_value);
			auto entity_index = found_entity_index.value;

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

					result = parsed.value;
					return true;
				};
					
				if (t->string == u8"position"s) {
					t += 1;
					if (!parse_float(entity.position.x)) return false;
					if (!parse_float(entity.position.y)) return false;
					if (!parse_float(entity.position.z)) return false;
				} else if (t->string == u8"rotation"s) {
					t += 1;
					v3f angles;
					if (!parse_float(angles.x)) return false;
					if (!parse_float(angles.y)) return false;
					if (!parse_float(angles.z)) return false;
					entity.rotation = quaternion_from_euler(radians(angles));
				} else if (t->string == u8"scale"s) {
					t += 1;
					if (!parse_float(entity.scale.x)) return false;
					if (!parse_float(entity.scale.y)) return false;
					if (!parse_float(entity.scale.z)) return false;
				} else {
					for (u32 component_type = 0; component_type < component_type_count; component_type += 1) {
						auto component_name = component_info[component_type].name;
						if (t->string == component_name) {
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

							auto added = component_storages[component_type].add();

							component_info[component_type].construct(added.pointer);
							((Component *)added.pointer)->entity_index = entity_index;

							entity.components.add(ComponentIndex{
								.type = component_type,
								.index = added.index,
								.entity_index = entity_index,
							});

							if (!component_info[component_type].deserialize(t, end, added.pointer)) {
								return false;
							}
							component_info[component_type].init(added.pointer);
							break;
						}
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

bool deserialize(f32 &value, Token *&from, Token *end) {
	auto parsed = parse_f32(from->string);

	if (!parsed) {
		print(Print_error, "Failed to parse a number\n");
		return false;
	}
	from += 1;

	value = parsed.value;
	return true;
}

bool deserialize(v3f &value, Token *&from, Token *end) {
	if (!deserialize(value.x, from, end)) return false;
	if (!deserialize(value.y, from, end)) return false;
	if (!deserialize(value.z, from, end)) return false;
	return true;
}

bool deserialize(Texture2D *&value, Token *&from, Token *end) {
	if (from->kind == Token_null) {
		value = 0;
		return true;
	}

	auto successful_path = with(temporary_allocator, unescape_string(from->string));
	if (!successful_path) {
		return false;
	}

	value = assets.textures_2d.get(successful_path.value);

	from += 1;

	return true;
}

bool deserialize(Mesh *&value, Token *&from, Token *end) {
	if (from->kind == Token_null) {
		value = 0;
		return true;
	}

	auto successful_path = with(temporary_allocator, unescape_string(from->string));
	if (!successful_path) {
		return false;
	}
	
	value = assets.meshes.get(successful_path.value);

	from += 1;

	return true;
}

EditorWindow *deserialize_editor_window(Stream &stream) {
#define read_bytes(value) if (!stream.b_read(value_as_bytes(value))) { print(Print_error, "Failed to deserialize editor window: no data for field '" #value "'\n"); return 0; }

	EditorWindowKind kind;
	read_bytes(kind);
	
	EditorWindowId window_id;
	read_bytes(window_id);

	// TODO: make this scalable, like components
	EditorWindow *window;
	switch (kind) {
		case EditorWindow_file_view:      window = create_editor_window<FileView>(kind, window_id); break;
		case EditorWindow_hierarchy_view: window = create_editor_window<HierarchyView>(kind, window_id); break;
		case EditorWindow_property_view:  window = create_editor_window<PropertyView>(kind, window_id); break;
		case EditorWindow_scene_view:     window = create_editor_window<SceneView>(kind, window_id); break;
		case EditorWindow_split_view:     window = create_editor_window<SplitView>(kind, window_id); break;
		case EditorWindow_tab_view:       window = create_editor_window<TabView>(kind, window_id); break;
		default: {
			print(Print_error, "Failed to deserialize editor window: invalid kind (%)\n", (u32)kind);
			return 0;
		}
	}
		
	EditorWindowId parent_id;
	read_bytes(parent_id);

	if (parent_id != (EditorWindowId)-1) {
		auto found = editor_windows.find(parent_id);
		if (!found) {
			print(Print_error, "Failed to deserialize editor window: parent_id is invalid (%)\n", parent_id);
			return 0;
		}
		window->parent = *found;
	}
#undef read_bytes

	if (!window->deserialize(stream)) {
		return 0;
	}

	return window;
}

bool deserialize_window_layout() {
	auto buffer = with(temporary_allocator, read_entire_file(window_layout_path));
	if (!buffer.data) {
		print(Print_error, "Failed to deserialize window layout: file % does not exist\n", window_layout_path);
		return false;
	}

	auto stream = create_memory_stream(buffer);

	main_window = deserialize_editor_window(*stream);

	return stream->remaining_bytes() == 0 && main_window != 0;
}
