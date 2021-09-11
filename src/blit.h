#pragma once
#include "common.h"

tg::Shader *blit_texture_shader;

struct BlitColorConstants {
	v4f color;
};
tg::TypedShaderConstants<BlitColorConstants> blit_color_constants;
tg::Shader *blit_color_shader;

struct BlitTextureColorConstants {
	v4f color;
};
tg::TypedShaderConstants<BlitTextureColorConstants> blit_texture_color_constants;
tg::Shader *blit_texture_color_shader;

void blit(v4f color) {
	tg::set_rasterizer({
		.depth_test = false,
		.depth_write = false,
	});
	tg::set_shader(blit_color_shader);
	tg::set_blend(tg::BlendFunction_add, tg::Blend_source_alpha, tg::Blend_one_minus_source_alpha);
	tg::set_topology(tg::Topology_triangle_list);
	tg::set_shader_constants(blit_color_constants, 0);
	tg::update_shader_constants(blit_color_constants, {.color = color});
	tg::draw(3);
}

void blit(tg::Texture2D *texture) {
	tg::set_rasterizer({
		.depth_test = false,
		.depth_write = false,
	});
	tg::set_shader(blit_texture_shader);
	tg::set_blend(tg::BlendFunction_add, tg::Blend_source_alpha, tg::Blend_one_minus_source_alpha);
	tg::set_topology(tg::Topology_triangle_list);
	tg::set_texture(texture, 0);
	tg::draw(3);
}
