#pragma once
#include <t3d/entity.h>
#include <t3d/component.h>
#include <t3d/material.h>
#include <t3d/components/camera.h>
#include <t3d/editor/window.h>
#include <t3d/editor/input.h>
#include <t3d/assets.h>

struct GlobalConstants {
	m4 camera_rotation_projection_matrix;
	m4 world_to_camera_matrix;

	v3f camera_position;
	f32 _dummy;

	v3f camera_forward;
};
#define GLOBAL_CONSTANTS_SLOT 5


struct EntityConstants {
	m4 local_to_camera_matrix;
	m4 local_to_world_position_matrix;
	m4 local_to_world_normal_matrix;
	m4 object_rotation_matrix;
};
#define ENTITY_CONSTANTS_SLOT 6


struct LightConstants {
	m4 world_to_light_matrix;

	v3f light_position;
	f32 light_intensity;

	u32 light_index;
};
#define LIGHT_CONSTANTS_SLOT 7


struct SurfaceConstants {
	v4f color;
};


// Do we need this in runtime?
struct HandleConstants {
	m4 matrix = m4::identity();

	v3f color;
	f32 selected;

	v3f to_camera;
	f32 is_rotation;
};


struct BlitColorConstants {
	v4f color;
};

struct BlitTextureColorConstants {
	v4f color;
};

struct TextShaderConstants {
	v2f inv_half_viewport_size;
	v2f offset;
};

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

struct SharedData {
	Allocator                            allocator;
	HashMap<ComponentUID, ComponentInfo> component_infos;
	HashMap<Span<utf8>, ComponentUID>    component_name_to_uid;
	MaskedBlockList<Entity, 256>         entities;
	ComponentUID                         component_uid_counter;

	bool is_editor;

	PreciseTimer frame_timer;
	f32 frame_time = 1 / 60.0f;
	f32 max_frame_time = 0.1f;
	f32 time;
	u32 frame_index;

	Window *window;

	tg::State *tg;

	tg::Texture2D *white_texture;
	tg::Texture2D *black_texture;
	tg::Texture2D *default_light_mask;

	tg::TextureCube *sky_box_texture;
	tg::Shader *sky_box_shader;

	tg::TypedShaderConstants<GlobalConstants> global_constants;
	tg::TypedShaderConstants<EntityConstants> entity_constants;
	tg::TypedShaderConstants<LightConstants> light_constants;

	tg::Shader *shadow_map_shader;

	tg::TypedShaderConstants<HandleConstants> handle_constants;
	tg::Shader *handle_shader;

	tg::Shader *blit_texture_shader;

	tg::TypedShaderConstants<BlitColorConstants> blit_color_constants;
	tg::Shader *blit_color_shader;

	tg::TypedShaderConstants<BlitTextureColorConstants> blit_texture_color_constants;
	tg::Shader *blit_texture_color_shader;


	FontCollection *font_collection;
	tg::VertexBuffer *text_vertex_buffer;
	tg::Shader *text_shader;
	tg::TypedShaderConstants<TextShaderConstants> text_shader_constants;


	MaskedBlockList<Material, 256> materials;

	Material surface_material;

	Entity *current_camera_entity;
	Camera *current_camera;
	v2s current_mouse_position;
	tg::Viewport current_viewport;
	tg::Viewport current_scissor;
	Cursor current_cursor;

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


	::KeyInputState key_state[256 + 3];

	bool input_is_locked;
	v2s input_lock_mouse_position;

	List<utf8> input_string;

	u32 input_user_index;
	u32 focusable_input_user_index;
	u32 should_switch_focus_to = -1;
	u32 input_locker;

	DragAndDropKind drag_and_drop_kind;

	List<u8> drag_and_drop_data;

	bool should_unlock_input;

	Assets assets;
	Assets editor_assets;
};

inline void update_time() {
	shared->frame_time = min(shared->max_frame_time, reset(shared->frame_timer));
	shared->time += shared->frame_time;
	shared->frame_index += 1;
}

inline v3f world_to_camera(v3f point) {
	return shared->current_camera->world_to_camera(point);
}
inline v3f world_to_viewport(v4f point) {
	return map(shared->current_camera->world_to_camera(point), {-1,-1,-1}, {1,1,1}, {0,0,0}, V3f((v2f)shared->current_viewport.size(), 1));
}
inline v3f world_to_viewport(v3f point) {
	return world_to_viewport(V4f(point, 1));
}
inline v2s get_mouse_position_in_current_viewport() {
	return v2s{shared->window->mouse_position.x, (s32)shared->window->client_size.y - shared->window->mouse_position.y} - shared->current_viewport.min;
}
