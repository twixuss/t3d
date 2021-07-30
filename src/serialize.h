#pragma once
#include <t3d.h>
#include "entity.h"
#include "material.h"
#include "texture.h"

inline void serialize_component(StringBuilder &builder, u32 component_type, void *data) {
	component_functions[component_type].serialize(builder, data);
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

void serialize(StringBuilder &builder, Texture *value) {
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
		if (entity.flags & Entity_editor)
			return;

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
			append(builder, component_names[component.type]);
			append(builder, " {\n");
			serialize_component(builder, component.type, component_storages[component.type].get(component.index));
			append(builder, "\t}\n");
		}
		append(builder, "}\n");
	});

	return (List<utf8>)to_string(builder, current_allocator);
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

bool deserialize_scene(Span<utf8> source) {
	List<Token> tokens;
	tokens.allocator = temporary_allocator;

	utf8 *c = source.data;
	utf8 *end = source.end();

	while (c != end) {
		while (c != end && is_whitespace(*c)) {
			c += 1;
		}
		if (c == end) {
			break;
		}

		if (is_alpha(*c) || *c == '_') {
			Token token;
			token.kind = Token_identifier;
			token.string.data = c;

			c += 1;
			while (c != end && (is_alpha(*c) || *c == '_' || is_digit(*c))) {
				c += 1;
			}

			token.string.size = c - token.string.data;
			
			if (token.string == u8"null"s) {
				token.kind = Token_null;
			} else if (token.string == u8"entity"s) {
				token.kind = Token_entity;
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
						auto component_name = component_names[component_type];
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

							((Component *)added.pointer)->entity_index = entity_index;

							entity.components.add(ComponentIndex{
								.type = component_type,
								.index = added.index,
								.entity_index = entity_index,
							});

							if (!component_functions[component_type].deserialize(t, end, added.pointer)) {
								return false;
							}
							component_functions[component_type].init(added.pointer);
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

bool deserialize(Texture *&value, Token *&from, Token *end) {
	if (from->kind == Token_null) {
		value = 0;
		return true;
	}

	auto successful_path = with(temporary_allocator, unescape_string(from->string));
	if (!successful_path) {
		return false;
	}

	value = assets.textures.get(successful_path.value);

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
	auto path = successful_path.value;

	auto found = meshes_by_name.find(path);
	if (found) {
		value = *found;
	} else {
		auto submesh_separator = find(path, u8':');
		if (submesh_separator) {
			auto scene_path   = Span<utf8>{path.data, submesh_separator};
			auto submesh_name = Span<utf8>{submesh_separator + 1, path.end()};


			auto found_scene = scenes3d_by_name.find(scene_path);
			Scene3D *scene = 0;
			if (found_scene) {
				scene = *found_scene;
			} else {
				scene = &scenes3d.add();
				auto parsed = parse_glb_from_file(scene_path);
				if (!parsed) {
					print(Print_error, "Failed to parse scene file '%'\n", scene_path);
					return false;
				}
				*scene = parsed.value;
				scenes3d_by_name.get_or_insert(scene_path) = scene;
			}

			assert(scene);

			value = get_submesh(*scene, submesh_name);
		} else {
			value = load_mesh(path);
		}
	}

	from += 1;

	return true;
}
