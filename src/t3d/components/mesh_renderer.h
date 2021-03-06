#pragma once
#include <t3d/app.h>
#include <t3d/material.h>
#include <t3d/mesh.h>

#define FIELDS(f) \
f(Mesh *,          mesh,     0) \
f(tg::Texture2D *, lightmap, 0) \
//f(Material *, material, 0) \

DECLARE_COMPONENT(MeshRenderer) {
	Material *material = 0;
};

#undef FIELDS
