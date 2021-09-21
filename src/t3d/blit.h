#pragma once
#include <t3d/shared_data.h>

inline void blit(v4f color) {
	tg::set_rasterizer({
		.depth_test = false,
		.depth_write = false,
	});
	tg::set_shader(shared_data->blit_color_shader);
	tg::set_blend(tg::BlendFunction_add, tg::Blend_source_alpha, tg::Blend_one_minus_source_alpha);
	tg::set_topology(tg::Topology_triangle_list);
	tg::set_shader_constants(shared_data->blit_color_constants, 0);
	tg::update_shader_constants(shared_data->blit_color_constants, {.color = color});
	tg::draw(3);
}

inline void blit(tg::Texture2D *texture) {
	tg::set_rasterizer({
		.depth_test = false,
		.depth_write = false,
	});
	tg::set_shader(shared_data->blit_texture_shader);
	tg::set_blend(tg::BlendFunction_add, tg::Blend_source_alpha, tg::Blend_one_minus_source_alpha);
	tg::set_topology(tg::Topology_triangle_list);
	tg::set_sampler(tg::Filtering_linear_mipmap, 0);
	tg::set_texture(texture, 0);
	tg::draw(3);
}
