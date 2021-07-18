#include "tl.h"
#include "../dep/tl/include/tl/masked_block_list.h"
#include "component.h"
#include "light.h"
#include "mesh_renderer.h"
#include "camera.h"

using namespace tl;

v3f camera_position;
v3f camera_rotation;
f32 frame_time = 1 / 60.0f;
f32 time;
PreciseTimer frame_timer;


struct GlobalConstants {
	m4 camera_rotation_projection_matrix;

	v3f camera_position;
	f32 _dummy;

	v3f camera_forward;
};
t3d::TypedShaderConstants<GlobalConstants> global_constants;
#define GLOBAL_CONSTANTS_SLOT 15


struct EntityConstants {
	m4 local_to_camera_matrix;
};
t3d::TypedShaderConstants<EntityConstants> entity_constants;
#define ENTITY_CONSTANTS_SLOT 14


struct LightConstants {
	m4 world_to_light_matrix;

	v3f light_position;
	f32 light_intensity;

	u32 light_index;
};
t3d::TypedShaderConstants<LightConstants> light_constants;
#define LIGHT_CONSTANTS_SLOT 13


struct AverageComputeData {
	u32 sum;
};
#define AVERAGE_COMPUTE_SLOT 12

t3d::Shader *shadow_map_shader;
t3d::Shader *blit_shader;

struct BlitColorConstants {
	v4f color;
};
t3d::TypedShaderConstants<BlitColorConstants> blit_color_constants;
t3d::Shader *blit_color_shader;

struct HandleConstants {
	v3f color;
	f32 selected;
};
t3d::TypedShaderConstants<HandleConstants> handle_constants;
t3d::Shader *handle_shader;

Material surface_material;
struct SurfaceConstants {
	v4f color;
};

#define shader_value_location(struct, member) t3d::ShaderValueLocation{offsetof(struct, member), sizeof(struct::member)}

u32 fps_counter;
u32 fps_counter_result;
f32 fps_timer;

Mesh *suzanne_mesh;
Mesh *floor_mesh;
Mesh *handle_center_mesh;
Mesh *handle_axis_x_mesh;
Mesh *handle_axis_y_mesh;
Mesh *handle_axis_z_mesh;
Mesh *handle_plane_x_mesh;
Mesh *handle_plane_y_mesh;
Mesh *handle_plane_z_mesh;

f32 camera_velocity;



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
	q.y = y.x * x.y * z.x + y.y * x.x * z.y;
	q.z = y.y * x.x * z.x - y.x * x.y * z.y;
	q.w = y.y * x.y * z.y + y.x * x.x * z.x;
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
	v3f rotation = {};
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

#define SHADOW_MAP_TEXTURE_SLOT 15
#define LIGHT_TEXTURE_SLOT      14
#define LIGHTMAP_TEXTURE_SLOT	13

t3d::Shader *create_shader(Span<utf8> source) {
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

	vec3 camera_position;
	float _dummy;

	vec3 camera_forward;
};

layout(binding=)" STRINGIZE(ENTITY_CONSTANTS_SLOT) R"(, std140) uniform entity_uniforms {
	mat4 local_to_camera_matrix;
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
	return t3d::create_shader(concatenate(shader_header, source));
}

Entity *selected_entity;

t3d::RenderTarget *source_target;
t3d::RenderTarget *destination_target;

t3d::ComputeShader *average_shader;
t3d::ComputeBuffer *average_shader_buffer;

struct PostEffect {
	Allocator allocator = current_allocator;
	void *data;
	void (*_init)(void *data);
	void (*_free)(void *data);
	void (*_render)(void *data, t3d::RenderTarget *source, t3d::RenderTarget *destination);
	void (*_resize)(void *data, v2u size);

	void init() {
		_init(data);
	}
	void free() {
		_free(data);
	}
	void render(t3d::RenderTarget *source, t3d::RenderTarget *destination) {
		_render(data, source, destination);
	}
	void resize(v2u size) {
		_resize(data, size);
	}
};

List<PostEffect> post_effects;

template <class Effect>
Effect &add_post_effect() {
	PostEffect effect;
	effect.allocator = current_allocator;
	effect.data = effect.allocator.allocate<Effect>();
	effect._init   = Effect::init;
	effect._free   = Effect::free;
	effect._render = Effect::render;
	effect._resize = Effect::resize;
	effect.init();
	post_effects.add(effect);
	return *(Effect *)effect.data;
}

struct Exposure {
	inline static constexpr u32 min_texture_size = 1;
	struct Constants {
		f32 exposure_offset;
		f32 exposure_scale;
	};

	enum ApproachKind {
		Approach_lerp,
		Approach_log_lerp,
	};

	enum MaskKind {
		Mask_one,
		Mask_proximity,
	};

	ApproachKind approach_kind;

	MaskKind mask_kind;
	f32 mask_radius;

	t3d::Shader *shader;
	t3d::TypedShaderConstants<Constants> constants;
	f32 exposure = 1;
	f32 scale;
	f32 limit_min = 0;
	f32 limit_max = 1 << 24;
	List<t3d::RenderTarget *> downsampled_targets;

	static void init(void *data) {
		auto &exposure = *(Exposure *)data;
		exposure.constants = t3d::create_shader_constants<Exposure::Constants>();
		exposure.shader = t3d::create_shader(u8R"(
#ifdef VERTEX_SHADER
#define V2F out
#else
#define V2F in
#endif

layout (std140, binding=0) uniform _ {
	float exposure_offset;
	float exposure_scale;
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
	vec3 color = texture(main_texture, vertex_uv).rgb;
	color = 1 - exp(-color * exposure_offset);
	color = -log(max(1 - color, 0.000000000001));
	//color = min(color, vec3(256));
	//color = log(color + 1);
	//color = pow(color, vec3(1 / 2.2));
	fragment_color = vec4(color, 1);
}
#endif
)"s);
	}


	static void render(void *data, t3d::RenderTarget *source, t3d::RenderTarget *destination) {
		auto &exposure = *(Exposure *)data;
		t3d::set_rasterizer(
			t3d::get_rasterizer()
				.set_depth_test(false)
				.set_depth_write(false)
		);

		t3d::set_blend(t3d::BlendFunction_disable, {}, {});

		t3d::set_shader(blit_shader);

		auto sample_from = source;
		for (auto &target : exposure.downsampled_targets) {
			t3d::set_render_target(target);
			t3d::set_viewport(target->color->size);
			t3d::set_texture(sample_from->color, 0);
			t3d::draw(3);
			sample_from = target;
		}
		v3f texels[Exposure::min_texture_size * Exposure::min_texture_size];

		t3d::read_texture(exposure.downsampled_targets.back()->color, as_bytes(array_as_span(texels)));

		f32 target_exposure = 0;
		switch (exposure.mask_kind) {
			case Exposure::Mask_one: {
				f32 sum_luminance = 0;
				for (auto texel : texels) {
					sum_luminance += max(texel.x, texel.y, texel.z);
				}
				if (sum_luminance == 0) {
					target_exposure = exposure.limit_max;
				} else {
					target_exposure = clamp(1 / sum_luminance * count_of(texels), exposure.limit_min, exposure.limit_max);
				}
				break;
			}
			case Exposure::Mask_proximity: {
				f32 sum_luminance = 0;
				f32 sum_mask = 0;
				for (u32 y = 0; y < Exposure::min_texture_size; ++y) {
					for (u32 x = 0; x < Exposure::min_texture_size; ++x) {
						auto texel = texels[y*Exposure::min_texture_size+x];
						f32 dist = distance(V2f(x,y), V2f(63)*0.5);

						constexpr f32 inv_diagonal = 1 / max(1, CE::sqrt(pow2(Exposure::min_texture_size * 0.5f - 0.5f) * 2));

						f32 mask = map_clamped(dist * inv_diagonal, exposure.mask_radius, 0.0f, 0.0f, 1.0f);
						sum_mask += mask;
						sum_luminance += mask * max(texel.x, texel.y, texel.z);
					}
				}
				if (sum_luminance == 0) {
					target_exposure = exposure.limit_max;
				} else {
					target_exposure = clamp(1 / sum_luminance * sum_mask, exposure.limit_min, exposure.limit_max);
				}
				break;
			}
			default:
				invalid_code_path("exposure.mask_kind is invalid");
				break;
		}
		exposure.exposure = pow(2, lerp(log2(exposure.exposure), log2(target_exposure), frame_time));

		t3d::update_shader_constants(exposure.constants, {
			.exposure_offset = +exposure.exposure * exposure.scale,
			.exposure_scale = exposure.scale
		});

		t3d::set_shader(exposure.shader);
		t3d::set_shader_constants(exposure.constants, 0);
		t3d::set_render_target(destination);
		t3d::set_viewport(destination->color->size);
		t3d::set_texture(source->color, 0);
		t3d::draw(3);
	}

	static void resize(void *data, v2u client_size) {
		auto &exposure = *(Exposure *)data;
		v2u next_size = floor_to_power_of_2(client_size - 1);
		u32 target_index = 0;
		while (1) {
			if (target_index < exposure.downsampled_targets.size) {
				t3d::resize_texture(exposure.downsampled_targets[target_index]->color, next_size);
			} else {
				exposure.downsampled_targets.add(t3d::create_render_target(
					t3d::create_texture(t3d::CreateTexture_default, next_size.x, next_size.y, 0, t3d::TextureFormat_rgb_f16, t3d::TextureFiltering_linear, t3d::Comparison_none),
					0
				));
			}

			if (next_size.x == Exposure::min_texture_size && next_size.y == Exposure::min_texture_size) break;

			if (next_size.x != Exposure::min_texture_size) next_size.x /= 2;
			if (next_size.y != Exposure::min_texture_size) next_size.y /= 2;

			++target_index;
		}
	}
	static void free(void *data) {}
};

struct Bloom {
	inline static constexpr u32 min_texture_size = 1;
	struct Constants {
		v2f texel_size;
		f32 threshold;
	};

	t3d::Shader *downsample_shader;
	t3d::Shader *upsample_shader;
	t3d::Shader *downsample_filter_shader;
	t3d::TypedShaderConstants<Constants> constants;
	List<t3d::RenderTarget *> downsampled_targets;
	f32 threshold = 1;

	static void init(void *data) {
		auto &bloom = *(Bloom *)data;
		bloom.constants = t3d::create_shader_constants<Bloom::Constants>();

		constexpr auto header = u8R"(
#ifdef VERTEX_SHADER
#define V2F out
#else
#define V2F in
#endif

layout (std140, binding=0) uniform _ {
	vec2 texel_size;
	float threshold;
	float intensity;
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

vec4 get_sample(vec2 vertex_uv, float offset) {
	return (texture(main_texture, vertex_uv + texel_size * vec2( offset, offset))
		  + texture(main_texture, vertex_uv + texel_size * vec2(-offset, offset))
		  + texture(main_texture, vertex_uv + texel_size * vec2( offset,-offset))
		  + texture(main_texture, vertex_uv + texel_size * vec2(-offset,-offset))) * 0.25f;
}

vec4 apply_filter(vec4 c) {
	float brightness = max(c.r, max(c.g, c.b));
	float contribution = max(0, brightness - threshold);
	contribution /= max(brightness, 0.00001);
	return c * contribution;
}

)"s;
		bloom.downsample_shader = t3d::create_shader(concatenate(header, u8R"(
#ifdef FRAGMENT_SHADER
out vec4 fragment_color;
void main() {
	fragment_color = get_sample(vertex_uv, 1);
}
#endif
)"s));
		bloom.downsample_filter_shader = t3d::create_shader(concatenate(header, u8R"(
#ifdef FRAGMENT_SHADER
out vec4 fragment_color;
void main() {
	fragment_color = apply_filter(get_sample(vertex_uv, 1));
}
#endif
)"s));
		bloom.upsample_shader = t3d::create_shader(concatenate(header, u8R"(
#ifdef FRAGMENT_SHADER
out vec4 fragment_color;
void main() {
	fragment_color = get_sample(vertex_uv, 0.5);
}
#endif
)"s));
	}

	static void render(void *data, t3d::RenderTarget *source, t3d::RenderTarget *destination) {
		auto &bloom = *(Bloom *)data;
		t3d::set_rasterizer(
			t3d::get_rasterizer()
				.set_depth_test(false)
				.set_depth_write(false)
		);

		t3d::set_shader(bloom.downsample_filter_shader);
		t3d::set_shader_constants(bloom.constants, 0);

		auto sample_from = source;
		for (auto &target : bloom.downsampled_targets) {
			t3d::set_render_target(target);
			t3d::set_viewport(target->color->size);
			t3d::set_texture(sample_from->color, 0);
			t3d::update_shader_constants(bloom.constants, {.texel_size = 1.0f / (v2f)sample_from->color->size, .threshold = bloom.threshold});
			t3d::draw(3);
			sample_from = target;

			if (&target == &bloom.downsampled_targets.front())
				t3d::set_shader(bloom.downsample_shader);
		}

		t3d::set_shader(bloom.upsample_shader);
		for (s32 i = 0; i < bloom.downsampled_targets.size - 1; ++i) {
			auto &target = bloom.downsampled_targets[i];
			auto &sample_from = bloom.downsampled_targets[i + 1];

			t3d::set_render_target(target);
			t3d::set_viewport(target->color->size);
			t3d::set_texture(sample_from->color, 0);
			t3d::update_shader_constants(bloom.constants, {.texel_size = 1.0f / (v2f)sample_from->color->size});
			t3d::draw(3);
		}

		t3d::set_shader(blit_shader);
		t3d::set_render_target(destination);
		t3d::set_viewport(destination->color->size);
		t3d::set_blend(t3d::BlendFunction_disable, {}, {});
		t3d::set_texture(source->color, 0);
		t3d::draw(3);

		t3d::set_shader(blit_color_shader);
		t3d::update_shader_constants(blit_color_constants, {.color = V4f(0.1f)});
		t3d::set_shader_constants(blit_color_constants, 0);
		t3d::set_blend(t3d::BlendFunction_add, t3d::Blend_one, t3d::Blend_one);
		for (auto &target : bloom.downsampled_targets) {
			t3d::set_texture(target->color, 0);
			t3d::draw(3);
			//if (&target == &bloom.downsampled_targets[0])
			//	break;
		}

		//t3d::set_blend(t3d::BlendFunction_disable, {}, {});
		//t3d::set_texture(bloom.downsampled_targets[2]->color, 0);
		//t3d::draw(3);
		//t3d::set_blend(t3d::BlendFunction_add, t3d::Blend_one, t3d::Blend_one);
		//t3d::set_texture(source->color, 0);
		//t3d::set_texture(bloom.downsampled_targets[0]->color, 0);
		//t3d::draw(3);
		//t3d::set_texture(bloom.downsampled_targets[1]->color, 0);
		//t3d::draw(3);
		//t3d::set_texture(bloom.downsampled_targets[2]->color, 0);
		//t3d::draw(3);
	}

	static void resize(void *data, v2u client_size) {
		auto &bloom = *(Bloom *)data;
		v2u next_size = floor_to_power_of_2(client_size - 1);
		u32 target_index = 0;
		while (1) {
			if (target_index < bloom.downsampled_targets.size) {
				t3d::resize_texture(bloom.downsampled_targets[target_index]->color, next_size);
			} else {
				bloom.downsampled_targets.add(t3d::create_render_target(
					t3d::create_texture(t3d::CreateTexture_default, next_size.x, next_size.y, 0, t3d::TextureFormat_rgb_f16, t3d::TextureFiltering_linear, t3d::Comparison_none),
					0
				));
			}

			if (next_size.x == Bloom::min_texture_size && next_size.y == Bloom::min_texture_size) break;

			if (next_size.x != Bloom::min_texture_size) next_size.x /= 2;
			if (next_size.y != Bloom::min_texture_size) next_size.y /= 2;

			++target_index;
		}
	}
	static void free(void *data) {}
};

struct Dither {
	struct Constants {
		f32 time;
	};

	t3d::Shader *shader;
	t3d::TypedShaderConstants<Constants> constants;

	static void init(void *data) {
		auto &dither = *(Dither *)data;
		dither.constants = t3d::create_shader_constants<Dither::Constants>();

		dither.shader = t3d::create_shader(u8R"(
#ifdef VERTEX_SHADER
#define V2F out
#else
#define V2F in
#endif
layout (std140, binding=0) uniform _ {
	float time;
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
	fragment_color = texture(main_texture, vertex_uv);
	fragment_color += (vec4(fract(sin(dot(gl_FragCoord.xy + time, vec2(12.9898, 78.233))) * 43758.5453)) - 0.5f) / 256;
}
#endif
)"s);
	}

	static void render(void *data, t3d::RenderTarget *source, t3d::RenderTarget *destination) {
		auto &dither = *(Dither *)data;
		t3d::set_rasterizer(
			t3d::get_rasterizer()
				.set_depth_test(false)
				.set_depth_write(false)
		);
		t3d::set_blend(t3d::BlendFunction_disable, {}, {});

		t3d::set_shader(dither.shader);
		t3d::set_shader_constants(dither.constants, 0);

		t3d::update_shader_constants(dither.constants, {.time = time});

		t3d::set_render_target(destination);
		t3d::set_texture(source->color, 0);
		t3d::draw(3);
	}

	static void resize(void *data, v2u client_size) {}
	static void free(void *data) {}
};

t3d::Texture *sky_box_texture;
t3d::Shader *sky_box_shader;

t3d::Texture *floor_lightmap;

m4 world_to_camera_matrix;

Window *window;

t3d::Viewport render_viewport() {
	return {
		.size = window->client_size,
	};
}

v3f world_to_camera(v4f point) {
	auto p = world_to_camera_matrix * point;
	return p.xyz / p.w;
}
v3f world_to_camera(v3f point) {
	return world_to_camera(V4f(point, 1));
}
v2f world_to_window(v4f point) {
	return map(world_to_camera(point).xy, {-1,1}, {1,-1}, {0,0}, (v2f)window->client_size);
}
v2f world_to_window(v3f point) {
	return world_to_window(V4f(point, 1));
}

void update_frame() {

	static v3f base_camera_rotation;
	static v3f target_camera_rotation;
	static f32 camera_rotation_lerp_t;

	bool flying = mouse_held(1);

	main_camera->fov = clamp(main_camera->fov - window->mouse_wheel * radians(10), radians(30.0f), radians(120.0f));

	if (flying) {
		target_camera_rotation.x -= window->mouse_delta.y * 0.01f * main_camera->fov;
		target_camera_rotation.y -= window->mouse_delta.x * 0.01f * main_camera->fov;
		if (window->mouse_delta.x || window->mouse_delta.y) {
			base_camera_rotation = camera_rotation;
			camera_rotation_lerp_t = 0;
		}
	}

#if 1
	camera_rotation_lerp_t = 1;
#else
	camera_rotation_lerp_t = min(1, camera_rotation_lerp_t + frame_time * 30);
#endif
	camera_rotation = lerp(base_camera_rotation, target_camera_rotation, V3f(camera_rotation_lerp_t));

	v3f camera_position_delta = {};
	if (flying) {
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
	}
	camera_position += m3::rotation_r_zxy(camera_rotation) * camera_position_delta * frame_time * camera_velocity;

	m4 camera_projection_matrix = m4::perspective_right_handed((f32)window->client_size.x / window->client_size.y, main_camera->fov, 0.1f, 100.0f);
	m4 camera_translation_matrix = m4::translation(-camera_position);
	m4 camera_rotation_matrix = m4::rotation_r_yxz(-camera_rotation);
	world_to_camera_matrix = camera_projection_matrix * camera_rotation_matrix * camera_translation_matrix;

	t3d::update_shader_constants(global_constants, {
		.camera_rotation_projection_matrix = camera_projection_matrix * camera_rotation_matrix,
		.camera_position = camera_position,
		.camera_forward = m3::rotation_r_zxy(camera_rotation) * v3f{0,0,-1},
	});

	t3d::set_rasterizer(
		t3d::get_rasterizer()
			.set_depth_test(true)
			.set_depth_write(true)
			.set_depth_func(t3d::Comparison_less)
	);
	t3d::set_blend(t3d::BlendFunction_disable, {}, {});

	for_each_component_of_type(Light, light) {
		auto &light_entity = entities[light.entity_index];

		t3d::set_render_target(light.shadow_map);
		t3d::set_viewport(shadow_map_resolution, shadow_map_resolution);
		t3d::clear(light.shadow_map, t3d::ClearFlags_depth, {}, 1);

		t3d::set_shader(shadow_map_shader);

		light.world_to_light_matrix = m4::perspective_right_handed(1, radians(90), 0.1f, 100.0f) * m4::rotation_r_yxz(-light_entity.rotation) * m4::translation(-light_entity.position);

		for_each_component_of_type(MeshRenderer, mesh_renderer) {
			auto &mesh_entity = entities[mesh_renderer.entity_index];
			t3d::update_shader_constants(entity_constants, shader_value_location(EntityConstants, local_to_camera_matrix), light.world_to_light_matrix * m4::translation(mesh_entity.position));
			draw_mesh(mesh_renderer.mesh);
		};
	};

	t3d::set_blend(t3d::BlendFunction_disable, {}, {});
	t3d::set_render_target(destination_target);
	t3d::set_viewport(window->client_size);
	t3d::clear(destination_target, t3d::ClearFlags_color | t3d::ClearFlags_depth, V4f(.1), 1);

	u32 light_index = 0;
	for_each_component_of_type(Light, light) {
		defer {
			++light_index;
		};

		auto &light_entity = entities[light.entity_index];

		t3d::update_shader_constants(light_constants, {
			.world_to_light_matrix = light.world_to_light_matrix,
			.light_position = light_entity.position,
			.light_intensity = 100,
			.light_index = light_index,
		});


		t3d::set_texture(light.shadow_map->depth, SHADOW_MAP_TEXTURE_SLOT);
		t3d::set_texture(light.texture, LIGHT_TEXTURE_SLOT);
		for_each_component_of_type(MeshRenderer, mesh_renderer) {
			auto &mesh_entity = entities[mesh_renderer.entity_index];

			t3d::set_shader(mesh_renderer.material->shader);
			t3d::set_shader_constants(mesh_renderer.material->constants, 0);


			EntityConstants entity_data = {};
			entity_data.local_to_camera_matrix = world_to_camera_matrix * m4::translation(mesh_entity.position) * m4::rotation_r_zxy(mesh_entity.rotation);
			t3d::update_shader_constants(entity_constants, entity_data);
			t3d::set_texture(mesh_renderer.lightmap, LIGHTMAP_TEXTURE_SLOT);
			draw_mesh(mesh_renderer.mesh);
		};
		t3d::set_blend(t3d::BlendFunction_add, t3d::Blend_one, t3d::Blend_one);
		t3d::set_rasterizer(t3d::get_rasterizer()
			.set_depth_test(true)
			.set_depth_write(false)
			.set_depth_func(t3d::Comparison_equal)
		);
	};

	t3d::set_rasterizer(t3d::get_rasterizer()
		.set_depth_test(true)
		.set_depth_write(false)
		.set_depth_func(t3d::Comparison_equal)
	);
	t3d::set_blend(t3d::BlendFunction_disable, {}, {});

	t3d::set_shader(sky_box_shader);
	t3d::set_texture(sky_box_texture, 0);
	t3d::draw(36);

	swap(source_target, destination_target);

	// print("%\n", handle_window_position);


	if (false) {
		t3d::set_rasterizer(
			t3d::get_rasterizer()
				.set_depth_test(false)
				.set_depth_write(false)
		);

		t3d::set_render_target(destination_target);
		t3d::set_viewport(t3d::back_buffer->color->size);
		t3d::set_shader(blit_shader);
		t3d::set_texture(source_target->color, 0);
		t3d::draw(3);
	} else {
		for (auto &effect : post_effects) {
			if (&effect == &post_effects.back()) {
				effect.render(source_target, t3d::back_buffer);
			} else {
				effect.render(source_target, destination_target);
			}
			swap(source_target, destination_target);
		}
	}

}

s32 tl_main(Span<Span<utf8>> arguments) {
	current_printer = console_printer;

	post_effects = {};

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

		auto create_hdr_target = [&]() {
			auto hdr_color = t3d::create_texture(t3d::CreateTexture_resize_with_window, 0, 0, 0, t3d::TextureFormat_rgb_f16, t3d::TextureFiltering_linear, t3d::Comparison_none);
			auto hdr_depth = t3d::create_texture(t3d::CreateTexture_resize_with_window, 0, 0, 0, t3d::TextureFormat_depth,   t3d::TextureFiltering_none,   t3d::Comparison_none);
			return t3d::create_render_target(hdr_color, hdr_depth);
		};
		source_target      = create_hdr_target();
		destination_target = create_hdr_target();

		global_constants = t3d::create_shader_constants<GlobalConstants>();
		t3d::set_shader_constants(global_constants, GLOBAL_CONSTANTS_SLOT);

		entity_constants = t3d::create_shader_constants<EntityConstants>();
		t3d::set_shader_constants(entity_constants, ENTITY_CONSTANTS_SLOT);

		light_constants = t3d::create_shader_constants<LightConstants>();
		t3d::set_shader_constants(light_constants, LIGHT_CONSTANTS_SLOT);

		average_shader_buffer = t3d::create_compute_buffer(sizeof(u32));
		t3d::set_compute_buffer(average_shader_buffer, AVERAGE_COMPUTE_SLOT);

		auto &exposure = add_post_effect<Exposure>();
		exposure.scale = 0.5f;
		exposure.limit_min = 1.0f / 16;
		exposure.limit_max = 1024;
		exposure.approach_kind = Exposure::Approach_log_lerp;
		exposure.mask_kind = Exposure::Mask_one;
		exposure.mask_radius = 1;

		auto &bloom = add_post_effect<Bloom>();
		bloom.threshold = 1;

		auto &dither = add_post_effect<Dither>();

		switch (graphics_api) {
			case t3d::GraphicsApi_opengl: {
				surface_material.constants = t3d::create_shader_constants(sizeof(SurfaceConstants));
				t3d::update_shader_constants(surface_material.constants, SurfaceConstants{.color = {1,1,1,1}});
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
	vertex_normal = normal;
	vertex_color = color * u_color;
	vertex_world_position = local_position;
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
	light *= light_intensity / length_squared(vertex_to_light_direction);
	fragment_color *= light * texture(light_texture, light_space.xy);

	if (light_index == 0) {
		fragment_color += texture(lightmap_texture, vertex_uv) / pi;
	}

	//fragment_color = texture(lightmap_texture, vertex_uv);
}
#endif
)"s);
				handle_constants = t3d::create_shader_constants<HandleConstants>();
				handle_shader = create_shader(u8R"(
layout (std140, binding=0) uniform _ {
	vec3 u_color;
	float selected;
};

V2F vec4 vertex_color;
V2F vec3 vertex_normal;

#ifdef VERTEX_SHADER

layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;

void main() {
	vec3 local_position = position;
	vertex_color = vec4(u_color, 1);
	vertex_normal = normal;
	gl_Position = local_to_camera_matrix * vec4(local_position, 1);
}
#endif
#ifdef FRAGMENT_SHADER
out vec4 fragment_color;
void main() {
	fragment_color = mix(mix(vec4(0.5f), vertex_color * 0.5, dot(vertex_normal, -camera_forward) * 0.5 + 0.5), vertex_color, selected);
}
#endif
)"s);
				blit_shader = t3d::create_shader(u8R"(
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
				blit_color_constants = t3d::create_shader_constants<BlitColorConstants>();
				blit_color_shader = t3d::create_shader(u8R"(
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
	vertex_uv = local_position;
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
				average_shader = t3d::create_compute_shader(u8R"(
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

		u32 white_pixel = ~0;
		white_texture = t3d::create_texture(t3d::CreateTexture_default, 1, 1, &white_pixel, t3d::TextureFormat_rgba_u8n, t3d::TextureFiltering_nearest, t3d::Comparison_none);

		u32 black_pixel = 0xFF000000;
		black_texture = t3d::create_texture(t3d::CreateTexture_default, 1, 1, &black_pixel, t3d::TextureFormat_rgba_u8n, t3d::TextureFiltering_nearest, t3d::Comparison_none);

		auto &suzanne         = entities.add();
		auto &floor           = entities.add();

		{
			auto &mr = add_component<MeshRenderer>(suzanne);
			mr.mesh = load_mesh(TL_FILE_STRING("../data/suzanne.glb"ts));
			mr.material = &surface_material;
		}

		{
			auto &mr = add_component<MeshRenderer>(floor);
			mr.mesh = load_mesh(TL_FILE_STRING("../data/floor.glb"ts));
			mr.material = &surface_material;
			mr.lightmap = t3d::load_texture(TL_FILE_STRING("../data/floor_lightmap.png"ts));
		}

		auto handle_meshes = parse_glb_from_file(TL_FILE_STRING("../data/handle.glb"ts)).get();
		handle_center_mesh  = create_mesh(*handle_meshes.get_node(u8"Center"s).mesh);
		handle_axis_x_mesh  = create_mesh(*handle_meshes.get_node(u8"AxisX"s ).mesh);
		handle_axis_y_mesh  = create_mesh(*handle_meshes.get_node(u8"AxisY"s ).mesh);
		handle_axis_z_mesh  = create_mesh(*handle_meshes.get_node(u8"AxisZ"s ).mesh);
		handle_plane_x_mesh = create_mesh(*handle_meshes.get_node(u8"PlaneX"s).mesh);
		handle_plane_y_mesh = create_mesh(*handle_meshes.get_node(u8"PlaneY"s).mesh);
		handle_plane_z_mesh = create_mesh(*handle_meshes.get_node(u8"PlaneZ"s).mesh);

		auto light_texture = t3d::load_texture(TL_FILE_STRING("../data/spotlight_mask.png"ts));

		{
			auto &light = entities.add();
			light.position = {0,2,6};
			light.rotation = {-pi/10,0,0};
			add_component<Light>(light).texture = light_texture;
			selected_entity = &light;
		}

		{
			auto &light = entities.add();
			light.position = {6,2,-6};
			light.rotation = {-pi/10,pi*0.75,0};
			add_component<Light>(light).texture = light_texture;
		}

		auto &camera_entity = entities.add();
		auto &camera = add_component<Camera>(camera_entity);

		sky_box_texture = t3d::load_texture({
			.left   = TL_FILE_STRING("../data/sky_x+.hdr"ts),
			.right  = TL_FILE_STRING("../data/sky_x-.hdr"ts),
			.top    = TL_FILE_STRING("../data/sky_y+.hdr"ts),
			.bottom = TL_FILE_STRING("../data/sky_y-.hdr"ts),
			.front  = TL_FILE_STRING("../data/sky_z-.hdr"ts),
			.back   = TL_FILE_STRING("../data/sky_z+.hdr"ts),
		});
	};
	info.on_draw = [](Window &window) {
		update_frame();

		t3d::set_blend(t3d::BlendFunction_disable, {}, {});

		f32 const handle_size_scale = 0.25f;
		f32 const handle_grab_thickness = 0.1f;
		f32 handle_size = handle_size_scale * main_camera->fov / (pi * 0.5f);
		m4 handle_matrix = m4::translation(selected_entity->position) * m4::rotation_r_zxy(selected_entity->rotation) * m4::scale(handle_size * dot(selected_entity->position - camera_position, m3::rotation_r_zxy(camera_rotation) * v3f{0,0,-1}));

		v3f handle_world_x = (handle_matrix * v4f{1,0,0,1}).xyz;
		v3f handle_world_y = (handle_matrix * v4f{0,1,0,1}).xyz;
		v3f handle_world_z = (handle_matrix * v4f{0,0,1,1}).xyz;
		Array<v3f, 3> handle_world_axes = { handle_world_x, handle_world_y, handle_world_z };

		v2f handle_window_position = world_to_window(handle_matrix * v4f{0,0,0,1});
		v2f handle_window_x = world_to_window(handle_world_x);
		v2f handle_window_y = world_to_window(handle_world_y);
		v2f handle_window_z = world_to_window(handle_world_z);
		Array<v2f, 3> handle_window_axes = { handle_window_x, handle_window_y, handle_window_z };

		Array<v2f, 3> handle_window_planes = {
			world_to_window((handle_matrix * v4f{0,   0.4f,0.4f,1}).xyz),
			world_to_window((handle_matrix * v4f{0.4f,0,   0.4f,1}).xyz),
			world_to_window((handle_matrix * v4f{0.4f,0.4f,0,   1}).xyz),
		};


		f32 closest_dist = max_value<f32>;
		u32 closest_element = -1;
		for (u32 axis_index = 0; axis_index != 3; ++axis_index) {
			f32 dist = distance(line_begin_end(handle_window_axes[axis_index], handle_window_position), (v2f)window.mouse_position);
			if (dist < closest_dist) {
				closest_dist = dist;
				closest_element = axis_index;
			}

			dist = distance(handle_window_planes[axis_index], (v2f)window.mouse_position);
			if (dist < closest_dist) {
				closest_dist = dist;
				closest_element = axis_index + 3;
			}
		}

		static u32 dragging_element = -1;
		static v3f dragging_offset;

		m4 camera_to_world_matrix = inverse(world_to_camera_matrix);

		auto get_drag_cursor_world_position = [&]() {
			if (dragging_element < 3) {
				v2f closest_in_window = closest_point_unclamped(line_begin_end(handle_window_position, handle_window_axes[dragging_element]), (v2f)window.mouse_position);

				v4f end = camera_to_world_matrix * V4f(map(closest_in_window, {}, (v2f)window.client_size, {-1,1}, {1,-1}),1, 1);
				ray<v3f> cursor_ray = ray_origin_end(camera_position, end.xyz / end.w);

				cursor_ray.direction = normalize(cursor_ray.direction);
				return closest_point(line_begin_end(selected_entity->position, handle_world_axes[dragging_element]), as_line(cursor_ray));
			} else {
				v3f plane_normal = handle_world_axes[dragging_element - 3] - selected_entity->position;

				v4f end = camera_to_world_matrix * V4f(map((v2f)window.mouse_position, {}, (v2f)window.client_size, {-1,1}, {1,-1}),1, 1);
				ray<v3f> cursor_ray = ray_origin_end(camera_position, end.xyz / end.w);

				auto d = dot(selected_entity->position, -plane_normal);
				auto t = -(d + dot(cursor_ray.origin, plane_normal)) / dot(cursor_ray.direction, plane_normal);
				return cursor_ray.origin + t * cursor_ray.direction;
			}
		};

		v3f handle_color = V3f(0.5f);
		if (closest_dist < window.client_size.y * handle_grab_thickness) {
			handle_color.s[closest_element % 3] = 1;
			if (mouse_down(0)) {
				dragging_element = closest_element;
				dragging_offset = selected_entity->position - get_drag_cursor_world_position();
			}
		} else {
			closest_element = -1;
		}

		if (mouse_up(0)) {
			dragging_element = -1;
		}



		if (dragging_element != -1) {
			selected_entity->position = get_drag_cursor_world_position() + dragging_offset;
		}


		t3d::clear(t3d::back_buffer, t3d::ClearFlags_depth, {}, 1);

		t3d::set_rasterizer({
			.depth_test = true,
			.depth_write = true,
			.depth_func = t3d::Comparison_less,
		});

		EntityConstants entity_data = {};
		v3f camera_to_handle_direction = normalize(selected_entity->position - camera_position);
		entity_data.local_to_camera_matrix =
			world_to_camera_matrix
			* m4::translation(camera_position + camera_to_handle_direction)
			* m4::rotation_r_zxy(selected_entity->rotation)
			* m4::scale(handle_size * dot(camera_to_handle_direction, m3::rotation_r_zxy(camera_rotation) * v3f{0,0,-1}));
		t3d::update_shader_constants(entity_constants, entity_data);
		t3d::set_shader(handle_shader);
		t3d::set_shader_constants(handle_constants, 0);

		t3d::update_shader_constants(handle_constants, {.color = V3f(1), .selected = (f32)(closest_element != -1)});
		draw_mesh(handle_center_mesh);

		t3d::update_shader_constants(handle_constants, {.color = V3f(1,0,0), .selected = (f32)(closest_element == 0)});
		draw_mesh(handle_axis_x_mesh);

		t3d::update_shader_constants(handle_constants, {.color = V3f(0,1,0), .selected = (f32)(closest_element == 1)});
		draw_mesh(handle_axis_y_mesh);

		t3d::update_shader_constants(handle_constants, {.color = V3f(0,0,1), .selected = (f32)(closest_element == 2)});
		draw_mesh(handle_axis_z_mesh);

		t3d::update_shader_constants(handle_constants, {.color = V3f(1,0,0), .selected = (f32)(closest_element == 3)});
		draw_mesh(handle_plane_x_mesh);

		t3d::update_shader_constants(handle_constants, {.color = V3f(0,1,0), .selected = (f32)(closest_element == 4)});
		draw_mesh(handle_plane_y_mesh);

		t3d::update_shader_constants(handle_constants, {.color = V3f(0,0,1), .selected = (f32)(closest_element == 5)});
		draw_mesh(handle_plane_z_mesh);

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
		for (auto &effect : post_effects) {
			effect.resize(window.client_size);
		}
	};

	window = create_window(info);
	assert_always(window);

	frame_timer = create_precise_timer();
	while (update(window)) {
	}

	return 0;
}
