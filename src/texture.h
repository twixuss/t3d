#pragma once
#include "../include/t3d.h"

struct Texture {
	t3d::Texture *texture;
	List<utf8> name;
	bool serializable;
};

Texture *white_texture;
Texture *black_texture;

MaskedBlockList<Texture, 256> textures;
HashMap<Span<pathchar>, Texture *> textures_by_path;

Texture *load_texture(Span<pathchar> path, t3d::LoadTextureParams params = {}) {
	Texture result = {};
	result.texture = t3d::load_texture(with(temporary_allocator, null_terminate(path)), params);
	if (!result.texture) {
		return 0;
	}
	result.serializable = true;
	result.name = to_utf8(path);

	auto pointer = &textures.add();
	*pointer = result;
	textures_by_path.get_or_insert(path) = pointer;
	return pointer;
}
