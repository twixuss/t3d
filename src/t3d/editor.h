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

struct PanelState {
	v4f previous_color = {-1337};
	aabb<v2s> viewport;
};
struct LabelState {
	v4f previous_color = {-1337};
	aabb<v2s> viewport;
	List<utf8> text;
};
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
			v4f color;
		} label;
	};
};

struct ButtonTheme {
	v4f color = foreground_color;
	v4f hover_enter_color = foreground_color * highlight_color * 1.5f;
	v4f hover_stay_color = foreground_color * highlight_color;
	v4f press_color = {.1f, .1f, .1f, 1};
	v4f click_color = {.5f, .5f, 1, 1};
	f32 hover_enter_speed = 10;
	f32 hover_stay_speed = 20;
	f32 press_speed = 20;
	f32 click_speed = 10;
	u32 font_size = 12;
	s32 right_padding  = 0;
	s32 left_padding   = 0;
	s32 top_padding    = 0;
	s32 bottom_padding = 0;
	s32 content_padding = 2;
};

struct TextFieldTheme {
	v4f color = background_color;
	v4f hovered_color = {.15f, .15f, .15f, 1};
	v4f edit_color = {.2f, .2f, .1f, 1};
	u32 font_size = 0;
};

struct LabelTheme {
	v4f color = {1,1,1,1};
};

inline constexpr ButtonTheme default_button_theme;
inline constexpr TextFieldTheme default_text_field_theme;
inline constexpr LabelTheme default_label_theme;

struct EditorData {
	HashMap<EditorWindowId, EditorWindow *> editor_windows;
	EditorWindowId editor_window_id_counter;

	EditorWindow *main_window;

	s32 debug_print_editor_window_hierarchy_tab;


	HashMap<GuiKey, ButtonState>            button_states;
	HashMap<GuiKey, FieldState<f32>>        float_field_states;
	HashMap<GuiKey, FieldState<List<utf8>>> text_field_states;
	HashMap<GuiKey, ScrollBarState>         scroll_bar_states;
	HashMap<GuiKey, PanelState>             panel_states;
	HashMap<GuiKey, LabelState>             label_states;

	ButtonTheme    button_theme     = default_button_theme    ;
	TextFieldTheme text_field_theme = default_text_field_theme;
	LabelTheme     label_theme      = default_label_theme     ;

	tg::Viewport current_viewport;
	tg::Viewport current_scissor;

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

	Scene *scene;

	v2s get_mouse_position_in_current_viewport();
};

struct LabelThemePusher {
	LabelTheme old;
	LabelThemePusher() { old = editor->label_theme; }
	~LabelThemePusher() { editor->label_theme = old; }
	explicit operator bool() { return true; }
};
#define push_label_theme tl_push(LabelThemePusher)

struct ButtonThemePusher {
	ButtonTheme old;
	ButtonThemePusher() { old = editor->button_theme; }
	~ButtonThemePusher() { editor->button_theme = old; }
	explicit operator bool() { return true; }
};
#define push_button_theme tl_push(ButtonThemePusher)

inline void update_current_scissor() {
	editor->current_scissor = intersection(editor->current_viewport, editor->current_scissor);
}

struct ViewportPusher {
	tg::Viewport old_viewport;
	tg::Viewport old_scissor;
	bool has_area;
	ViewportPusher(tg::Viewport new_viewport) {
		old_viewport = editor->current_viewport;
		old_scissor  = editor->current_scissor;

		editor->current_viewport = new_viewport;
		update_current_scissor();
		/*
		editor->current_scissor = intersection(new_viewport, editor->current_scissor);
		has_area = volume(editor->current_scissor) > 0;

		if (has_area) {
			app->tg->set_viewport(new_viewport);
			app->tg->set_scissor(editor->current_scissor);
		}
		*/
	}
	~ViewportPusher() {
		editor->current_viewport = old_viewport;
		editor->current_scissor = old_scissor;
		//app->tg->set_viewport(old_viewport);
		//app->tg->set_scissor(old_scissor);
	}
	explicit operator bool() { return has_area; }
};
#define push_viewport(new_viewport) tl_push(ViewportPusher, new_viewport)
