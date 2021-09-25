#include "camera.h"
REGISTER_COMPONENT(Camera)

#include <t3d/app.h>

v3f Camera::world_to_camera(v4f point) {
	auto p = world_to_camera_matrix * point;
	return {p.xyz / p.w};
}
v3f Camera::world_to_camera(v3f point) {
	return world_to_camera(V4f(point, 1));
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
}
void Camera::resize_targets(v2u size) {
	app->tg->resize_texture(source_target->color, size);
	app->tg->resize_texture(source_target->depth, size);
	app->tg->resize_texture(destination_target->color, size);
	app->tg->resize_texture(destination_target->depth, size);
}
