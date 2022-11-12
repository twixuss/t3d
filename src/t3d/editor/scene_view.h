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

void recompile_all_scripts();
void reload_all_scripts(bool recompile);
void build_executable();

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
	void resize(tg::Rect viewport) {
		this->viewport = viewport;
		for (auto &effect : camera->post_effects) {
			effect.resize((v2u)viewport.size());
		}
		camera->resize_targets((v2u)viewport.size());
	}
	void render() {
		begin_input_user();

		auto old_camera_entity = app->current_camera_entity;
		auto old_camera        = app->current_camera;
		auto old_viewport      = editor->current_viewport;
		defer {
			app->current_camera_entity = old_camera_entity;
			app->current_camera        = old_camera;
			editor->current_viewport   = old_viewport;
		};
		app->current_camera_entity = camera_entity;
		app->current_camera        = camera;
		editor->current_viewport   = viewport;

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
			camera->fov = clamp(camera->fov - app->window->mouse_wheel * radians(10), radians(30.0f), radians(120.0f));
		}

		v3f camera_move_direction = {};
		f32 target_camera_velocity = 0;

		f32 const mouse_scale = -0.003f;
		quaternion rotation_delta =
			quaternion_from_axis_angle({0,1,0}, app->window->mouse_delta.x * mouse_scale * camera->fov) *
			quaternion_from_axis_angle(camera_entity->rotation * v3f{1,0,0}, app->window->mouse_delta.y * mouse_scale * camera->fov);

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
						camera_velocity *= 1 + app->frame_time * 0.5f;
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

				camera_entity->position += camera_entity->rotation * V3f(app->window->mouse_delta * v2f{-1,1} / app->window->client_size.y, 0) * 5;

				break;
			}
			default: {
				break;
			}
		}
		if (movement_state != Movement_flying) {
			camera_velocity = 0;
		}
		camera_entity->position += camera_entity->rotation * camera_move_direction * app->frame_time * camera_velocity;

		app->tg->disable_scissor();
		render_scene(this);

		u32 const button_size = 32;

		if (!translate_icon) {
			translate_icon = app->tg->load_texture_2d(u8"../data/icons/translate.png"s, {.generate_mipmaps = true, .flip_y = true});
			rotate_icon    = app->tg->load_texture_2d(u8"../data/icons/rotate.png"s   , {.generate_mipmaps = true, .flip_y = true});
			scale_icon     = app->tg->load_texture_2d(u8"../data/icons/scale.png"s    , {.generate_mipmaps = true, .flip_y = true});
		}

		auto translate_viewport = editor->current_viewport;
		translate_viewport.min.x += 2;
		translate_viewport.min.y = editor->current_viewport.max.y - 2 - button_size;
		translate_viewport.max.x = translate_viewport.min.x + button_size;
		translate_viewport.max.y = translate_viewport.min.y + button_size;
		push_viewport(translate_viewport) if (button(translate_icon, (umm)this)) manipulator_kind = Manipulate_position;
		translate_viewport.min.x += button_size + 2;
		translate_viewport.max.x += button_size + 2;
		push_viewport(translate_viewport) if (button(rotate_icon, (umm)this)) manipulator_kind = Manipulate_rotation;
		translate_viewport.min.x += button_size + 2;
		translate_viewport.max.x += button_size + 2;
		push_viewport(translate_viewport) if (button(scale_icon, (umm)this)) manipulator_kind = Manipulate_scale;
		translate_viewport.min.x += button_size + 2;
		translate_viewport.max.x += button_size + 2;
		push_viewport(translate_viewport) if (button(u8"Camera"s, (umm)this)) selection.set(camera_entity);
		translate_viewport.min.x += button_size + 2;
		translate_viewport.max.x += button_size + 2;
		push_viewport(translate_viewport) if (button(u8"Reload scripts"s, (umm)this)) {
			reload_all_scripts(true);
		}
		translate_viewport.min.x += button_size + 2;
		translate_viewport.max.x += button_size + 2;
		push_viewport(translate_viewport) {
			if (button(u8"Build"s, (umm)this)) {
				build_executable();
			}
		}
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
	result->camera_entity = &editor->scene->create_entity(tformat("scene_camera_{}", result->id));
	result->camera_entity->flags |= Entity_editor_camera;
	result->camera = &add_component<Camera>(*result->camera_entity);
	result->camera->add_post_effect<Dither>();
	result->name = u8"Scene"s;
	return result;
}
