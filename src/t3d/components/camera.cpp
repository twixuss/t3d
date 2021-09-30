#include "camera.h"
REGISTER_COMPONENT(Camera)

#include <t3d/app.h>

v3f Camera::world_to_ndc(v4f point) {
	auto p = world_to_camera_matrix * point;
	return {p.xyz / p.w};
}
v3f Camera::world_to_ndc(v3f point) {
	return world_to_ndc(V4f(point, 1));
}
v3f Camera::world_to_window(v4f point) {
	return map(world_to_ndc(point), {-1,-1,-1}, {1,1,1}, {0,0,0}, V3f((v2f)source_target->color->size, 1));
}
v3f Camera::world_to_window(v3f point) {
	return world_to_window(V4f(point, 1));
}

void Camera::init() {
	auto create_hdr_target = [&]() {
		auto hdr_color = app->tg->create_texture_2d(1, 1, 0, tg::Format_rgb_f16);
		auto hdr_depth = app->tg->create_texture_2d(1, 1, 0, tg::Format_depth);
		return app->tg->create_render_target(hdr_color, hdr_depth);
	};
	source_target      = create_hdr_target();
	destination_target = create_hdr_target();
}

void Camera::free() {
	for (auto &effect : post_effects) {
		effect.free();
	}
	tl::free(post_effects);
	//app->tg->free(source_target->color);
	//app->tg->free(source_target->depth);
	//app->tg->free(destination_target->color);
	//app->tg->free(destination_target->depth);
}
void Camera::resize_targets(v2u size) {
	app->tg->resize_texture(source_target->color, size);
	app->tg->resize_texture(source_target->depth, size);
	app->tg->resize_texture(destination_target->color, size);
	app->tg->resize_texture(destination_target->depth, size);
}
