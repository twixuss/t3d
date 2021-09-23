#pragma once
#include <t3d/editor/window.h>
#include <t3d/entity.h>
#include <t3d/manipulator.h>
#include <t3d/gui.h>
#include <t3d/selection.h>
#include <t3d/post_effects/dither.h>

void render_scene(struct SceneView *);

tg::Texture2D *translate_icon;
tg::Texture2D *rotate_icon;
tg::Texture2D *scale_icon;

struct SceneView : EditorWindow {
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
	void resize(tg::Viewport viewport) {
		this->viewport = viewport;
		for (auto &effect : camera->post_effects) {
			effect.resize((v2u)viewport.size());
		}
		camera->resize_targets((v2u)viewport.size());
	}
	void render() {
		begin_input_user();

		auto old_camera_entity = shared->current_camera_entity;
		auto old_camera        = shared->current_camera;
		auto old_viewport      = shared->current_viewport;
		defer {
			shared->current_camera_entity = old_camera_entity;
			shared->current_camera        = old_camera;
			shared->current_viewport      = old_viewport;
		};
		shared->current_camera_entity = camera_entity;
		shared->current_camera = camera;
		shared->current_viewport = viewport;

		if (movement_state == Movement_none) {
			if (mouse_down(1)) movement_state = Movement_flying;
			if (mouse_down(2)) movement_state = key_held(Key_shift) ? Movement_panning : Movement_orbiting;

			if (movement_state == Movement_none) {
				select_entity();
			} else {
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
			camera->fov = clamp(camera->fov - shared->window->mouse_wheel * radians(10), radians(30.0f), radians(120.0f));
		}

		v3f camera_move_direction = {};
		f32 target_camera_velocity = 0;

		f32 const mouse_scale = -0.003f;
		quaternion rotation_delta =
			quaternion_from_axis_angle({0,1,0}, shared->window->mouse_delta.x * mouse_scale * camera->fov) *
			quaternion_from_axis_angle(camera_entity->rotation * v3f{1,0,0}, shared->window->mouse_delta.y * mouse_scale * camera->fov);

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
						camera_velocity *= 1 + shared->frame_time * 0.5f;
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

				camera_entity->position += camera_entity->rotation * V3f(shared->window->mouse_delta * v2f{-1,1} / shared->window->client_size.y, 0) * 5;

				break;
			}
			default: {
				break;
			}
		}
		if (movement_state != Movement_flying) {
			camera_velocity = 0;
		}
		camera_entity->position += camera_entity->rotation * camera_move_direction * shared->frame_time * camera_velocity;

		shared->tg->disable_scissor();
		render_scene(this);

		u32 const button_size = 32;

		if (!translate_icon) {
			translate_icon = shared->tg->load_texture_2d(tl_file_string("../data/icons/translate.png"ts), {.generate_mipmaps = true, .flip_y = true});
			rotate_icon    = shared->tg->load_texture_2d(tl_file_string("../data/icons/rotate.png"ts)   , {.generate_mipmaps = true, .flip_y = true});
			scale_icon     = shared->tg->load_texture_2d(tl_file_string("../data/icons/scale.png"ts)    , {.generate_mipmaps = true, .flip_y = true});
		}

		auto translate_viewport = shared->current_viewport;
		translate_viewport.min.x += 2;
		translate_viewport.min.y = shared->current_viewport.max.y - 2 - button_size;
		translate_viewport.max.x = translate_viewport.min.x + button_size;
		translate_viewport.max.y = translate_viewport.min.y + button_size;
		if (button(translate_viewport, translate_icon, (umm)this)) manipulator_kind = Manipulate_position;
		translate_viewport.min.x += button_size + 2;
		translate_viewport.max.x += button_size + 2;
		if (button(translate_viewport, rotate_icon, (umm)this)) manipulator_kind = Manipulate_rotation;
		translate_viewport.min.x += button_size + 2;
		translate_viewport.max.x += button_size + 2;
		if (button(translate_viewport, scale_icon, (umm)this)) manipulator_kind = Manipulate_scale;
		translate_viewport.min.x += button_size + 2;
		translate_viewport.max.x += button_size + 2;
		if (button(translate_viewport, u8"Camera"s, (umm)this)) selection.set(camera_entity);
	}
	void select_entity() {
		//for_each_component_of_type(MeshRenderer, renderer) {
		//	renderer.mesh->positions;
		//};
	}
	void free() {
		destroy_entity(*camera_entity);
	}
	void serialize(StringBuilder &builder) {
		append_bytes(builder, camera_entity->position);
		append_bytes(builder, camera_entity->rotation);
		append_bytes(builder, camera->fov);
	}
	bool deserialize(Stream &stream) {

#define read_bytes(value) if (!stream.read(value_as_bytes(value))) { print(Print_error, "Failed to deserialize editor window: no data for field '" #value "'\n"); return 0; }

		read_bytes(camera_entity->position);
		read_bytes(camera_entity->rotation);
		read_bytes(camera->fov);

#undef read_bytes

	}
};

SceneView *create_scene_view() {
	auto result = create_editor_window<SceneView>(EditorWindow_scene_view);
	result->camera_entity = &create_entity("scene_camera_%", result->id);
	result->camera_entity->flags |= Entity_editor_camera;
	result->camera = &add_component<Camera>(*result->camera_entity);
	result->camera->add_post_effect<Dither>();
	result->name = u8"Scene"s;
	return result;
}
