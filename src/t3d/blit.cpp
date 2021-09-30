#include "blit.h"
#include <t3d/app.h>
void blit(v4f color) {
	if (color.w == 0)
		return;

	app->tg->set_rasterizer({
		.depth_test = false,
		.depth_write = false,
	});
	app->tg->set_shader(app->blit_color_shader);
	app->tg->set_blend(tg::BlendFunction_add, tg::Blend_source_alpha, tg::Blend_one_minus_source_alpha);
	app->tg->set_topology(tg::Topology_triangle_list);
	app->tg->set_shader_constants(app->blit_color_constants, 0);

	app->tg->update_shader_constants(app->blit_color_constants, {.color = color});

	app->tg->draw(3);
}

void blit(tg::Texture2D *texture, bool blend) {
	app->tg->set_rasterizer({
		.depth_test = false,
		.depth_write = false,
	});
	app->tg->set_shader(app->blit_texture_shader);
	if (blend) {
		app->tg->set_blend(tg::BlendFunction_add, tg::Blend_source_alpha, tg::Blend_one_minus_source_alpha);
	} else {
		app->tg->disable_blend();
	}
	app->tg->set_topology(tg::Topology_triangle_list);
	app->tg->set_sampler(tg::Filtering_linear_mipmap, 0);
	app->tg->set_texture(texture, 0);
	app->tg->draw(3);
}
