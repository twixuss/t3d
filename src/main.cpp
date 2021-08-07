#include "tl.h"
#include <source_location>
#include <tl/common.h>
tl::umm get_hash(struct ManipulatorStateKey const &);

#include "tl.h"
#include "../dep/tl/include/tl/masked_block_list.h"
#include "component.h"
#include "components/light.h"
#include "components/mesh_renderer.h"
#include "components/camera.h"
#include "components/rotator.h"
#include "editor/window.h"
#include "editor/scene_view.h"
#include "editor/hierarchy_view.h"
#include "editor/split_view.h"
#include "editor/property_view.h"
#include "editor/file_view.h"
#include "editor/tab_view.h"
#include "editor/input.h"
#include "serialize.h"
#include "assets.h"

#define c(name) { \
.init         = adapt_editor_window_init<name>, \
.get_min_size = editor_window_get_min_size<name>, \
.resize       = editor_window_resize<name>, \
.render       = editor_window_render<name>, \
.free         = editor_window_free<name>, \
.debug_print  = editor_window_debug_print<name>, \
.serialize    = editor_window_serialize<name>, \
.deserialize  = editor_window_deserialize<name>, \
.size         = sizeof(name), \
.alignment    = alignof(name), \
}
#define sep ,

EditorWindowMetadata editor_window_metadata[editor_window_type_count] = {
	ENUMERATE_WINDOWS
};


#undef sep
#undef c

static PreciseTimer frame_timer;
bool is_editor;

struct GlobalConstants {
	m4 camera_rotation_projection_matrix;
	m4 world_to_camera_matrix;

	v3f camera_position;
	f32 _dummy;

	v3f camera_forward;
};
tg::TypedShaderConstants<GlobalConstants> global_constants;
#define GLOBAL_CONSTANTS_SLOT 15


struct EntityConstants {
	m4 local_to_camera_matrix;
	m4 local_to_world_position_matrix;
	m4 local_to_world_normal_matrix;
	m4 object_rotation_matrix;
};
tg::TypedShaderConstants<EntityConstants> entity_constants;
#define ENTITY_CONSTANTS_SLOT 14


struct LightConstants {
	m4 world_to_light_matrix;

	v3f light_position;
	f32 light_intensity;

	u32 light_index;
};
tg::TypedShaderConstants<LightConstants> light_constants;
#define LIGHT_CONSTANTS_SLOT 13


struct AverageComputeData {
	u32 sum;
};
#define AVERAGE_COMPUTE_SLOT 12

tg::Shader *shadow_map_shader;

struct HandleConstants {
	m4 matrix = m4::identity();

	v3f color;
	f32 selected;
	
	v3f to_camera;
	f32 is_rotation;
};
tg::TypedShaderConstants<HandleConstants> handle_constants;
tg::Shader *handle_shader;

Material surface_material;
struct SurfaceConstants {
	v4f color;
};

#define shader_value_location(struct, member) tg::ShaderValueLocation{offsetof(struct, member), sizeof(struct::member)}

u32 fps_counter;
u32 fps_counter_result;
f32 fps_timer;

Mesh *suzanne_mesh;
Mesh *floor_mesh;
Mesh *handle_sphere_mesh;
Mesh *handle_circle_mesh;
Mesh *handle_tangent_mesh;
Mesh *handle_axis_x_mesh;
Mesh *handle_axis_y_mesh;
Mesh *handle_axis_z_mesh;
Mesh *handle_arrow_x_mesh;
Mesh *handle_arrow_y_mesh;
Mesh *handle_arrow_z_mesh;
Mesh *handle_plane_x_mesh;
Mesh *handle_plane_y_mesh;
Mesh *handle_plane_z_mesh;


m4 local_to_world_position(v3f position, quaternion rotation, v3f scale) {
	return m4::translation(position) * (m4)rotation * m4::scale(scale);
}

m4 local_to_world_normal(quaternion rotation, v3f scale) {
	return (m4)rotation * m4::scale(1.0f / scale);
}

#define SHADOW_MAP_TEXTURE_SLOT 15
#define LIGHT_TEXTURE_SLOT      14
#define LIGHTMAP_TEXTURE_SLOT	13

tg::Shader *create_shader(Span<utf8> source) {
	auto shader_header = u8R"(
#ifdef GL_core_profile
#extension GL_ARB_shading_language_420pack : enable
#define float2 vec2
#define float3 vec3
#define float4 vec4
float saturate(float a){return clamp(a,0,1);}
float2 saturate(float2 a){return clamp(a,0,1);}
float3 saturate(float3 a){return clamp(a,0,1);}
float4 saturate(float4 a){return clamp(a,0,1);}

layout(binding=)" STRINGIZE(GLOBAL_CONSTANTS_SLOT) R"(, std140) uniform global_uniforms {
	mat4 camera_rotation_projection_matrix;
	mat4 world_to_camera_matrix;

	vec3 camera_position;
	float _dummy;

	vec3 camera_forward;
};

layout(binding=)" STRINGIZE(ENTITY_CONSTANTS_SLOT) R"(, std140) uniform entity_uniforms {
	mat4 local_to_camera_matrix;
	mat4 local_to_world_position_matrix;
	mat4 local_to_world_normal_matrix;
	mat4 object_rotation_matrix;
};

layout(binding=)" STRINGIZE(LIGHT_CONSTANTS_SLOT) R"(, std140) uniform light_uniforms {
	mat4 world_to_light_matrix;

	vec3 light_position;
	float light_intensity;

	uint light_index;
};

layout(binding=)" STRINGIZE(SHADOW_MAP_TEXTURE_SLOT) R"() uniform sampler2DShadow shadow_map;
layout(binding=)" STRINGIZE(LIGHT_TEXTURE_SLOT) R"() uniform sampler2D light_texture;
layout(binding=)" STRINGIZE(LIGHTMAP_TEXTURE_SLOT) R"() uniform sampler2D lightmap_texture;

#endif

float pow2(float x){return x*x;}
float pow4(float x){return pow2(x*x);}
float pow5(float x){return pow4(x)*x;}

float length_squared(float2 x){return dot(x,x);}
float length_squared(float3 x){return dot(x,x);}
float length_squared(float4 x){return dot(x,x);}

#define pi 3.1415926535897932384626433832795


float trowbridge_reitz_distribution(float roughness, float NH) {
	float r2 = roughness*roughness;
	return r2 / (pi * pow2(pow2(NH)*(r2-1)+1));
}
float beckmann_distribution(float roughness, float NH) {
	float r2 = roughness*roughness;
	return exp(-pow2(sqrt(1 - NH*NH)/NH)/r2)
		/
		(pi*r2*pow4(NH));
}

float schlick_geometry_direct(float roughness, float NV) {
	float k = pow2(roughness + 1) / 8;
	return NV / (NV * (1 - k) + k);
}
float smith_geometry_direct(float roughness, float NV, float NL) {
	return schlick_geometry_direct(roughness, NV)
		 * schlick_geometry_direct(roughness, NL);
}
float3 fresnel_schlick(float3 F0, float NV) {
	return F0 + (1 - F0) * pow5(1 - NV);
}


float3 pbr(float3 albedo, float3 N, float3 L, float3 V) {
	float3 H = normalize(L + V);
	float NV = max(0.001, dot(N, V));
	float NL = max(0.001, dot(N, L));
	float NH = dot(N, H);
	float VH = dot(V, H);

	float roughness = 0.01f;

	float metalness = 0;
	float3 F0 = mix(vec3(0.04), albedo, metalness);

	float D = trowbridge_reitz_distribution(roughness, NH);
	float3 F = fresnel_schlick(F0, NV);
	float G = smith_geometry_direct(roughness, NV, NL);

	float3 specular = D * F * G / (pi * NV * NL);

	float3 diffuse = albedo * NL * (1 - metalness) / pi * (1 - specular);

	return diffuse + specular;
}

float sample_shadow_map(sampler2DShadow shadow_map, float3 light_space, float bias) {
	float light = 0;
	const int shadow_sample_radius = 2;
	if (saturate(light_space) == light_space) {
		for (int y = -shadow_sample_radius; y <= shadow_sample_radius; y += 1) {
			for (int x = -shadow_sample_radius; x <= shadow_sample_radius; x += 1) {
				light += textureOffset(shadow_map, vec3(light_space.xy, light_space.z - bias), ivec2(x, y));
			}
		}
		light *= 1 / pow2(shadow_sample_radius * 2 + 1);
	}
	return light;
}

#ifdef VERTEX_SHADER
#define V2F out
#else
#define V2F in
#endif

)"s;
	return tg::create_shader(with(temporary_allocator, concatenate(shader_header, source)));
}

tg::ComputeShader *average_shader;
tg::ComputeBuffer *average_shader_buffer;

tg::TextureCube *sky_box_texture;
tg::Shader *sky_box_shader;

tg::Texture2D *floor_lightmap;

void render_scene(SceneView *view) {
	timed_function();

	tg::disable_scissor();

	//print("%\n", to_euler_angles(quaternion_from_euler(0, time, time)));
	//selected_entity->qrotation = quaternion_from_euler(to_euler_angles(quaternion_from_euler(0, time, time)));
	
	auto &camera = *view->camera;
	auto &camera_entity = *view->camera_entity;

	v2s mouse_position = v2s{
		window->mouse_position.x,
		((s32)window->client_size.y - window->mouse_position.y),
	} - view->viewport.min;

	m4 camera_projection_matrix = m4::perspective_right_handed((f32)view->viewport.size().x / view->viewport.size().y, camera.fov, 0.1f, 100.0f);
	m4 camera_translation_matrix = m4::translation(-camera_entity.position);
	//m4 camera_rotation_matrix = m4::rotation_r_yxz(-camera_entity.rotation);
	//m4 camera_rotation_matrix = m4::rotation_r_yxz(-to_euler_angles(camera_entity.rotation));
	m4 camera_rotation_matrix = transpose((m4)camera_entity.rotation);
	camera.world_to_camera_matrix = camera_projection_matrix * camera_rotation_matrix * camera_translation_matrix;

	tg::update_shader_constants(global_constants, {
		.camera_rotation_projection_matrix = camera_projection_matrix * camera_rotation_matrix,
		.world_to_camera_matrix = camera.world_to_camera_matrix, 
		.camera_position = camera_entity.position,
		//.camera_forward = m3::rotation_r_zxy(camera_entity.rotation) * v3f{0,0,-1},
		.camera_forward = camera_entity.rotation * v3f{0,0,-1},
	});

	tg::set_render_target(camera.destination_target);
	tg::set_viewport(camera.destination_target->color->size);
	tg::clear(camera.destination_target, tg::ClearFlags_color | tg::ClearFlags_depth, {.9,.1,.9,1}, 1);
	
	tg::set_topology(tg::Topology_triangle_list);

	tg::set_rasterizer({
		.depth_test = true,
		.depth_write = true,
		.depth_func = tg::Comparison_less,
	});
	tg::disable_blend();

	u32 light_index = 0;
	for_each_component_of_type(Light, light) {
		timed_block("Light"s);

		defer {
			++light_index;
		};

		auto &light_entity = entities[light.entity_index];

		tg::update_shader_constants(light_constants, {
			.world_to_light_matrix = light.world_to_light_matrix,
			.light_position = light_entity.position,
			.light_intensity = light.intensity,
			.light_index = light_index,
		});


		tg::set_texture(light.shadow_map->depth, SHADOW_MAP_TEXTURE_SLOT);
		tg::set_texture(light.mask, LIGHT_TEXTURE_SLOT);
		for_each_component_of_type(MeshRenderer, mesh_renderer) {
			timed_block("MeshRenderer"s);
			auto &mesh_entity = entities[mesh_renderer.entity_index];

			auto material = mesh_renderer.material;
			if (!material) {
				material = &surface_material;
			}

			tg::set_shader(material->shader);
			tg::set_shader_constants(material->constants, 0);


			//entity_data.local_to_camera_matrix = camera.world_to_camera_matrix * m4::translation(mesh_entity.position) * m4::rotation_r_zxy(mesh_entity.rotation);
			m4 local_to_world = m4::translation(mesh_entity.position) * (m4)mesh_entity.rotation * m4::scale(mesh_entity.scale);
			tg::update_shader_constants(entity_constants, {
				.local_to_camera_matrix = camera.world_to_camera_matrix * local_to_world,
				.local_to_world_position_matrix = local_to_world,
				.local_to_world_normal_matrix = (m4)mesh_entity.rotation * m4::scale(1 / mesh_entity.scale),
			});
			tg::set_texture(mesh_renderer.lightmap, LIGHTMAP_TEXTURE_SLOT);
			draw_mesh(mesh_renderer.mesh);
		};
		tg::set_blend(tg::BlendFunction_add, tg::Blend_one, tg::Blend_one);
		tg::set_rasterizer(tg::get_rasterizer()
			.set_depth_test(true)
			.set_depth_write(false)
			.set_depth_func(tg::Comparison_equal)
		);
	};

	tg::set_rasterizer(tg::get_rasterizer()
		.set_depth_test(true)
		.set_depth_write(false)
		.set_depth_func(tg::Comparison_equal)
	);
	tg::disable_blend();

	tg::set_shader(sky_box_shader);
	tg::set_texture(sky_box_texture, 0);
	tg::draw(36);

	swap(camera.source_target, camera.destination_target);

	{
		timed_block("Post effects"s);
		if (is_editor) {
			for (auto &effect : camera.post_effects) {
				effect.render(camera.source_target, camera.destination_target);
				swap(camera.source_target, camera.destination_target);
			}

			tg::set_render_target(tg::back_buffer);
			tg::set_viewport(current_viewport);
			blit(camera.source_target->color);
		} else {
			for (auto &effect : camera.post_effects) {
				if (&effect == &camera.post_effects.back()) {
					effect.render(camera.source_target, tg::back_buffer);
				} else {
					effect.render(camera.source_target, camera.destination_target);
				}
				swap(camera.source_target, camera.destination_target);
			}
		}
	}
	
	tg::disable_blend();

	//tg::clear(tg::back_buffer, tg::ClearFlags_depth, {}, 1);

	tg::set_rasterizer({
		.depth_test = true,
		.depth_write = true,
		.depth_func = tg::Comparison_less,
	});
	tg::set_blend(tg::BlendFunction_add, tg::Blend_source_alpha, tg::Blend_one_minus_source_alpha);

	if (selection.kind == Selection_entity) {
		auto new_transform = manipulate_transform(selection.entity->position, selection.entity->rotation, selection.entity->scale, view->manipulator_kind);
		selection.entity->position = new_transform.position;
		selection.entity->rotation = new_transform.rotation;
		selection.entity->scale    = new_transform.scale;

		for (auto &request : manipulator_draw_requests) {
			v3f camera_to_handle_direction = normalize(request.position - camera_entity.position);
			tg::update_shader_constants(entity_constants, {
				.local_to_camera_matrix = 
					camera.world_to_camera_matrix
					* m4::translation(camera_entity.position + camera_to_handle_direction)
					* (m4)request.rotation
					* m4::scale(request.size * dot(camera_to_handle_direction, camera_entity.rotation * v3f{0,0,-1})),
				.local_to_world_normal_matrix = local_to_world_normal(request.rotation, V3f(request.size * dot(camera_to_handle_direction, camera_entity.rotation * v3f{0,0,-1}))),
				.object_rotation_matrix = (m4)request.rotation,
			});
			tg::set_shader(handle_shader);
			tg::set_shader_constants(handle_constants, 0);
		
			u32 selected_element = request.highlighted_part_index;

			v3f to_camera = normalize(camera_entity.position - selection.entity->position);
			switch (request.kind) {
				case Manipulate_position: {
					tg::update_shader_constants(handle_constants, {.color = V3f(1), .selected = (f32)(selected_element != null_manipulator_part), .to_camera = to_camera});
					draw_mesh(handle_sphere_mesh);
		
					tg::update_shader_constants(handle_constants, {.color = V3f(1,0,0), .selected = (f32)(selected_element == 0), .to_camera = to_camera});
					draw_mesh(handle_axis_x_mesh);

					tg::update_shader_constants(handle_constants, {.color = V3f(0,1,0), .selected = (f32)(selected_element == 1), .to_camera = to_camera});
					draw_mesh(handle_axis_y_mesh);

					tg::update_shader_constants(handle_constants, {.color = V3f(0,0,1), .selected = (f32)(selected_element == 2), .to_camera = to_camera});
					draw_mesh(handle_axis_z_mesh);
				
					tg::update_shader_constants(handle_constants, {.color = V3f(1,0,0), .selected = (f32)(selected_element == 0), .to_camera = to_camera});
					draw_mesh(handle_arrow_x_mesh);

					tg::update_shader_constants(handle_constants, {.color = V3f(0,1,0), .selected = (f32)(selected_element == 1), .to_camera = to_camera});
					draw_mesh(handle_arrow_y_mesh);

					tg::update_shader_constants(handle_constants, {.color = V3f(0,0,1), .selected = (f32)(selected_element == 2), .to_camera = to_camera});
					draw_mesh(handle_arrow_z_mesh);

					tg::update_shader_constants(handle_constants, {.color = V3f(1,0,0), .selected = (f32)(selected_element == 3), .to_camera = to_camera});
					draw_mesh(handle_plane_x_mesh);

					tg::update_shader_constants(handle_constants, {.color = V3f(0,1,0), .selected = (f32)(selected_element == 4), .to_camera = to_camera});
					draw_mesh(handle_plane_y_mesh);

					tg::update_shader_constants(handle_constants, {.color = V3f(0,0,1), .selected = (f32)(selected_element == 5), .to_camera = to_camera});
					draw_mesh(handle_plane_z_mesh);
					break;
				}
				case Manipulate_rotation: {
					tg::update_shader_constants(handle_constants, {.matrix = m4::rotation_r_zxy(0,0,pi/2), .color = V3f(1,0,0), .selected = (f32)(selected_element == 0), .to_camera = to_camera, .is_rotation = 1});
					draw_mesh(handle_circle_mesh);

					tg::update_shader_constants(handle_constants, {.color = V3f(0,1,0), .selected = (f32)(selected_element == 1), .to_camera = to_camera, .is_rotation = 1});
					draw_mesh(handle_circle_mesh);

					tg::update_shader_constants(handle_constants, {.matrix = m4::rotation_r_zxy(pi/2,0,0), .color = V3f(0,0,1), .selected = (f32)(selected_element == 2), .to_camera = to_camera, .is_rotation = 1});
					draw_mesh(handle_circle_mesh);
				
					if (request.dragging) {
						quaternion rotation = quaternion_look(request.tangent.direction);
						v3f position = request.tangent.origin;
						tg::update_shader_constants(entity_constants, {
							.local_to_camera_matrix = 
								camera.world_to_camera_matrix
								* m4::translation(camera_entity.position + camera_to_handle_direction)
								* m4::scale(request.size * dot(camera_to_handle_direction, camera_entity.rotation * v3f{0,0,-1})) * m4::translation(position) * (m4)rotation,
							.local_to_world_normal_matrix = local_to_world_normal(rotation, V3f(request.size * dot(camera_to_handle_direction, camera_entity.rotation * v3f{0,0,-1}))),
							.object_rotation_matrix = (m4)rotation,
						});
						tg::update_shader_constants(handle_constants, {.color = V3f(1,1,1), .selected = 1, .to_camera = to_camera});
						draw_mesh(handle_tangent_mesh);
					}

					break;
				}
				case Manipulate_scale: {
					tg::update_shader_constants(handle_constants, {.color = V3f(1), .selected = (f32)(selected_element != null_manipulator_part), .to_camera = to_camera});
					draw_mesh(handle_sphere_mesh);
		
					tg::update_shader_constants(handle_constants, {.matrix = m4::scale(request.scale.x, 1, 1), .color = V3f(1,0,0), .selected = (f32)(selected_element == 0), .to_camera = to_camera});
					draw_mesh(handle_axis_x_mesh);

					tg::update_shader_constants(handle_constants, {.matrix = m4::scale(1, request.scale.y, 1), .color = V3f(0,1,0), .selected = (f32)(selected_element == 1), .to_camera = to_camera});
					draw_mesh(handle_axis_y_mesh);

					tg::update_shader_constants(handle_constants, {.matrix = m4::scale(1, 1, request.scale.z), .color = V3f(0,0,1), .selected = (f32)(selected_element == 2), .to_camera = to_camera});
					draw_mesh(handle_axis_z_mesh);
				
					tg::update_shader_constants(handle_constants, {.matrix = m4::translation(0.8f*request.scale.x,0,0) * m4::scale(1.5f), .color = V3f(1,0,0), .selected = (f32)(selected_element == 0), .to_camera = to_camera});
					draw_mesh(handle_sphere_mesh);

					tg::update_shader_constants(handle_constants, {.matrix = m4::translation(0,0.8f*request.scale.y,0) * m4::scale(1.5f), .color = V3f(0,1,0), .selected = (f32)(selected_element == 1), .to_camera = to_camera});
					draw_mesh(handle_sphere_mesh);

					tg::update_shader_constants(handle_constants, {.matrix = m4::translation(0,0,0.8f*request.scale.z) * m4::scale(1.5f), .color = V3f(0,0,1), .selected = (f32)(selected_element == 2), .to_camera = to_camera});
					draw_mesh(handle_sphere_mesh);

					tg::update_shader_constants(handle_constants, {.color = V3f(1,0,0), .selected = (f32)(selected_element == 3), .to_camera = to_camera});
					draw_mesh(handle_plane_x_mesh);

					tg::update_shader_constants(handle_constants, {.color = V3f(0,1,0), .selected = (f32)(selected_element == 4), .to_camera = to_camera});
					draw_mesh(handle_plane_y_mesh);

					tg::update_shader_constants(handle_constants, {.color = V3f(0,0,1), .selected = (f32)(selected_element == 5), .to_camera = to_camera});
					draw_mesh(handle_plane_z_mesh);
					break;
				}
			}
		}
		manipulator_draw_requests.clear();
	}
	
	debug_draw_lines();
}

void run() {
	construct(entities);

	manipulator_draw_requests = {};
	manipulator_states = {};
	debug_lines = {};

	button_states = {};
	float_field_states = {};
	text_field_states  = {};

	input_string = {};
	
	drag_and_drop_data = {};
	tab_moves = {};
	editor_windows = {};

	construct(assets);
	
	init_component_storages<
#define c(name) name
#define sep ,
		ENUMERATE_COMPONENTS
#undef sep
#undef c
	>();

	CreateWindowInfo info;
	info.on_create = [](Window &window) {
		auto graphics_api = tg::GraphicsApi_opengl;
		assert_always(tg::init(graphics_api, {
			.window = window.handle,
			.debug = BUILD_DEBUG,
		}));
		
		tg::set_scissor(aabb_min_max({}, (v2s)window.client_size));
		tg::set_cull(tg::Cull_back);

		init_font();

		debug_lines_vertex_buffer = tg::create_vertex_buffer(
			{},
			{
				tg::Element_f32x3, // position
				tg::Element_f32x3, // color
			}
		);
		//tg::set_vsync(false);

		global_constants = tg::create_shader_constants<GlobalConstants>();
		tg::set_shader_constants(global_constants, GLOBAL_CONSTANTS_SLOT);

		entity_constants = tg::create_shader_constants<EntityConstants>();
		tg::set_shader_constants(entity_constants, ENTITY_CONSTANTS_SLOT);

		light_constants = tg::create_shader_constants<LightConstants>();
		tg::set_shader_constants(light_constants, LIGHT_CONSTANTS_SLOT);

		average_shader_buffer = tg::create_compute_buffer(sizeof(u32));
		tg::set_compute_buffer(average_shader_buffer, AVERAGE_COMPUTE_SLOT);

		switch (graphics_api) {
			case tg::GraphicsApi_opengl: {
				surface_material.constants = tg::create_shader_constants(sizeof(SurfaceConstants));
				tg::update_shader_constants(surface_material.constants, SurfaceConstants{.color = {1,1,1,1}});
				surface_material.shader = create_shader(u8R"(
layout (std140, binding=0) uniform _ {
	vec4 u_color;
};

V2F vec3 vertex_normal;
V2F vec4 vertex_color;
V2F vec3 vertex_world_position;
V2F vec3 vertex_view_direction;
V2F vec3 vertex_to_light_direction;
V2F vec4 vertex_position_in_light_space;
V2F vec2 vertex_uv;

#ifdef VERTEX_SHADER

layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
layout(location=2) in vec4 color;
layout(location=3) in vec2 uv;

void main() {
	vec3 local_position = position;
	vertex_normal = (local_to_world_normal_matrix * vec4(normal, 0)).xyz;
	vertex_color = color * u_color;
	vertex_world_position = (local_to_world_position_matrix * vec4(local_position, 1)).xyz;
	vertex_position_in_light_space = world_to_light_matrix * vec4(vertex_world_position, 1);
	vertex_view_direction = camera_position - vertex_world_position;
	vertex_to_light_direction = light_position - vertex_world_position;
	vertex_uv = uv;
	gl_Position = local_to_camera_matrix * vec4(local_position, 1);
}
#endif
#ifdef FRAGMENT_SHADER
out vec4 fragment_color;
void main() {
	fragment_color = vec4(pbr(vertex_color.xyz, normalize(vertex_normal), normalize(vertex_to_light_direction), normalize(vertex_view_direction)), 1);

	vec3 light_space = (vertex_position_in_light_space.xyz / vertex_position_in_light_space.w) * 0.5 + 0.5;

	float light = sample_shadow_map(shadow_map, light_space, 0.001f);
	light *= light_intensity / pow2(length(vertex_to_light_direction) + 1);
	fragment_color *= light * texture(light_texture, light_space.xy);

	if (light_index == 0) {
		fragment_color += texture(lightmap_texture, vertex_uv) / pi;
	}

	//fragment_color = texture(lightmap_texture, vertex_uv);
}
#endif
)"s);
				handle_constants = tg::create_shader_constants<HandleConstants>();
				handle_shader = create_shader(u8R"(
layout (std140, binding=0) uniform _ {
	mat4 object_matrix;
	vec3 u_color;
	float selected;
	vec3 to_camera;
	float is_rotation;
};

V2F vec3 vertex_color;
V2F vec3 vertex_normal;
V2F vec3 vertex_local_position;

#ifdef VERTEX_SHADER

layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;

void main() {
	vec3 local_position = position;
	vertex_color = u_color;
	vertex_normal = (local_to_world_normal_matrix * (object_matrix * vec4(normal, 0))).xyz;
	vertex_local_position =  (object_rotation_matrix * (object_matrix * vec4(local_position, 1))).xyz;
	gl_Position = local_to_camera_matrix * (object_matrix * vec4(local_position, 1));
}
#endif
#ifdef FRAGMENT_SHADER
out vec4 fragment_color;
void main() {
	vec3 normal = normalize(vertex_normal);
	vec3 default_color = vertex_color * 0.5f;
	vec3 highlighted_color = mix(vertex_color, vec3(1), .2);

	vec4 mixed_color = vec4(
		mix(default_color, highlighted_color, selected),
		abs(dot(normal, to_camera)) * mix(1, saturate((dot(vertex_local_position, to_camera) + 0.1f) * 16), is_rotation)
	);

	fragment_color = mixed_color;
	//fragment_color = vec4(-dot(normal, camera_forward));
	//fragment_color = vec4(normal, 1);
}
#endif
)"s);
				blit_texture_shader = tg::create_shader(u8R"(
#ifdef VERTEX_SHADER
#define V2F out
#else
#define V2F in
#endif

layout(binding=0) uniform sampler2D main_texture;

V2F vec2 vertex_uv;

#ifdef VERTEX_SHADER

void main() {
	vec2 positions[] = vec2[](
		vec2(-1, 3),
		vec2(-1,-1),
		vec2( 3,-1)
	);
	vec2 position = positions[gl_VertexID];
	vertex_uv = position * 0.5 + 0.5;
	gl_Position = vec4(position, 0, 1);
}
#endif
#ifdef FRAGMENT_SHADER
out vec4 fragment_color;
void main() {
	fragment_color = texture(main_texture, vertex_uv);
}
#endif
)"s);
				blit_color_constants = tg::create_shader_constants<BlitColorConstants>();
				blit_color_shader = tg::create_shader(u8R"(
#ifdef VERTEX_SHADER
#define V2F out
#else
#define V2F in
#endif

layout (std140, binding=0) uniform _ {
	vec4 u_color;
};

#ifdef VERTEX_SHADER

void main() {
	vec2 positions[] = vec2[](
		vec2(-1, 3),
		vec2(-1,-1),
		vec2( 3,-1)
	);
	gl_Position = vec4(positions[gl_VertexID], 0, 1);
}
#endif
#ifdef FRAGMENT_SHADER
out vec4 fragment_color;
void main() {
	fragment_color = u_color;
}
#endif
)"s);
				blit_texture_color_constants = tg::create_shader_constants<BlitTextureColorConstants>();
				blit_texture_color_shader = tg::create_shader(u8R"(
#ifdef VERTEX_SHADER
#define V2F out
#else
#define V2F in
#endif

layout (std140, binding=0) uniform _ {
	vec4 u_color;
};

layout(binding=0) uniform sampler2D main_texture;

V2F vec2 vertex_uv;

#ifdef VERTEX_SHADER

void main() {
	vec2 positions[] = vec2[](
		vec2(-1, 3),
		vec2(-1,-1),
		vec2( 3,-1)
	);
	vec2 position = positions[gl_VertexID];
	vertex_uv = position * 0.5 + 0.5;
	gl_Position = vec4(position, 0, 1);
}
#endif
#ifdef FRAGMENT_SHADER
out vec4 fragment_color;
void main() {
	fragment_color = texture(main_texture, vertex_uv) * u_color;
}
#endif
)"s);
				shadow_map_shader = create_shader(u8R"(
#ifdef VERTEX_SHADER

layout(location=0) in vec3 position;

void main() {
	vec3 local_position = position;
	gl_Position = local_to_camera_matrix * vec4(local_position, 1);
}
#endif
#ifdef FRAGMENT_SHADER
void main() {
}
#endif
)"s);
				sky_box_shader = create_shader(u8R"(
layout(binding=0) uniform samplerCube sky_box;

V2F vec3 vertex_uv;

#ifdef VERTEX_SHADER

void main() {
	vec3 positions[] = vec3[](
		vec3( 1, 1, 1),
		vec3( 1, 1,-1),
		vec3( 1,-1, 1),
		vec3( 1,-1,-1),
		vec3(-1, 1, 1),
		vec3(-1, 1,-1),
		vec3(-1,-1, 1),
		vec3(-1,-1,-1)
	);
	uint indices[] = uint[](
		5, 4, 7, 7, 4, 6,
		1, 5, 3, 3, 5, 7,
		0, 1, 2, 2, 1, 3,
		4, 0, 6, 6, 0, 2,
		1, 0, 5, 5, 0, 4,
		7, 6, 3, 3, 6, 2
	);
	vec3 local_position = positions[indices[gl_VertexID]];
	vertex_uv = local_position * vec3(1,1,-1);
	gl_Position = camera_rotation_projection_matrix * vec4(local_position, 1);
}
#endif
#ifdef FRAGMENT_SHADER
out vec4 fragment_color;
void main() {
	fragment_color = texture(sky_box, vertex_uv);
	//fragment_color = pow(fragment_color, vec4(1/2.2));
	gl_FragDepth = 1;
}
#endif
)"s);
				average_shader = tg::create_compute_shader(u8R"(
layout(std430, binding = )" STRINGIZE(AVERAGE_COMPUTE_SLOT) R"() writeonly buffer sum_buffer {
	uint dest_sum;
};
layout(binding=0, rgba16f) uniform image2D source_texture;
layout (local_size_x = 16, local_size_y = 16) in;
void main() {

	//atomicAdd(dest_sum, 1);
	dest_sum = 1;
	//ivec2 uv = ivec2(gl_GlobalInvocationID.xy);
	//vec4 texel = imageLoad(source_texture, uv);
	//float average = (texel.x + texel.y + texel.z) * 0.3333f;
	//
	//atomicAdd(dest_sum, uint(average * 256));
}
)"s);
				debug_line_shader = create_shader(u8R"(
V2F vec3 vertex_color;

#ifdef VERTEX_SHADER

layout(location=0) in vec3 position;
layout(location=1) in vec3 color;

void main() {
	vec3 local_position = position;
	vertex_color = color;
	gl_Position = world_to_camera_matrix * vec4(local_position, 1);
}
#endif
#ifdef FRAGMENT_SHADER
out vec4 fragment_color;
void main() {
	fragment_color = vec4(vertex_color, 1);
}
#endif
)"s);
				break;
			}
			/*
			case tg::GraphicsApi_d3d11: {
				surface_shader = tg::create_shader<SurfaceShaderValues>(concatenate(shader_header, u8R"(
cbuffer _ : register(b0) {
	float4x4 u_matrix;
	float4 albedo;
	float3 camera_position;
}

struct VSInput {
	float3 position : A;
	float3 normal : B;
	float4 color : C;
};

struct V2P {
	float4 position : SV_Position;
	float3 normal : NORMAL;
	float3 world_position : WORLD_POSITION;
};

void vertex_main(in VSInput input, out V2P output) {
	float3 local_position = input.position;
	output.world_position = local_position;
	output.normal = input.normal;
	output.position = mul(u_matrix, float4(local_position * float3(1,1,-1), 1));
}

void pixel_main(in V2P input, out float4 color : SV_Target) {
	float3 N = normalize(input.normal);
	float3 L = normalize(float3(1,3,2));
	float3 V = -normalize(input.world_position - camera_position);
	//V.z = -V.z;
	float3 H = normalize(L + V);
	float NV = dot(N, V);
	float NL = saturate(dot(N, L));
	float NH = dot(N, H);
	float VH = dot(V, H);

	float diffuse = saturate(dot(N, L));

	float roughness = 0.1f;
	float m2 = roughness*roughness;
	float D =
		exp(-pow2(sqrt(1 - NH*NH)/NH)/m2)
		/
		(pi*m2*pow4(NH));

	float r0 = 0.5f;
	float F = r0 + (1 - r0) * pow5(1 - NV);

	float G = min(1, min(2*NH*NV/VH,2*NH*NL/VH));

	float kspec = D * F * G / (pi * NV * NL);
	float kdiff = 1 - kspec;

	color = albedo * NL * kdiff + kspec;
	color = float4(camera_position, 1);
}
)"s));
				break;
			}
			*/
		}

		u32 white_pixel = ~0;
		white_texture = tg::create_texture_2d(1, 1, &white_pixel, tg::Format_rgba_u8n, tg::Filtering_nearest);
		white_texture->name = as_list(u8"white"s);

		u32 black_pixel = 0xFF000000;
		black_texture = tg::create_texture_2d(1, 1, &black_pixel, tg::Format_rgba_u8n, tg::Filtering_nearest);
		black_texture->name = as_list(u8"black"s);
		
		handle_sphere_mesh  = assets.meshes.get(u8"../data/handle.glb:Sphere"s);
		handle_circle_mesh  = assets.meshes.get(u8"../data/handle.glb:Circle"s);
		handle_tangent_mesh = assets.meshes.get(u8"../data/handle.glb:Tangent"s);
		handle_axis_x_mesh  = assets.meshes.get(u8"../data/handle.glb:AxisX"s );
		handle_axis_y_mesh  = assets.meshes.get(u8"../data/handle.glb:AxisY"s );
		handle_axis_z_mesh  = assets.meshes.get(u8"../data/handle.glb:AxisZ"s );
		handle_arrow_x_mesh = assets.meshes.get(u8"../data/handle.glb:ArrowX"s );
		handle_arrow_y_mesh = assets.meshes.get(u8"../data/handle.glb:ArrowY"s );
		handle_arrow_z_mesh = assets.meshes.get(u8"../data/handle.glb:ArrowZ"s );
		handle_plane_x_mesh = assets.meshes.get(u8"../data/handle.glb:PlaneX"s);
		handle_plane_y_mesh = assets.meshes.get(u8"../data/handle.glb:PlaneY"s);
		handle_plane_z_mesh = assets.meshes.get(u8"../data/handle.glb:PlaneZ"s);
		
		sky_box_texture = tg::load_texture_cube({
			.left   = tl_file_string("../data/sky_x+.hdr"s),
			.right  = tl_file_string("../data/sky_x-.hdr"s),
			.top    = tl_file_string("../data/sky_y+.hdr"s),
			.bottom = tl_file_string("../data/sky_y-.hdr"s),
			.front  = tl_file_string("../data/sky_z-.hdr"s),
			.back   = tl_file_string("../data/sky_z+.hdr"s),
		});

		auto create_default_scene = [&]() {
			auto &suzanne = create_entity("suzan\"ne");
			suzanne.rotation = quaternion_from_euler(radians(v3f{-54.7, 45, 0}));
			{
				auto &mr = add_component<MeshRenderer>(suzanne);
				mr.mesh = assets.meshes.get(u8"../data/scene.glb:Suzanne"s);
				mr.material = &surface_material;
				mr.lightmap = assets.textures_2d.get(u8"../data/suzanne_lightmap.png"s);

				auto &rotator = add_component<Rotator>(suzanne);
				rotator.axis = {1, 1, 1};
				rotator.degrees_per_second = 30;
			}
			selection.set(&suzanne);

			auto &floor = create_entity("floor");
			{
				auto &mr = add_component<MeshRenderer>(floor);
				mr.mesh = assets.meshes.get(u8"../data/scene.glb:Room"s);
				mr.material = &surface_material;
				mr.lightmap = assets.textures_2d.get(u8"../data/floor_lightmap.png"s);
			}
			
			auto light_texture = assets.textures_2d.get(u8"../data/spotlight_mask.png"s);

			{
				auto &light = create_entity("light1");
				light.position = {0,2,6};
				//light.rotation = quaternion_from_euler(-pi/10,0,pi/6);
				light.rotation = quaternion_from_euler(0,0,0);
				add_component<Light>(light).mask = light_texture;
			}

			{
				auto &light = create_entity("light2");
				light.position = {6,2,-6};
				light.rotation = quaternion_from_euler(-pi/10,pi*0.75,0);
				add_component<Light>(light).mask = light_texture;
			}
			
			auto &camera_entity = create_entity("main camera");

			auto &camera = add_component<Camera>(camera_entity);

			auto &exposure = camera.add_post_effect<Exposure>();
			exposure.auto_adjustment = false;
			exposure.exposure = 1.5;
			exposure.limit_min = 1.0f / 16;
			exposure.limit_max = 1024;
			exposure.approach_kind = Exposure::Approach_log_lerp;
			exposure.mask_kind = Exposure::Mask_one;
			exposure.mask_radius = 1;

			auto &bloom = camera.add_post_effect<Bloom>();

			auto &dither = camera.add_post_effect<Dither>();
		};

		//if (!deserialize_window_layout())
			main_window = create_split_view(
				create_split_view(
					create_tab_view(create_file_view()),
					create_tab_view(create_scene_view()),
					{ .split_t = 0 }
				),
				create_split_view(
					create_tab_view(create_hierarchy_view()),
					create_tab_view(create_property_view()),
					{ .horizontal = true }
				),
				{ .split_t = 1 }
			);

		if (!deserialize_scene(u8"test.scene"s))
			create_default_scene();
	};
	info.on_draw = [](Window &window) {
		current_cursor = Cursor_default;

		static v2u old_window_size;
		if (any_true(old_window_size != window.client_size)) {
			old_window_size = window.client_size;

			main_window->resize({.min = {}, .max = (v2s)window.client_size});

			tg::resize_render_targets(window.client_size);
		}
		
		current_viewport = current_scissor = {
			.min = {},
			.max = (v2s)window.client_size,
		};
		tg::set_viewport(current_viewport);
		tg::set_scissor(current_scissor);

		current_mouse_position = {window.mouse_position.x, (s32)window.client_size.y - window.mouse_position.y};
		
		if (key_down(Key_f1, {.anywhere = true})) {
			Profiler::enabled = true;
			Profiler::reset();
		}
		defer {
			if (Profiler::enabled) {
				Profiler::enabled = false;
				write_entire_file(tl_file_string("update.tmd"s), Profiler::output_for_timed());
			}
		};

		if (key_down(Key_f2, {.anywhere = true})) {
			for_each(entities, [](Entity &e) {
				print("name: %, index: %, flags: %, position: %, rotation: %\n", e.name, index_of(entities, &e).value, e.flags, e.position, degrees(to_euler_angles(e.rotation)));
				for (auto &c : e.components) {
					print("\tparent: %, type: % (%), index: %\n", c.entity_index, c.type, component_info[c.type].name, c.index);
				}
			});
		}

		if (key_down(Key_f3, {.anywhere = true})) {
		}

		window.min_window_size = client_size_to_window_size(window, main_window->get_min_size());

		input_user_index = 0;
		focusable_input_user_index = 0;

		timed_block("frame"s);

#define c(name) \
	if constexpr (is_statically_overridden(update, name, Component)) { \
		for_each_component_of_type(name, comp) { \
			comp.update(); \
		}; \
	}
#define sep
		ENUMERATE_COMPONENTS
#undef sep
#undef c


		{
			timed_block("Shadows"s);
			tg::disable_scissor();
			tg::set_rasterizer(
				tg::get_rasterizer()
					.set_depth_test(true)
					.set_depth_write(true)
					.set_depth_func(tg::Comparison_less)
			);
			tg::disable_blend();
			tg::set_topology(tg::Topology_triangle_list);

			for_each_component_of_type(Light, light) {
				timed_block("Light"s);
				auto &light_entity = entities[light.entity_index];

				tg::set_render_target(light.shadow_map);
				tg::set_viewport(shadow_map_resolution, shadow_map_resolution);
				tg::clear(light.shadow_map, tg::ClearFlags_depth, {}, 1);

				tg::set_shader(shadow_map_shader);

				light.world_to_light_matrix = m4::perspective_right_handed(1, light.fov, 0.1f, 100.0f) * (m4)-light_entity.rotation * m4::translation(-light_entity.position);

				for_each_component_of_type(MeshRenderer, mesh_renderer) {
					auto &mesh_entity = entities[mesh_renderer.entity_index];

					tg::update_shader_constants(entity_constants, {
						.local_to_camera_matrix = light.world_to_light_matrix * m4::translation(mesh_entity.position) * (m4)mesh_entity.rotation * m4::scale(mesh_entity.scale),
					});
					draw_mesh(mesh_renderer.mesh);
				};
			};
		}

		tg::clear(tg::back_buffer, tg::ClearFlags_color | tg::ClearFlags_depth, foreground_color, 1);
		
		{
			timed_block("main_window->render()"s);
			main_window->render();
		}
		
		switch (drag_and_drop_kind) {
			case DragAndDrop_file: {
				auto texture = assets.textures_2d.get(as_utf8(drag_and_drop_data));
				if (texture) {
					aabb<v2s> thumbnail_viewport;
					thumbnail_viewport.min = thumbnail_viewport.max = current_mouse_position;
					thumbnail_viewport.max.x += 128;
					thumbnail_viewport.min.y -= 128;
					push_current_viewport(thumbnail_viewport) {
						blit(texture);
					}
				}
				break;
			}
			case DragAndDrop_tab: {
				auto tab_info = *(DragDropTabInfo *)drag_and_drop_data.data;
				auto tab = tab_info.tab_view->tabs[tab_info.tab_index];

				auto font = get_font_at_size(font_collection, font_size);
				ensure_all_chars_present(tab.window->name, font);
				auto placed_chars = with(temporary_allocator, place_text(tab.window->name, font));

				tg::Viewport tab_viewport;
				tab_viewport.min = tab_viewport.max = current_mouse_position;

				tab_viewport.min.y -= TabView::tab_height;
				tab_viewport.max.x = tab_viewport.min.x + placed_chars.back().position.max.x + 4;

				push_current_viewport(tab_viewport) {
					blit({.1,.1,.1,1});
					draw_text(placed_chars, font, {.position = {2, 0}});
				}
				break;
			}
		}

		if (drag_and_dropping()) {
			if (key_state[256].state & KeyState_up) {
				drag_and_drop_kind = DragAndDrop_none;
				unlock_input_nocheck();
			}
		}

		debug_frame();
		
		bool debug_print_editor_window_hierarchy = frame_index == 0;
		for (auto &move : tab_moves) {
			auto from = move.from;
			auto tab_index = move.tab_index;
			auto to = move.to;
			auto direction = move.direction;
			
			auto tab = from->tabs[tab_index];

			auto remove_tab_view = [&]() {
				auto split_view = (SplitView *)from->parent;
				assert(split_view);
				assert(split_view->kind == EditorWindow_split_view);

				EditorWindow *what_is_left = split_view->get_other_part(from);

				what_is_left->parent = split_view->parent;

				if (split_view->parent) {
					switch (split_view->parent->kind) {
						case EditorWindow_split_view: {
							auto parent_view = (SplitView *)split_view->parent;
							parent_view->get_part(split_view) = what_is_left;
							parent_view->resize(parent_view->viewport);
							break;
						}
						default: {
							invalid_code_path();
							break;
						}
					}
				} else {
					auto main_window_viewport = main_window->viewport;
					main_window = what_is_left;
					main_window->resize(main_window_viewport);
				}
			};

			if (direction == (u32)-1) {
				if (!tab.window->parent->parent)
					return;

				to->tabs.add(tab);
				from->tabs.erase_at(tab_index);
				from->selected_tab = min(from->selected_tab, from->tabs.size - 1);

				if (from->tabs.size == 0) {
					remove_tab_view();
				}
			} else {
				bool horizontal = (direction & 1);
				bool swap_parts = direction >= 2;

				TabView *new_tab_view;
				if (from->tabs.size == 1) {
					new_tab_view = from;

					auto from_parent = (SplitView *)from->parent;
					assert(from_parent);
					assert(from_parent->kind == EditorWindow_split_view);
					auto what_is_left = from_parent->get_other_part(from);
					auto from_parent_parent = (SplitView *)from_parent->parent;
					assert(from_parent_parent);
					assert(from_parent_parent->kind == EditorWindow_split_view);
					from_parent_parent->replace_child(from_parent, what_is_left);
				} else {
					from->tabs.erase_at(tab_index);
					from->selected_tab = min(from->selected_tab, from->tabs.size - 1);
					if (from->tabs.size == 0) {
						remove_tab_view();
					}
					new_tab_view = create_tab_view(tab.window);
				}

				auto left = to;
				auto right = new_tab_view;
				if (swap_parts) {
					swap(left, right);
				}

				if (to->parent) {
					auto to_parent = (SplitView *)to->parent;
					assert(to_parent->kind == EditorWindow_split_view);

					to_parent->replace_child(to, create_split_view(left, right, {.split_t = 0.5f, .horizontal = horizontal}));
				} else {
					assert(to == main_window);
					main_window = create_split_view(left, right, {.split_t = 0.5f, .horizontal = horizontal});
				}

				tg::Viewport window_viewport = {};
				window_viewport.max = (v2s)max(main_window->get_min_size(), window.client_size);
				main_window->resize(window_viewport);
				resize(window, (v2u)window_viewport.max);

				//to_parent->resize(to_parent->viewport);
			}

			//split_view->free();

			debug_print_editor_window_hierarchy = true;
		}
		tab_moves.clear();


		if (should_unlock_input) {
			should_unlock_input = false;
			input_is_locked = false;
			input_locker = 0;
		}
		
		if (debug_print_editor_window_hierarchy || (key_state[Key_f4].state & KeyState_down)) {
			debug_print_editor_window_hierarchy = false;
			debug_print_editor_window_hierarchy_tab = 0;

			main_window->debug_print();
		}

		for (auto &state : key_state) {
			if (state.state & KeyState_down) {
				state.state &= ~KeyState_down;
			} else if (state.state & KeyState_up) {
				state.state = KeyState_none;
			}
			if (state.state & KeyState_repeated) {
				state.state &= ~KeyState_repeated;
			}
			state.state &= ~KeyState_begin_drag;
			if ((state.state & KeyState_held) && !(state.state & KeyState_drag) && (distance_squared(state.start_position, current_mouse_position) >= pow2(8))) {
				state.state |= KeyState_drag | KeyState_begin_drag;
			}
		}
		
		input_string.clear();
		
		clear_temporary_storage();

		{
			timed_block("present"s);
			tg::present();
		}

		frame_time = min(max_frame_time, reset(frame_timer));
		time += frame_time;
		++fps_counter;
		frame_index += 1;

		set_title(&window, tformat(u8"frame_time: % ms, fps: %", frame_time * 1000, fps_counter_result));

		set_cursor(window, current_cursor);

		fps_timer += frame_time;
		if (fps_timer >= 1) {
			fps_counter_result = fps_counter;
			fps_timer -= 1;
			fps_counter = 0;
		}
	};
	//info.get_cursor = [](Window &window) -> Cursor {
	//	print("get cursor %\n", get_time_string());
	//	return current_cursor;
	//};

	window = create_window(info);
	defer { free(window); };

	assert_always(window);
	
	on_key_down = [](u8 key) {
		key_state[key].state = KeyState_down | KeyState_repeated | KeyState_held;
		key_state[key].start_position = input_is_locked ? input_lock_mouse_position : current_mouse_position;
	};
	on_key_up = [](u8 key) {
		key_state[key].state = KeyState_up;
	};
	on_key_repeat = [](u8 key) {
		key_state[key].state |= KeyState_repeated;
	};
	on_mouse_down = [](u8 button){
		key_state[256 + button].state = KeyState_down | KeyState_held;
		key_state[256 + button].start_position = input_is_locked ? input_lock_mouse_position : current_mouse_position;
	};
	on_mouse_up = [](u8 button){
		auto &state = key_state[256 + button];
		state.state = KeyState_up | ((state.state & KeyState_drag) ? KeyState_end_drag : 0);
	};
	on_char = [](u32 ch) {
		input_string.add(encode_utf8(ch));
	};

	frame_timer = create_precise_timer();

	Profiler::enabled = false;
	Profiler::reset();

	while (update(window)) {
	}

	write_entire_file(tl_file_string("test.scene"s), as_bytes(with(temporary_allocator, serialize_scene())));
	
	serialize_window_layout();

	for_each(entities, [](Entity &e) {
		destroy(e);
	});

	free_component_storages();
}

s32 tl_main(Span<Span<utf8>> arguments) {
	Profiler::init();
	defer { Profiler::deinit(); };

	is_editor = true;
#define TRACK_ALLOCATIONS 0
#if TRACK_ALLOCATIONS
	debug_init();
	defer { debug_deinit(); };
	current_allocator = tracking_allocator;
#endif

	auto log_file = open_file(tl_file_string("log.txt"s), File_write);
	defer { close(log_file); };
	auto log_printer = Printer {
		[](PrintKind kind, Span<utf8> string, void *data) {
			console_printer(kind, string);
			write({data}, as_bytes(string));
		},
		log_file.handle
	};

	current_printer = log_printer;

	auto cpu_info = get_cpu_info();
	print(R"(CPU:
 - Brand: %
 - Vendor: %
 - Thread count: %
 - Cache line size: %
)", as_span(cpu_info.brand), to_string(cpu_info.vendor), cpu_info.logical_processor_count, cpu_info.cache_line_size);
	
	print("Cache:\n");

	for (u32 level = 0; level != CpuCacheLevel_count; ++level) {
		for (u32 type_index = 0; type_index != CpuCacheType_count; ++type_index) {
			auto &cache = cpu_info.caches_by_level_and_type[level][type_index];
			if (cache.count == 0 || cache.size == 0)
				continue;
			print("L% %: % x %\n", level + 1, to_string((CpuCacheType)type_index), cache.count, format_bytes(cache.size));
		}
	}

#define f(x) print(" - " #x ": %\n", cpu_info.has_feature(CpuFeature_##x));
	tl_all_cpu_features(f)
#undef f

	print("RAM: %\n", format_bytes(get_ram_size()));

	run();

#if TRACK_ALLOCATIONS
	current_allocator = temporary_allocator;

	print("Unfreed allocations:\n");
	for (auto &[pointer, info] : get_tracked_allocations()) {
		print("size: %, location: %, call stack:\n", info.size, info.location);
		auto call_stack = to_string(info.call_stack);
		for (auto &call : call_stack.call_stack) {
			print("\t%(%):%\n", call.file, call.line, call.name);
		}
	}
#endif
	return 0;
}
