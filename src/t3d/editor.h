#pragma once
#include <t3d/common.h>
#include <t3d/assets.h>
#include <t3d/editor/window.h>
#include <t3d/editor/input.h>
#include <t3d/scene.h>
#include <tl/font.h>

struct GuiKey {
	umm id;
	std::source_location location;
	bool operator==(GuiKey const &that) {
		return id == that.id && location == that.location;
	}
};

template <>
inline umm get_hash(GuiKey key) {
	return key.id * (umm)954277 + key.location.column() * (umm)152753 + key.location.line() * (umm)57238693 + (umm)key.location.file_name();
}

struct ButtonState {
	f32 hover_enter_t;
	f32 hover_stay_t;
	f32 press_t;
	f32 click_t;
	bool previously_hovered;
};

template <class T>
struct FieldState {
	bool editing;
	List<utf8> string;
	f32 caret_blink_time;
	T original_value;
	u32 caret_position;
	u32 selection_start;
	s32 text_offset;
};

struct ScrollBarState {
	u32 total_size;
	u32 scrolled_pixels;
};

enum GuiDrawKind : u8 {
	GuiDraw_none,
	GuiDraw_label,
	GuiDraw_rect_colored,
	GuiDraw_rect_textured,
};

struct GuiDraw {
	GuiDrawKind kind;
	aabb<v2s> viewport;
	aabb<v2s> scissor;
	union {
		struct {
			v4f color;
		} rect_colored;
		struct {
			tg::Texture2D *texture;
		} rect_textured;
		struct {
			v2s position;
			List<PlacedChar> placed_chars;
			SizedFont *font;
		} label;
	};
};

struct EditorData {
	HashMap<EditorWindowId, EditorWindow *> editor_windows;
	EditorWindowId editor_window_id_counter;

	EditorWindow *main_window;

	s32 debug_print_editor_window_hierarchy_tab;


	HashMap<GuiKey, ButtonState>            button_states;
	HashMap<GuiKey, FieldState<f32>>        float_field_states;
	HashMap<GuiKey, FieldState<List<utf8>>> text_field_states;
	HashMap<GuiKey, ScrollBarState>         scroll_bar_states;

	s32 current_property_y;

	List<GuiDraw> gui_draws;


	bool input_is_locked;
	v2s input_lock_mouse_position;


	u32 input_user_index;
	u32 focusable_input_user_index;
	u32 should_switch_focus_to = -1;
	u32 input_locker;

	DragAndDropKind drag_and_drop_kind;

	List<u8> drag_and_drop_data;

	bool should_unlock_input;

	::KeyInputState key_state[256 + 3];
	List<utf8> input_string;

	Assets assets;

	// All editor specific entities go here
	Scene *scene;
};
