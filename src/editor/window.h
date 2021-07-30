#pragma once
#include <t3d.h>
#include "current.h"

v4f background_color = {.12, .12, .12, 1};

enum EditorWindowKind : u16 {
	EditorWindow_none,
	EditorWindow_split_view,
	EditorWindow_scene_view,
	EditorWindow_hierarchy_view,
	EditorWindow_property_view,
	EditorWindow_file_view,
};

struct EditorWindow/* : InputHandler */{
	EditorWindowKind kind;
	t3d::Viewport viewport;

	v2u (*_get_min_size)(void *_this);
	v2u get_min_size() {
		return _get_min_size(this);
	}
	
	void (*_resize)(void *_this, t3d::Viewport viewport);
	void resize(t3d::Viewport viewport) {
		this->viewport = viewport;
		return _resize(this, viewport);
	}

	void (*_render)(void *_this);
	void render() {
		push_current_viewport(viewport) {
			return _render(this);
		};
	}

	void (*_free)(void *_this);
	void free() {
		return _free(this);
	}
};

template <class T> void editor_window_resize(void *data, t3d::Viewport viewport) { return ((T *)data)->resize(viewport); }
template <class T> void editor_window_render(void *data) { return ((T *)data)->render(); }
template <class T> void editor_window_free(void *data) { return ((T *)data)->free(); }
template <class T> v2u editor_window_get_min_size(void *data) { return ((T *)data)->get_min_size(); }

template <class T>
T *create_editor_window(EditorWindowKind kind) {
	auto result = default_allocator.allocate<T>();
	result->_get_min_size = editor_window_get_min_size<T>;
	result->_resize       = editor_window_resize<T>;
	result->_render       = editor_window_render<T>;
	result->_free         = editor_window_free<T>;
	result->kind          = kind;
	return result;
}
