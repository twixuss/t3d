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
	enum MovementState : u8 {
		Movement_none,
		Movement_flying,
		Movement_panning,
		Movement_orbiting,
	};

	Entity *camera_entity;
	Camera *camera;
	MovementState movement_state;
	ManipulateKind manipulator_kind;
	f32 camera_velocity;
	
	v2u get_min_size() {
		return {160, 160};
	}
	void resize(t3d::Viewport viewport) {
		this->viewport = viewport;
		for (auto &effect : camera->post_effects) {
			effect.resize((v2u)viewport.size());
		}
		t3d::resize_texture(camera->source_target->color, (v2u)viewport.size());
		t3d::resize_texture(camera->source_target->depth, (v2u)viewport.size());
		t3d::resize_texture(camera->destination_target->color, (v2u)viewport.size());
		t3d::resize_texture(camera->destination_target->depth, (v2u)viewport.size());
	}
	void render() {
		begin_input_user();

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
		
		if (movement_state == Movement_none) {
			if (mouse_down(1)) movement_state = Movement_flying;
			if (mouse_down(2)) movement_state = key_held(Key_shift) ? Movement_panning : Movement_orbiting;

			if (movement_state != Movement_none) {
				lock_input();
			}
		}
		switch (movement_state) {
			case Movement_flying: {
				if (mouse_up(1)) {
					movement_state = Movement_none;
					unlock_input();
				}
				break;
			}
			case Movement_panning:
			case Movement_orbiting: {
				if (mouse_up(2)) {
					movement_state = Movement_none;
					unlock_input();
				}
				break;
			}
		}
		//if (movement_state != Movement_none)
		//	current_cursor = Cursor_none;

		if (key_down('1')) manipulator_kind = Manipulate_position;
		if (key_down('2')) manipulator_kind = Manipulate_rotation;
		if (key_down('3')) manipulator_kind = Manipulate_scale;
		
		if (key_held(Key_control)) {
			camera->fov = clamp(camera->fov - window->mouse_wheel * radians(10), radians(30.0f), radians(120.0f));
		}

		v3f camera_move_direction = {};
		f32 target_camera_velocity = 0;

		f32 const mouse_scale = -0.003f;
		quaternion rotation_delta = 
			quaternion_from_axis_angle({0,1,0}, window->mouse_delta.x * mouse_scale * camera->fov) *
			quaternion_from_axis_angle(camera_entity->rotation * v3f{1,0,0}, window->mouse_delta.y * mouse_scale * camera->fov);

		switch (movement_state) {
			case Movement_flying: {
				camera_entity->rotation = rotation_delta * camera_entity->rotation;

				bool accelerate = false;
				if (key_held(Key_d)) { camera_move_direction.x += 1; accelerate = true; }
				if (key_held(Key_a)) { camera_move_direction.x -= 1; accelerate = true; }
				if (key_held(Key_e)) { camera_move_direction.y += 1; accelerate = true; }
				if (key_held(Key_q)) { camera_move_direction.y -= 1; accelerate = true; }
				if (key_held(Key_s)) { camera_move_direction.z += 1; accelerate = true; }
				if (key_held(Key_w)) { camera_move_direction.z -= 1; accelerate = true; }
				if (accelerate) {
					if (camera_velocity < 1) {
						camera_velocity = 1;
					} else {
						camera_velocity *= 1 + frame_time * 0.5f;
					}
				} else {
					camera_velocity = 0;
				}
				break;
			}
			case Movement_orbiting: {
				v3f around = camera_entity->position + camera_entity->forward() * 5;

				camera_entity->position = rotation_delta * (camera_entity->position - around) + around;
				camera_entity->rotation = rotation_delta * camera_entity->rotation;

				break;
			}
			case Movement_panning: {

				camera_entity->position += camera_entity->rotation * V3f(window->mouse_delta * v2f{-1,1} / window->client_size.y, 0) * 5;

				break;
			}
			default: {
				break;
			}
		}
		if (movement_state != Movement_flying) {
			camera_velocity = 0;
		}
		camera_entity->position += camera_entity->rotation * camera_move_direction * frame_time * camera_velocity;

		render_scene(this);

		u32 const button_size = 32;

		if (!translate_icon) {
			translate_icon = t3d::load_texture(tl_file_string("../data/icons/translate.png"ts), {.generate_mipmaps = true, .flip_y = true});
			rotate_icon    = t3d::load_texture(tl_file_string("../data/icons/rotate.png"ts)   , {.generate_mipmaps = true, .flip_y = true});
			scale_icon     = t3d::load_texture(tl_file_string("../data/icons/scale.png"ts)    , {.generate_mipmaps = true, .flip_y = true});
		}

		auto translate_viewport = current_viewport;
		translate_viewport.min.x += 2;
		translate_viewport.min.y = current_viewport.max.y - 2 - button_size;
		translate_viewport.max.x = translate_viewport.min.x + button_size;
		translate_viewport.max.y = translate_viewport.min.y + button_size;
		if (button(translate_viewport, translate_icon)) manipulator_kind = Manipulate_position;
		translate_viewport.min.x += button_size + 2;
		translate_viewport.max.x += button_size + 2;
		if (button(translate_viewport, rotate_icon)) manipulator_kind = Manipulate_rotation;
		translate_viewport.min.x += button_size + 2;
		translate_viewport.max.x += button_size + 2;
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
