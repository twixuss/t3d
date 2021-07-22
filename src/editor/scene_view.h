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
		current_camera_entity = camera_entity;
		current_camera = camera;
		current_viewport = viewport;
		render_scene(this);
		current_camera_entity = 0;
		current_camera = 0;
		current_viewport = {};
	}
	void free() {
		destroy(*camera_entity);
	}

	v3f get_drag_position(v2s mouse_position) {
		return {};
	}

	InputResponse on_input(InputEvent event) {
		switch (event.kind) {
			case InputEvent_key_down:   return on_key_down  (event.key_down);
			case InputEvent_mouse_down: return on_mouse_down(event.mouse_down);
			case InputEvent_mouse_up  : return on_mouse_up  (event.mouse_up  );
			case InputEvent_mouse_move: return on_mouse_move(event.mouse_move);
		}
		invalid_code_path();
		return {};
	}
	InputResponse on_key_down(InputEvent::KeyDown event) {
		switch (event.key) {
			case '1': manipulator_kind = Manipulate_position; break;
			case '2': manipulator_kind = Manipulate_rotation; break;
			case '3': manipulator_kind = Manipulate_scale; break;
		}
		return {};
	}
	InputResponse on_mouse_down(InputEvent::MouseDown event) {
		event.position -= viewport.position;
		if (event.button == 1) {
			flying = true;
			return {.kind = InputResponse_begin_drag, .sender = this};
		}

		return {};
	}
	InputResponse on_mouse_up(InputEvent::MouseUp event) {
		if (event.button == 1) {
			flying = false;
			return {InputResponse_end_grab};
		}
		return {};
	}
	InputResponse on_mouse_move(InputEvent::MouseMove event) {
		return {};
	}
};

SceneViewWindow *create_scene_view() {
	auto result = create_editor_window<SceneViewWindow>();
	result->kind = EditorWindow_scene_view;
	result->camera_entity = &entities.add();
	result->camera_entity->debug_name = format(u8"scene_camera_%", result);
	result->camera = &add_component<Camera>(*result->camera_entity);
	return result;
}
