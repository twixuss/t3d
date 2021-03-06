#pragma once
#include <t3d/app.h>

u32 const shadow_map_resolution = 256;

#define FIELDS(F) \
F(f32,             intensity, 100) \
F(f32,             fov,       pi/2) \
F(tg::Texture2D *, mask,      0)

DECLARE_COMPONENT(Light) {
	tg::RenderTarget *shadow_map;

	m4 world_to_light_matrix;

	void init() {
		auto depth = app->tg->create_texture_2d(shadow_map_resolution, shadow_map_resolution, 0, tg::Format_depth);
		shadow_map = app->tg->create_render_target(0, depth);
	}
	void free() {
		//app->tg->free(shadow_map);
	}
};

#undef FIELDS
