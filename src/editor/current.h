#pragma once
#include <t3d/entity.h>
#include <t3d/components/camera.h>

Window *window;

Entity *current_camera_entity;
Camera *current_camera;
v2s current_mouse_position;
tg::Viewport current_viewport;
tg::Viewport current_scissor;
Cursor current_cursor;

struct ViewportPusher {
	tg::Viewport old_viewport;
	tg::Viewport old_scissor;
	bool has_area;
	ViewportPusher(tg::Viewport new_viewport) {
		old_viewport = current_viewport;
		old_scissor  = current_scissor;

		current_viewport = new_viewport;
		current_scissor = intersection(new_viewport, current_scissor);

		has_area = volume(current_scissor) > 0;

		if (has_area) {
			tg::set_viewport(new_viewport);
			tg::set_scissor(current_scissor);
		}
	}
	~ViewportPusher() {
		current_viewport = old_viewport;
		current_scissor = old_scissor;
		tg::set_viewport(old_viewport);
		tg::set_scissor(old_scissor);
	}
	operator bool(){return has_area;}
};
#define push_current_viewport(new_viewport) tl_push(ViewportPusher, new_viewport)

v3f world_to_camera(v3f point) {
	return current_camera->world_to_camera(point);
}
v3f world_to_viewport(v4f point) {
	return map(current_camera->world_to_camera(point), {-1,-1,-1}, {1,1,1}, {0,0,0}, V3f((v2f)current_viewport.size(), 1));
}
v3f world_to_viewport(v3f point) {
	return world_to_viewport(V4f(point, 1));
}
v2s get_mouse_position_in_current_viewport() {
	return v2s{window->mouse_position.x, (s32)window->client_size.y - window->mouse_position.y} - current_viewport.min;
}
