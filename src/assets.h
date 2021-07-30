#pragma once
#include "texture.h"

struct {
	struct {
		MaskedBlockList<Texture, 256> all;
		HashMap<Span<utf8>, Texture *> by_path;
		Texture *get(Span<utf8> path) {
			auto found = by_path.find(path);
			if (found) {
				return *found;
			}
		
			print(Print_info, "Loading texture %.\n", path);

			Texture result = {};
			result.texture = t3d::load_texture(with(temporary_allocator, to_pathchars(path, true)));
			if (!result.texture) {
				return 0;
			}
			result.serializable = true;
			result.name.set(path);

			auto pointer = &all.add();
			*pointer = result;
			by_path.get_or_insert(result.name) = pointer;
			return pointer;
		}
	} textures;
} assets;
