#include "tl.h"
#include "../dep/tl/include/tl/masked_block_list.h"
#include "../include/components_base.h"

using namespace tl;

v3f camera_position;
v3f camera_rotation;
f32 frame_time = 1 / 60.0f;
f32 time;
PreciseTimer frame_timer;


struct GlobalConstants {
	v3f camera_position;
};
t3d::ShaderConstants *global_constants;
#define GLOBAL_CONSTANTS_SLOT 15


struct EntityConstants {
    m4 local_to_camera_matrix;
};
t3d::ShaderConstants *entity_constants;
#define ENTITY_CONSTANTS_SLOT 14


struct LightConstants {
	m4 world_to_light_matrix;
	v3f light_position;
	f32 light_intensity;
};
t3d::ShaderConstants *light_constants;
#define LIGHT_CONSTANTS_SLOT 13


t3d::Shader *shadow_map_shader;
t3d::Shader *handle_shader;
t3d::Shader *blit_shader;


struct Material {
	t3d::Shader *shader;
	t3d::ShaderConstants *constants;
};

Material surface_material;
struct SurfaceConstants {
	v4f color;
};

#define shader_value_location(struct, member) t3d::ShaderValueLocation{offsetof(struct, member), sizeof(struct::member)}

struct Mesh {
	t3d::VertexBuffer *vertex_buffer;
	t3d::IndexBuffer *index_buffer;
	u32 index_count;
};

Mesh load_mesh(Span<filechar> path) {
	Mesh result;
	if (auto parse_result = parse_glb_from_file(path)) {
		auto mesh = parse_result.value;

		result.vertex_buffer = t3d::create_vertex_buffer(
			as_bytes(mesh.vertices),
			{
				t3d::Element_f32x3, // position
				t3d::Element_f32x3, // normal
				t3d::Element_f32x4, // color
			}
		);

		result.index_buffer = t3d::create_index_buffer(as_bytes(mesh.indices), sizeof(u32));

		result.index_count = mesh.indices.size;

		free(mesh);
	} else {
		print("Failed to parse mesh");
		invalid_code_path();
	}
	return result;
}

void draw_mesh(Mesh &mesh) {
	t3d::set_vertex_buffer(mesh.vertex_buffer);
	t3d::set_index_buffer(mesh.index_buffer);
	t3d::draw_indexed(mesh.index_count);
}

u32 fps_counter;
u32 fps_counter_result;
f32 fps_timer;

Mesh suzanne_mesh;
Mesh floor_mesh;
Mesh position_handle_mesh;

u32 const shadow_map_resolution = 256;

f32 camera_velocity;

struct Light : Component {
	t3d::RenderTarget *shadow_map;
	t3d::Texture *texture;
	m4 world_to_light_matrix;
};

template <>
void on_create(Light &light) {
	auto depth = t3d::create_texture(t3d::CreateTexture_default, shadow_map_resolution, shadow_map_resolution, 0, t3d::TextureFormat_depth, t3d::TextureFiltering_linear, t3d::TextureComparison_less);
	light.shadow_map = t3d::create_render_target(0, depth);
}


struct MeshRenderer : Component {
	Mesh mesh;
	Material *material;
};

union quaternion {
	struct { f32 x, y, z, w; };
	f32 s[4];
	quaternion operator-() {
		return {-x, -y, -z, w};
	}
	operator m3() const {
		return {
			1 - 2 * (y*y + z*z),     2 * (x*y - z*w),     2 * (x*z + y*w),
			    2 * (x*y + z*w), 1 - 2 * (x*x + z*z),     2 * (y*z - x*w),
			    2 * (x*z - y*w),     2 * (y*z + x*w), 1 - 2 * (x*x + y*y),
		};
	}
	operator m4() const {
		return {
			1 - 2 * (y*y + z*z),     2 * (x*y - z*w),     2 * (x*z + y*w), 0,
			    2 * (x*y + z*w), 1 - 2 * (x*x + z*z),     2 * (y*z - x*w), 0,
			    2 * (x*z - y*w),     2 * (y*z + x*w), 1 - 2 * (x*x + y*y), 0,
			                  0,                   0,                   0, 1,
		};
	}
	static quaternion identity() {
		return {0,0,0,1};
	}
};

quaternion normalize(quaternion q) {
	f32 il = 1 / length(v4f{q.x,q.y,q.z,q.w});
	return {q.x*il,q.y*il,q.z*il,q.w*il};
}

quaternion quaternion_from_euler_r(f32 ax, f32 ay, f32 az) {
    v2f z = cos_sin(ax * -0.5f);
    v2f x = cos_sin(ay * -0.5f);
    v2f y = cos_sin(az * -0.5f);

    quaternion q;
    q.x = y.x * x.x * z.y - y.y * x.y * z.x;
    q.y = y.y * x.x * z.y + y.x * x.y * z.x;
    q.z = y.y * x.x * z.x - y.x * x.y * z.y;
    q.w = y.x * x.x * z.x + y.y * x.y * z.y;
    return q;
}
quaternion quaternion_from_axis_angle(v3f axis, f32 angle) {
    f32 half_angle = angle * 0.5f;
    f32 s = tl::sin(half_angle);
	return {
		axis.x * s,
		axis.y * s,
		axis.z * s,
		tl::cos(half_angle),
	};
}
quaternion quaternion_look_at(v3f from, v3f to, v3f up) {
	v3f the_forward = {0,0,-1};
    v3f forward = normalize(to - from);

	f32 d = dot(the_forward, forward);

    if (absolute(d - (-1.0f)) < 0.000001f) {
		return normalize(quaternion{up.x, up.y, up.z, pi});
    }
    if (absolute(d - (1.0f)) < 0.000001f) {
        return quaternion::identity();
    }

    f32 rotAngle = (f32)acos(d);
	v3f rotAxis = cross(the_forward, forward);
    rotAxis = normalize(rotAxis);
    return quaternion_from_axis_angle(rotAxis, rotAngle);
}

struct Entity {
	v3f position = {};
	quaternion rotation = quaternion::identity();
	StaticList<ComponentIndex, 16> components;
};

MaskedBlockList<Entity, 256> entities;

template <class T>
T &add_component(Entity &entity, u32 entity_index) {
	static constexpr u32 component_type = get_component_type_index<T>();

	auto added = component_storages[component_type].add();
	T &component = construct(*(T *)added.pointer);

	ComponentIndex component_index = {
		.type = component_type,
		.index = added.index,
		.entity_index = entity_index,
	};
	entity.components.add(component_index);

	component.entity_index = entity_index;
	on_create<T>(component);

	return component;
}
template <class T>
T &add_component(Entity &entity) {
	auto found = index_of(entities, &entity);
	assert(found);
	return add_component<T>(entity, found.value);
}
template <class T>
T &add_component(u32 entity_index) {
	return add_component<T>(entities[entity_index], entity_index);
}


template <class T>
T *get_component(Entity &entity, u32 nth = 0) {
	static constexpr u32 component_type = get_component_type_index<T>();
	for (auto component : entity.components) {
		if (component.type == component_type) {
			return component_storages[component_type].get(component.index);
		}
	}
}
template <class T>
T *get_component(u32 entity_index, u32 nth = 0) {
	return get_component<T>(entities[entity_index], nth);
}

Entity *selected_entity;

Mesh handle_mesh;

t3d::RenderTarget *hdr_target;
t3d::ComputeShader *average_shader;

s32 tl_main(Span<Span<utf8>> arguments) {
	current_printer = console_printer;

	init_component_storages<
#define c(name) name
#define sep ,
		ENUMERATE_COMPONENTS
#undef sep
#undef c
	>();

	CreateWindowInfo info;
	info.on_create = [](Window &window) {
		auto graphics_api = t3d::GraphicsApi_opengl;
		assert_always(t3d::init(graphics_api, {
			.window = window.handle,
			.window_size = window.client_size,
			.debug = true,
		}));

		t3d::set_vsync(true);

		auto hdr_color = t3d::create_texture(t3d::CreateTexture_resize_with_window, 0, 0, 0, t3d::TextureFormat_rgb_f16, t3d::TextureFiltering_nearest, t3d::TextureComparison_none);
		auto hdr_depth = t3d::create_texture(t3d::CreateTexture_resize_with_window, 0, 0, 0, t3d::TextureFormat_depth,   t3d::TextureFiltering_nearest, t3d::TextureComparison_none);
		hdr_target = t3d::create_render_target(hdr_color, hdr_depth);

		global_constants = t3d::create_shader_constants<GlobalConstants>();
		t3d::set_shader_constants(global_constants, GLOBAL_CONSTANTS_SLOT);

		entity_constants = t3d::create_shader_constants<EntityConstants>();
		t3d::set_shader_constants(entity_constants, ENTITY_CONSTANTS_SLOT);

		light_constants = t3d::create_shader_constants<LightConstants>();
		t3d::set_shader_constants(light_constants, LIGHT_CONSTANTS_SLOT);

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
	vec3 camera_position;
};

layout(binding=)" STRINGIZE(ENTITY_CONSTANTS_SLOT) R"(, std140) uniform entity_uniforms {
    mat4 local_to_camera_matrix;
};

layout(binding=)" STRINGIZE(LIGHT_CONSTANTS_SLOT) R"(, std140) uniform light_uniforms {
	mat4 world_to_light_matrix;
	vec3 light_position;
	float light_intensity;
};

#endif

float pow2(float x){return x*x;}
float pow4(float x){return pow2(x*x);}
float pow5(float x){return pow4(x)*x;}

float length_squared(float2 x){return dot(x,x);}
float length_squared(float3 x){return dot(x,x);}
float length_squared(float4 x){return dot(x,x);}

#define pi 3.1415926535897932384626433832795


float3 pbr(float3 albedo, float3 N, float3 L, float3 V) {
	float3 H = normalize(L + V);
	float NV = dot(N, V);
	float NL = saturate(dot(N, L));
	float NH = dot(N, H);
	float VH = dot(V, H);

	float roughness = 0.01f;
	float m2 = roughness*roughness;
	float D =
		exp(-pow2(sqrt(1 - NH*NH)/NH)/m2)
		/
		(pi*m2*pow4(NH));

	float metalness = 0;
	float3 r0 = mix(vec3(0.04), albedo, metalness);

	float3 F = r0 + (1 - r0) * pow5(1 - NV);

	float G = min(1, min(2*NH*NV/VH,2*NH*NL/VH));

	float3 specular = D * F * G / (pi * NV * NL);

	float3 diffuse = albedo * NL * (1 - metalness) / pi * (1 - specular);

	return diffuse + specular;
}

float sample_shadow_map(sampler2DShadow shadow_map, float3 light_space, float bias) {
	float light = 0;
	const int shadow_sample_radius = 1;
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

)"s;

		switch (graphics_api) {
			case t3d::GraphicsApi_opengl: {
				surface_material.constants = t3d::create_shader_constants<SurfaceConstants>();
				t3d::set_value(surface_material.constants, SurfaceConstants{.color = {1,1,1,1}});
				surface_material.shader = t3d::create_shader(concatenate(shader_header, u8R"(
#ifdef VERTEX_SHADER
#define V2F out
#else
#define V2F in
#endif

layout (std140, binding=0) uniform _ {
    vec4 u_color;
};

V2F vec3 vertex_normal;
V2F vec4 vertex_color;
V2F vec3 vertex_world_position;
V2F vec3 vertex_view_direction;
V2F vec3 vertex_to_light_direction;
V2F vec4 vertex_position_in_light_space;

layout(binding=0) uniform sampler2DShadow shadow_map;
layout(binding=1) uniform sampler2D light_texture;

#ifdef VERTEX_SHADER

layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
layout(location=2) in vec4 color;

void main() {
	vec3 local_position = position;
	vertex_normal = normal;
	vertex_color = color * u_color;
	vertex_world_position = local_position;
	vertex_position_in_light_space = world_to_light_matrix * vec4(vertex_world_position, 1);
	vertex_view_direction = camera_position - vertex_world_position;
	vertex_to_light_direction = light_position - vertex_world_position;
	gl_Position = local_to_camera_matrix * vec4(local_position, 1);
}
#endif
#ifdef FRAGMENT_SHADER
out vec4 fragment_color;
void main() {
	fragment_color = vec4(pbr(vertex_color.xyz, normalize(vertex_normal), normalize(vertex_to_light_direction), normalize(vertex_view_direction)), 1);

	vec3 light_space = (vertex_position_in_light_space.xyz / vertex_position_in_light_space.w) * 0.5 + 0.5;

	float light = sample_shadow_map(shadow_map, light_space, 0.001f);
	light *= light_intensity / length_squared(vertex_to_light_direction);

	fragment_color *= light * texture(light_texture, light_space.xy);
	//fragment_color = vec4(light_space, 1);
}
#endif
)"s));
				handle_shader = t3d::create_shader(concatenate(shader_header, u8R"(
#ifdef VERTEX_SHADER
#define V2F out
#else
#define V2F in
#endif

layout (std140, binding=0) uniform _ {
    vec4 u_color;
};

V2F vec4 vertex_color;

#ifdef VERTEX_SHADER

layout(location=0) in vec3 position;
layout(location=2) in vec4 color;

void main() {
	vec3 local_position = position;
	vertex_color = color;
	gl_Position = local_to_camera_matrix * vec4(local_position, 1);
}
#endif
#ifdef FRAGMENT_SHADER
out vec4 fragment_color;
void main() {
	fragment_color = vertex_color;
}
#endif
)"s));
				blit_shader = t3d::create_shader(concatenate(shader_header, u8R"(
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
)"s));
				shadow_map_shader = t3d::create_shader(concatenate(shader_header, u8R"(
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
)"s));
				break;
			}
			/*
			case t3d::GraphicsApi_d3d11: {
				surface_shader = t3d::create_shader<SurfaceShaderValues>(concatenate(shader_header, u8R"(
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

		auto &suzanne         = entities.add();
		auto &floor           = entities.add();
		auto &light           = entities.add();

		{
			auto &mr = add_component<MeshRenderer>(suzanne);
			mr.mesh = load_mesh(TL_FILE_STRING("../data/suzanne.glb"ts));
			mr.material = &surface_material;
		}

		{
			auto &mr = add_component<MeshRenderer>(floor);
			mr.mesh = load_mesh(TL_FILE_STRING("../data/floor.glb"ts));
			mr.material = &surface_material;
		}

		handle_mesh = load_mesh(TL_FILE_STRING("../data/position handle.glb"ts));

		light.position = {0,1,4};
		light.rotation = quaternion_from_euler_r(-pi/6,0,0);
		add_component<Light>(light).texture = t3d::load_texture(TL_FILE_STRING("../data/spotlight_mask.png"ts));

		selected_entity = &light;

	};
	info.on_draw = [](Window &window) {
		camera_rotation.x -= window.mouse_delta.y * 0.01f;
		camera_rotation.y -= window.mouse_delta.x * 0.01f;

		v3f camera_position_delta = {};
		if (key_held(Key_d)) camera_position_delta.x += 1;
		if (key_held(Key_a)) camera_position_delta.x -= 1;
		if (key_held(Key_e)) camera_position_delta.y += 1;
		if (key_held(Key_q)) camera_position_delta.y -= 1;
		if (key_held(Key_s)) camera_position_delta.z += 1;
		if (key_held(Key_w)) camera_position_delta.z -= 1;
		if (camera_position_delta == v3f{}) {
			camera_velocity = 1;
		} else {
			camera_velocity += frame_time;
		}
		camera_position += m3::rotation_r_zxy(camera_rotation) * camera_position_delta * frame_time * camera_velocity;

		m4 camera_projection_matrix = m4::perspective_right_handed((f32)window.client_size.x / window.client_size.y, radians(90), 0.1f, 100.0f);
		m4 camera_translation_matrix = m4::translation(-camera_position);
		m4 camera_rotation_matrix = m4::rotation_r_yxz(-camera_rotation);
		m4 world_to_camera_matrix = camera_projection_matrix * camera_rotation_matrix * camera_translation_matrix;

		auto world_to_camera = [&](v4f point) {
			auto p = world_to_camera_matrix * point;
			return p.xy / p.w;
		};
		auto world_to_camera3 = [&](v3f point) {
			return world_to_camera(V4f(point, 1));
		};

		selected_entity->position.xy = cos_sin(time) + v2f{0,1};

		GlobalConstants global_data = {};
		global_data.camera_position = camera_position;
		t3d::set_value(global_constants, global_data);

		for_each_component_of_type(Light, light) {
			auto &light_entity = entities[light.entity_index];

			t3d::set_render_target(light.shadow_map);
			t3d::set_viewport(shadow_map_resolution, shadow_map_resolution);
			t3d::clear(light.shadow_map, t3d::ClearFlags_depth, {}, 1);

			t3d::set_shader(shadow_map_shader);

			light.world_to_light_matrix = m4::perspective_right_handed(1, radians(90), 0.1f, 10.0f) * -light_entity.rotation * m4::translation(-light_entity.position);

			for_each_component_of_type(MeshRenderer, mesh_renderer) {
				auto &mesh_entity = entities[mesh_renderer.entity_index];
				t3d::set_value(entity_constants, shader_value_location(EntityConstants, local_to_camera_matrix), light.world_to_light_matrix * m4::translation(mesh_entity.position));
				draw_mesh(mesh_renderer.mesh);
			};
		};

		t3d::set_render_target(hdr_target);
		t3d::set_viewport(window.client_size);
		t3d::clear(hdr_target, t3d::ClearFlags_color | t3d::ClearFlags_depth, V4f(.1), 1);

		t3d::set_rasterizer(
			t3d::get_rasterizer()
				.set_depth_test(true)
				.set_depth_write(true)
		);

		for_each_component_of_type(MeshRenderer, mesh_renderer) {
			auto &mesh_entity = entities[mesh_renderer.entity_index];

			t3d::set_shader(mesh_renderer.material->shader);
			t3d::set_shader_constants(mesh_renderer.material->constants, 0);


			EntityConstants entity_data = {};
			entity_data.local_to_camera_matrix = world_to_camera_matrix * m4::translation(mesh_entity.position) * mesh_entity.rotation;
			t3d::set_value(entity_constants, entity_data);
			for_each_component_of_type(Light, light) {
				auto &light_entity = entities[light.entity_index];

				LightConstants light_data = {};
				light_data.world_to_light_matrix = light.world_to_light_matrix;
				light_data.light_position = light_entity.position;
				light_data.light_intensity = 30;
				t3d::set_value(light_constants, light_data);


				t3d::set_texture(light.shadow_map->depth, 0);
				t3d::set_texture(light.texture, 1);
				draw_mesh(mesh_renderer.mesh);
			};
		};

		m4 handle_matrix = m4::translation(selected_entity->position) * selected_entity->rotation * m4::scale(0.25f * distance(selected_entity->position, camera_position));

		EntityConstants entity_data = {};
		entity_data.local_to_camera_matrix = world_to_camera_matrix * handle_matrix;
		t3d::set_value(entity_constants, entity_data);
		t3d::set_shader(handle_shader);
		draw_mesh(handle_mesh);

		v2f handle_position = map(world_to_camera(handle_matrix * v4f{0,0,0,1}), {-1,1}, {1,-1}, {0,0}, (v2f)window.client_size);

		// print("%\n", handle_position);

		t3d::set_rasterizer(
			t3d::get_rasterizer()
				.set_depth_test(false)
				.set_depth_write(false)
		);

		t3d::set_render_target(0);
		t3d::set_shader(blit_shader);
		t3d::set_texture(hdr_target->color, 0);
		t3d::draw(3);

		t3d::present();

		frame_time = reset(frame_timer);
		time += frame_time;
		++fps_counter;

		set_title(&window, tformat(u8"frame_time: % ms, fps: %", frame_time * 1000, fps_counter_result));

		fps_timer += frame_time;
		if (fps_timer >= 1) {
			fps_counter_result = fps_counter;
			fps_timer -= 1;
			fps_counter = 0;
		}
	};
	info.on_size = [](Window &window) {
		t3d::resize_render_targets(window.client_size);
	};

	Window *window = create_window(info);
	assert_always(window);

	frame_timer = create_precise_timer();
	while (update(window)) {
	}

	return 0;
}
