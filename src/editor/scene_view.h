#pragma once
#include "window.h"
#include "../entity.h"
#include "manipulator.h"
#include "gui.h"

void render_scene(struct SceneViewWindow *);

t3d::Texture *translate_icon;
t3d::Texture *rotate_icon;
t3d::Texture *scale_icon;

struct SceneViewWindow : EditorWindow {
	Entity *camera_entity;
	Camera *camera;
	bool flying;
	ManipulateKind manipulator_kind;
	f32 camera_velocity;
	
	v2u get_min_size() {
		return {160, 160};
	}
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
			lock_input(this);
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

			f32 mouse_scale = -0.003f;
			camera_entity->rotation =
				quaternion_from_axis_angle({0,1,0}, window->mouse_delta.x * mouse_scale * camera->fov) *
				quaternion_from_axis_angle(camera_entity->rotation * v3f{1,0,0}, window->mouse_delta.y * mouse_scale * camera->fov) *
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

		u32 const button_size = 32;

		if (!translate_icon) {
			translate_icon = t3d::load_texture(tl_file_string("../data/icons/translate.png"ts), {.generate_mipmaps = true, .flip_y = true});
			rotate_icon    = t3d::load_texture(tl_file_string("../data/icons/rotate.png"ts)   , {.generate_mipmaps = true, .flip_y = true});
			scale_icon     = t3d::load_texture(tl_file_string("../data/icons/scale.png"ts)    , {.generate_mipmaps = true, .flip_y = true});
		}

		auto translate_viewport = current_viewport;
		translate_viewport.x += 2;
		translate_viewport.y = current_viewport.y + current_viewport.h - 2 - button_size;
		translate_viewport.w = button_size;
		translate_viewport.h = button_size;
		if (button(translate_viewport, translate_icon)) manipulator_kind = Manipulate_position;
		translate_viewport.x += button_size + 2;
		if (button(translate_viewport, rotate_icon)) manipulator_kind = Manipulate_rotation;
		translate_viewport.x += button_size + 2;
		if (button(translate_viewport, scale_icon)) manipulator_kind = Manipulate_scale;
	}
	void free() {
		destroy(*camera_entity);
	}
};

SceneViewWindow *create_scene_view() {
	auto result = create_editor_window<SceneViewWindow>(EditorWindow_scene_view);
	result->camera_entity = &create_entity("scene_camera_%", result);
	result->camera_entity->flags |= Entity_editor;
	result->camera = &add_component<Camera>(*result->camera_entity);
	return result;
}
