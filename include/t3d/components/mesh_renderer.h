#pragma once
#include <t3d/material.h>
#include <t3d/component.h>
#include <t3d/mesh.h>
#include <t3d/texture.h>

#define FIELDS(f) \
f(Mesh *,          mesh,     0) \
f(tg::Texture2D *, lightmap, 0) \
//f(Material *, material, 0) \

DECLARE_COMPONENT(MeshRenderer) {
	Material *material = 0;
	void init() {
		if (!lightmap) lightmap = black_texture;
	}
};

#undef FIELDS
