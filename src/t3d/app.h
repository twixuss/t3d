#pragma once
#include <t3d/entity.h>
#include <t3d/component.h>
#include <t3d/material.h>
#include <t3d/components/camera.h>
#include <t3d/assets.h>
#include <t3d/scene.h>
#include <tl/time.h>
#include <tl/window.h>
#include <tl/font.h>
#include <tl/random.h>

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
	v4f color;
};


struct AppData {
	Allocator                   allocator;
	HashMap<Uid, ComponentInfo> component_infos;
	HashMap<Span<utf8>, Uid>    component_name_to_uid;

	xorshift64 uid_generator;

	List<Scene *> scenes;
	Scene *current_scene;

	bool is_editor;

	PreciseTimer frame_timer;
	f32 frame_time = 1 / 60.0f;
	f32 max_frame_time = 0.1f;
	f32 time;
	u32 frame_index;

	Window *window;
	bool did_resize = true;

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


	StaticMaskedBlockList<Material, 256> materials;

	Material surface_material;

	Entity *current_camera_entity;
	Camera *current_camera;
	v2s current_mouse_position;
	Cursor current_cursor;

	Assets assets;
};

inline void update_time() {
	app->frame_time = min(app->max_frame_time, reset(app->frame_timer));
	app->time += app->frame_time;
	app->frame_index += 1;
}
