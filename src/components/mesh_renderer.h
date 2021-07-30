#pragma once
#include "material.h"
#include "component.h"
#include "mesh.h"
#include "texture.h"

#define FIELDS(f) \
f(Mesh *, mesh, 0) \
f(Texture *, lightmap, 0) \
//f(Material *, material, 0) \

DECLARE_COMPONENT(MeshRenderer) {
	Material *material = 0;
};

template <>
void component_init(MeshRenderer &mesh_renderer) {
	if (!mesh_renderer.lightmap) mesh_renderer.lightmap = black_texture;
}
