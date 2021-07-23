#pragma once
#include "entity.h"
#include "components/camera.h"

Window *window;

Entity *current_camera_entity;
Camera *current_camera;
v2s current_mouse_position;
t3d::Viewport current_viewport;

struct ViewportPusher {
	t3d::Viewport old_viewport;
	ViewportPusher(t3d::Viewport new_viewport) {
		old_viewport = current_viewport;
		current_viewport = new_viewport;
	}
	~ViewportPusher() {
		current_viewport = old_viewport;
	}
	operator bool(){return true;}
};
#define push_current_viewport(new_viewport) tl_push(ViewportPusher, new_viewport)

v3f world_to_camera(v3f point) {
	return current_camera->world_to_camera(point);
}
v3f world_to_viewport(v4f point) {
	return map(current_camera->world_to_camera(point), {-1,-1,-1}, {1,1,1}, {0,0,0}, V3f((v2f)current_viewport.size, 1));
}
v3f world_to_viewport(v3f point) {
	return world_to_viewport(V4f(point, 1));
}
v2s get_mouse_position_in_current_viewport() {
	return v2s{window->mouse_position.x, (s32)window->client_size.y - window->mouse_position.y} - current_viewport.position;
}
