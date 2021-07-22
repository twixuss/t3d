#pragma once
#include <t3d.h>
/*
enum InputResponseKind {
	InputResponse_none,
	InputResponse_begin_drag,
	InputResponse_end_grab,
};

struct InputResponse {
	InputResponseKind kind;
	struct InputHandler *sender;
};

struct InputHandler {
	InputResponse (*_on_input)(void *data, InputEvent event);
	InputResponse on_input(InputEvent event) {
		return _on_input(this, event);
	}
};

template <class T> InputResponse input_handler_on_input(void *data, InputEvent event) { return ((T *)data)->on_input(event); }
*/

enum EditorWindowKind : u16 {
	EditorWindow_none,
	EditorWindow_scene_view,
};

struct EditorWindow/* : InputHandler */{
	EditorWindowKind kind;
	t3d::Viewport viewport;
	void (*_resize)(void *data, t3d::Viewport viewport);
	void resize(t3d::Viewport viewport) {
		_resize(this, viewport);
	}

	void (*_render)(void *data);
	void render() {
		_render(this);
	}

	void (*_free)(void *data);
	void free() {
		_free(this);
	}
};

template <class T> void editor_window_resize(void *data, t3d::Viewport viewport) { ((T *)data)->resize(viewport); }
template <class T> void editor_window_render(void *data) { ((T *)data)->render(); }
template <class T> void editor_window_free(void *data) { ((T *)data)->free(); }

template <class T>
T *create_editor_window() {
	auto result = default_allocator.allocate<T>();
	result->_resize   = editor_window_resize<T>;
	result->_render   = editor_window_render<T>;
	result->_free     = editor_window_free<T>;
	//result->_on_input = input_handler_on_input<T>;
	return result;
}
