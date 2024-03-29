#pragma once
#include <t3d/common.h>
#include "current.h"
#include "window_list.h"
#include <tl/memory_stream.h>

inline constexpr v4f background_color = {.08, .08, .08, 1};
inline constexpr v4f     middle_color = {.12, .12, .12, 1};
inline constexpr v4f foreground_color = {.18, .18, .18, 1};
inline constexpr v4f  highlight_color = {1.2, 1.2, 1.5, 1};
inline constexpr v4f  selection_color = {1.5, 1.5, 2.0, 1};

enum EditorWindowKind : u16 {
	EditorWindow_none,
	EditorWindow_split_view,
	EditorWindow_scene_view,
	EditorWindow_hierarchy_view,
	EditorWindow_property_view,
	EditorWindow_file_view,
	EditorWindow_tab_view,
};

struct EditorWindow;

using EditorWindowId = u32;

EditorWindowId get_new_editor_window_id();

template <class T>
T *create_editor_window(EditorWindowKind kind, EditorWindowId id = get_new_editor_window_id());

struct EditorWindow {
	EditorWindowId id;
	EditorWindow *parent;
	EditorWindowKind kind;
	tg::Rect viewport;
	Span<utf8> name;

	v2u (*_get_min_size)(void *_this);
	v2u get_min_size();

	void (*_resize)(void *_this, tg::Rect viewport);
	void resize(tg::Rect viewport);

	void (*_render)(void *_this);
	void render();

	void (*_free)(void *_this);
	void free();

	void (*_debug_print)(void *_this);
	void debug_print();

	void (*_serialize)(void *_this, StringBuilder &builder);
	void serialize(StringBuilder &builder);

	bool (*_deserialize)(void *_this, Stream &stream);
	bool deserialize(Stream &stream);
};

template <class T> void editor_window_resize(void *data, tg::Rect viewport) {
	if constexpr (&T::resize != &EditorWindow::resize) {
		return ((T *)data)->resize(viewport);
	}
}
template <class T> void editor_window_render(void *data) {
	if constexpr (&T::render != &EditorWindow::render) {
		return ((T *)data)->render();
	}
}
template <class T> void editor_window_free(void *data) {
	if constexpr (&T::free != &EditorWindow::free) {
		return ((T *)data)->free();
	}
}
template <class T> void editor_window_serialize(void *data, StringBuilder &builder) {
	if constexpr (&T::serialize != &EditorWindow::serialize) {
		return ((T *)data)->serialize(builder);
	}
}
template <class T> bool editor_window_deserialize(void *data, Stream &stream) {
	if constexpr (&T::deserialize != &EditorWindow::deserialize) {
		return ((T *)data)->deserialize(stream);
	}
	return true;
}
template <class T> v2u editor_window_get_min_size(void *data) {
	static_assert(&T::get_min_size != &EditorWindow::get_min_size, "get_min_size must be present in a window");
	return ((T *)data)->get_min_size();
}

void debug_print_editor_window_tabs();
void push_debug_print_editor_window_tab();
void pop_debug_print_editor_window_tab();

template <class T>
void editor_window_debug_print(void *data) {
	debug_print_editor_window_tabs();
	print("{} {}, parent={}\n", ((T *)data)->name, data, ((EditorWindow *)data)->parent);
	if constexpr (&T::debug_print != &EditorWindow::debug_print) {
		push_debug_print_editor_window_tab();
		((T *)data)->debug_print();
		pop_debug_print_editor_window_tab();
	}
}

#define c(name) struct name;
#define sep

ENUMERATE_WINDOWS

#undef sep
#undef c


#define c(name) name
#define sep ,

// Make sure every component is listed before including this file
template <class Window>
inline static constexpr u32 editor_window_type_id = type_index<Window,
	ENUMERATE_WINDOWS
>(0);

inline static constexpr u32 editor_window_type_count = type_count<
	ENUMERATE_WINDOWS
>();

#undef sep
#undef c

/*
#define c(name) u8#name##s
#define sep ,

Span<utf8> editor_window_names[] {
	ENUMERATE_WINDOWS
};

#undef sep
#undef c
*/

using EditorWindowInit        = void(*)(void *);
using EditorWindowGetMinSize  = v2u(*)(void *);
using EditorWindowResize      = void(*)(void *, tg::Rect);
using EditorWindowRender      = void(*)(void *);
using EditorWindowFree        = void(*)(void *);
using EditorWindowDebugPrint  = void(*)(void *);
using EditorWindowSerialize   = void(*)(void *, StringBuilder &);
using EditorWindowDeserialize = bool(*)(void *, Stream &);

struct EditorWindowMetadata {
	EditorWindowInit        init         = 0;
	EditorWindowGetMinSize  get_min_size = 0;
	EditorWindowResize      resize       = 0;
	EditorWindowRender      render       = 0;
	EditorWindowFree        free         = 0;
	EditorWindowDebugPrint  debug_print  = 0;
	EditorWindowSerialize   serialize    = 0;
	EditorWindowDeserialize deserialize  = 0;
	u32 size;
	u32 alignment;
};

template <class Window>
void editor_window_init(Window &_this) {}

template <class Window> void adapt_editor_window_init(void *_this) { return editor_window_init<Window>(*(Window *)_this); }
template <class Window> v2u  adapt_editor_window_get_min_size(void *_this) { return ((Window *)_this)->get_min_size(); }
template <class Window> void adapt_editor_window_resize(void *_this, tg::Rect viewport) { return ((Window *)_this)->resize(viewport); }
template <class Window> void adapt_editor_window_render(void *_this) { return ((Window *)_this)->render(); }
template <class Window> void adapt_editor_window_free(void *_this) { return ((Window *)_this)->free(); }
template <class Window> void adapt_editor_window_debug_print(void *_this) { return ((Window *)_this)->debug_print(); }
template <class Window> void adapt_editor_window_serialize(void *_this, StringBuilder &builder) { return ((Window *)_this)->serialize(builder); }
template <class Window> bool adapt_editor_window_deserialize(void *_this, Stream &stream) { return ((Window *)_this)->deserialize(stream); }

//extern EditorWindowMetadata editor_window_metadata[editor_window_type_count];

void insert_editor_window(EditorWindowId id, EditorWindow *result);

template <class T>
T *create_editor_window(EditorWindowKind kind, EditorWindowId id) {
	auto result = default_allocator.allocate<T>();
	result->_get_min_size = editor_window_get_min_size<T>;
	result->_resize       = editor_window_resize<T>;
	result->_render       = editor_window_render<T>;
	result->_free         = editor_window_free<T>;
	result->_debug_print  = editor_window_debug_print<T>;
	result->_serialize    = editor_window_serialize<T>;
	result->_deserialize  = editor_window_deserialize<T>;
	result->kind          = kind;
	result->id            = id;
	insert_editor_window(id, result);
	editor_window_init<T>(*result);
	return result;
}

/*
inline EditorWindow *create_editor_window(EditorWindowKind kind, EditorWindowId id) {
	auto &metadata = editor_window_metadata[kind];
	auto result = (EditorWindow *)default_allocator.allocate(metadata.size, metadata.alignment);
	result->_get_min_size = metadata.get_min_size;
	result->_resize       = metadata.resize;
	result->_render       = metadata.render;
	result->_free         = metadata.free;
	result->_debug_print  = metadata.debug_print;
	result->_serialize    = metadata.serialize;
	result->_deserialize  = metadata.deserialize;
	result->kind          = kind;
	result->id            = id;
	editor_windows.get_or_insert(id) = result;
	metadata.init(result);
	return result;
}
*/
