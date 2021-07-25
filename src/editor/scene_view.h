#pragma once
#include "window.h"
#include "../entity.h"
#include "manipulator.h"

void render_scene(struct SceneViewWindow *);

struct SceneViewWindow : EditorWindow {
	Entity *camera_entity;
	Camera *camera;
	bool flying;
	ManipulateKind manipulator_kind;
	f32 camera_velocity;

	void resize(t3d::Viewport viewport) {
		this->viewport = viewport;
		for (auto &effect : camera->post_effects) {
			effect.resize(viewport.size);
		}
		t3d::resize_texture(camera->source_target->color, viewport.size);
		t3d::resize_texture(camera->source_target->depth, viewport.size);
		t3d::resize_texture(camera->destination_target->color, viewport.size);
		t3d::resize_texture(camera->destination_target->depth, viewport.size);
	}
	void render() {
		auto old_camera_entity = current_camera_entity;
		auto old_camera        = current_camera;
		auto old_viewport      = current_viewport;
		defer {
			current_camera_entity = old_camera_entity;
			current_camera        = old_camera;
			current_viewport      = old_viewport;
		};
		current_camera_entity = camera_entity;
		current_camera = camera;
		current_viewport = viewport;
		
		if (mouse_down(1)) {
			flying = true;
			lock_input();
		}
		if (mouse_up_no_lock(1)) {
			flying = false;
			unlock_input();
		}

		if (key_down('1')) manipulator_kind = Manipulate_position;
		if (key_down('2')) manipulator_kind = Manipulate_rotation;
		if (key_down('3')) manipulator_kind = Manipulate_scale;
		
		if (key_held(Key_control)) {
			camera->fov = clamp(camera->fov - window->mouse_wheel * radians(10), radians(30.0f), radians(120.0f));
		}

		v3f camera_position_delta = {};
		if (flying) {

			camera_entity->rotation =
				quaternion_from_axis_angle({0,1,0}, window->mouse_delta.x * -0.005f * camera->fov) *
				quaternion_from_axis_angle(camera_entity->rotation * v3f{1,0,0}, window->mouse_delta.y * -0.005f * camera->fov) *
				camera_entity->rotation;

			if (key_held(Key_d)) camera_position_delta.x += 1;
			if (key_held(Key_a)) camera_position_delta.x -= 1;
			if (key_held(Key_e)) camera_position_delta.y += 1;
			if (key_held(Key_q)) camera_position_delta.y -= 1;
			if (key_held(Key_s)) camera_position_delta.z += 1;
			if (key_held(Key_w)) camera_position_delta.z -= 1;
			if (all_true(camera_position_delta == v3f{})) {
				camera_velocity = 1;
			} else {
				camera_velocity += frame_time;
			}
		}
		//camera_entity->position += m4::rotation_r_zxy(camera_entity->rotation) * camera_position_delta * frame_time * camera_velocity;
		camera_entity->position += camera_entity->rotation * camera_position_delta * frame_time * camera_velocity;

		render_scene(this);
	}
	void free() {
		destroy(*camera_entity);
	}
};

SceneViewWindow *create_scene_view() {
	auto result = create_editor_window<SceneViewWindow>();
	result->kind = EditorWindow_scene_view;
	result->camera_entity = &create_entity("scene_camera_%", result);
	result->camera_entity->flags |= Entity_editor;
	result->camera = &add_component<Camera>(*result->camera_entity);
	return result;
}
