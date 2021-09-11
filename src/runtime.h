#pragma once
#include "common.h"
#include "component_list.h"
#include "component.h"

#include "components/camera.h"
#include "components/light.h"
#include "components/mesh_renderer.h"
#include "components/rotator.h"

#include "debug.h"
#include "serialize.h"
#include "blit.h"
#include "skybox.h"

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

Material surface_material;
struct SurfaceConstants {
	v4f color;
};

tg::Shader *shadow_map_shader;

// Do we need this in runtime?
struct HandleConstants {
	m4 matrix = m4::identity();

	v3f color;
	f32 selected;
	
	v3f to_camera;
	f32 is_rotation;
};
tg::TypedShaderConstants<HandleConstants> handle_constants;
tg::Shader *handle_shader;

#define shader_value_location(struct, member) tg::ShaderValueLocation{offsetof(struct, member), sizeof(struct::member)}

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

tg::GraphicsApi graphics_api;

//
// Called once on program start
//
void runtime_init(Window &window) {
	construct(entities);
	construct(assets);
	
	init_component_storages<
#define c(name) name
#define sep ,
		ENUMERATE_COMPONENTS
#undef sep
#undef c
	>();

	graphics_api = tg::GraphicsApi_opengl;
	assert_always(tg::init(graphics_api, {
		.window = window.handle,
		.debug = BUILD_DEBUG,
	}));

	global_constants = tg::create_shader_constants<GlobalConstants>();
	tg::set_shader_constants(global_constants, GLOBAL_CONSTANTS_SLOT);

	entity_constants = tg::create_shader_constants<EntityConstants>();
	tg::set_shader_constants(entity_constants, ENTITY_CONSTANTS_SLOT);

	light_constants = tg::create_shader_constants<LightConstants>();
	tg::set_shader_constants(light_constants, LIGHT_CONSTANTS_SLOT);

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

			debug_line_shader = create_shader(u8R"(
V2F vec3 vertex_color;

#ifdef VERTEX_SHADER

layout(location=0) in vec3 position;
layout(location=1) in vec3 color;

void main() {
	vec3 local_position = position;
	vertex_color = color;
	gl_Position = world_to_camera_matrix * vec4(local_position, 1);
	gl_Position.z = 0;
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
		
	sky_box_texture = tg::load_texture_cube({
		.left   = tl_file_string("../example/sky_x+.hdr"s),
		.right  = tl_file_string("../example/sky_x-.hdr"s),
		.top    = tl_file_string("../example/sky_y+.hdr"s),
		.bottom = tl_file_string("../example/sky_y-.hdr"s),
		.front  = tl_file_string("../example/sky_z-.hdr"s),
		.back   = tl_file_string("../example/sky_z+.hdr"s),
	});
}

//
// in editor:
// Called every time play mode is entetered
//
// in build:
// Called once after runtime_init()
//
void runtime_start() {

#define c(name) \
	if constexpr (is_statically_overridden(start, name, Component)) { \
		for_each_component_of_type(name, comp) { \
			comp.start(); \
		}; \
	}
#define sep
	ENUMERATE_COMPONENTS
#undef sep
#undef c

}

void runtime_update() {
	
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

}

//
// Called once per frame
//
void runtime_render() {
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
}

//
// Render scene from `camera`'s perspective into backbuffer with current viewport
//
void render_camera(Camera &camera, Entity &camera_entity) {
	
	m4 camera_projection_matrix = m4::perspective_right_handed((f32)current_viewport.size().x / current_viewport.size().y, camera.fov, camera.near_plane, camera.far_plane);
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
		for (auto &effect : camera.post_effects) {
			effect.render(camera.source_target, camera.destination_target);
			swap(camera.source_target, camera.destination_target);
		}

		tg::set_render_target(tg::back_buffer);
		tg::set_viewport(current_viewport);
		blit(camera.source_target->color);
	}
	
}
