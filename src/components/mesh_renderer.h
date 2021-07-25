#pragma once
#include "material.h"
#include "component.h"
#include "mesh.h"
#include "basic_textures.h"

#define FIELDS(F)

DECLARE_COMPONENT(MeshRenderer) {
	Mesh *mesh;
	Material *material;
	t3d::Texture *lightmap;

	void free() {

	}
};

template <>
void on_create(MeshRenderer &mesh_renderer) {
	mesh_renderer.lightmap = black_texture;
}
