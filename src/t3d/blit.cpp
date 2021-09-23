#include "blit.h"
#include <t3d/shared.h>
void blit(v4f color) {
	shared->tg->set_rasterizer({
		.depth_test = false,
		.depth_write = false,
	});
	shared->tg->set_shader(shared->blit_color_shader);
	shared->tg->set_blend(tg::BlendFunction_add, tg::Blend_source_alpha, tg::Blend_one_minus_source_alpha);
	shared->tg->set_topology(tg::Topology_triangle_list);
	shared->tg->set_shader_constants(shared->blit_color_constants, 0);

	shared->tg->update_shader_constants(shared->blit_color_constants, {.color = color});

	shared->tg->draw(3);
}

void blit(tg::Texture2D *texture) {
	shared->tg->set_rasterizer({
		.depth_test = false,
		.depth_write = false,
	});
	shared->tg->set_shader(shared->blit_texture_shader);
	shared->tg->set_blend(tg::BlendFunction_add, tg::Blend_source_alpha, tg::Blend_one_minus_source_alpha);
	shared->tg->set_topology(tg::Topology_triangle_list);
	shared->tg->set_sampler(tg::Filtering_linear_mipmap, 0);
	shared->tg->set_texture(texture, 0);
	shared->tg->draw(3);
}
