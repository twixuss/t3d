#define TL_ENABLE_VEC4_SIMD
#include "../dep/tl/include/tl/common.h"
#include "../dep/tl/include/tl/window.h"
#include "../dep/tl/include/tl/console.h"
#include "../dep/tl/include/tl/time.h"
#include "../dep/tl/include/tl/math.h"
#include "../dep/tl/include/tl/mesh.h"
#include "../include/t3d.h"

using namespace TL;

v3f camera_position;
v3f camera_rotation;
f32 frame_time = 1 / 60.0f;
f32 time;
PreciseTimer frame_timer;
t3d::Shader *surface_shader;
t3d::Shader *shadow_map_shader;

struct SurfaceShaderValues {
	m4 mvp_matrix;
	v4f color;
	v3f camera_position;
};

struct ShadowMapShaderValues {
	m4 mvp_matrix;
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

t3d::RenderTarget *shadow_map;
u32 const shadow_map_resolution = 1024;

s32 tl_main(Span<Span<utf8>> arguments) {
	current_printer = console_printer;


	CreateWindowInfo info;
	info.on_create = [](Window &window) {
		auto graphics_api = t3d::GraphicsApi_opengl;
		assert_always(t3d::init(graphics_api, {
			.window = window.handle,
			.window_size = window.client_size,
			.debug = true,
		}));

		t3d::set_vsync(true);

		suzanne_mesh = load_mesh(TL_FILE_STRING("../data/suzanne.glb"ts));
		floor_mesh   = load_mesh(TL_FILE_STRING("../data/floor.glb"ts));

		shadow_map = t3d::create_render_target(t3d::CreateRenderTarget_default, t3d::TextureFormat_r_f32, shadow_map_resolution, shadow_map_resolution);

		auto shader_header = u8R"(
#ifdef GL_core_profile
#define float2 vec2
#define float3 vec3
#define float4 vec4
float saturate(float a){return clamp(a,0,1);}
#endif

float pow2(float x){return x*x;}
float pow4(float x){return pow2(x*x);}
float pow5(float x){return pow4(x)*x;}

#define pi 3.1415926535897932384626433832795


float3 pbr(float3 albedo, float3 N, float3 L, float3 V) {
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

	return albedo * NL * kdiff + kspec;
}
)"s;

		switch (graphics_api) {
			case t3d::GraphicsApi_opengl: {
				surface_shader = t3d::create_shader<SurfaceShaderValues>(concatenate(shader_header, u8R"(
#ifdef VERTEX_SHADER
#define V2F out
#else
#define V2F in
#endif

layout (std140) uniform _ {
    mat4 u_matrix;
    vec4 u_color;
	vec3 camera_position;
};

V2F vec3 vertex_normal;
V2F vec3 vertex_world_position;
V2F vec3 vertex_view_direction;

#ifdef VERTEX_SHADER

layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
layout(location=2) in vec4 color;

void main() {
	vec3 local_position = position;
	vertex_normal = normal;
	vertex_world_position = local_position;
	vertex_view_direction = camera_position - vertex_world_position;
	gl_Position = u_matrix * vec4(local_position, 1);
}
#endif
#ifdef FRAGMENT_SHADER
out vec4 fragment_color;
void main() {
	fragment_color = vec4(pbr(u_color.xyz, normalize(vertex_normal), normalize(vec3(1,3,2)), normalize(vertex_view_direction)), 1);
}
#endif
)"s));
				shadow_map_shader = t3d::create_shader<ShadowMapShaderValues>(u8R"(
layout (std140) uniform _ {
    mat4 u_matrix;
};

#ifdef VERTEX_SHADER

layout(location=0) in vec3 position;

void main() {
	vec3 local_position = position;
	gl_Position = u_matrix * vec4(local_position, 1);
}
#endif
#ifdef FRAGMENT_SHADER
#endif
)"s);
				break;
			}
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
		}
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
		camera_position += m3::rotation_r_zxy(camera_rotation) * camera_position_delta * frame_time;

		auto camera_matrices = t3d::calculate_perspective_matrices(camera_position, camera_rotation, (f32)window.client_size.x / window.client_size.y, radians(90), 0.1f, 100.0f);


		t3d::set_render_target(shadow_map);
		t3d::set_viewport(shadow_map_resolution, shadow_map_resolution);
		t3d::clear(shadow_map, t3d::ClearFlags_depth, {}, 1);

		t3d::set_render_target(0);
		t3d::set_viewport(window.client_size);
		t3d::clear(0, t3d::ClearFlags_color | t3d::ClearFlags_depth, {0.3f, 0.6f, 0.9f, 1.0f}, 1);

		t3d::set_shader(surface_shader);

		SurfaceShaderValues values;
		values.mvp_matrix = camera_matrices.mvp;
		values.color = {1,0,1,1};
		values.camera_position = camera_position;
		t3d::set_value(surface_shader, values);

		draw_mesh(suzanne_mesh);
		draw_mesh(floor_mesh);

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
		t3d::resize({}, window.client_size);
	};

	Window *window = create_window(info);
	assert_always(window);

	frame_timer = create_precise_timer();
	while (update(window)) {
	}

	return 0;
}
