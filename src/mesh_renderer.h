#pragma once
#include "material.h"
#include "component.h"
#include "mesh.h"
#include "basic_textures.h"

struct MeshRenderer : Component {
	Mesh *mesh;
	Material *material;
	t3d::Texture *lightmap;
};

template <>
void on_create(MeshRenderer &mesh_renderer) {
	mesh_renderer.lightmap = black_texture;
}
