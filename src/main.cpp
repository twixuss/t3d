#include <source_location>
#include <tl/common.h>
tl::umm get_hash(struct ManipulatorStateKey const &);
bool operator==(std::source_location a, std::source_location b) {
	return a.line() == b.line() && a.column() == b.column() && tl::as_span(a.file_name()) == tl::as_span(b.file_name());
}

#include "tl.h"
#include "../dep/tl/include/tl/masked_block_list.h"
#include "component.h"
#include "components/light.h"
#include "components/mesh_renderer.h"
#include "components/camera.h"

using namespace tl;

static PreciseTimer frame_timer;
bool is_editor;

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
	v3f xyz;
	f32 s[4];
	quaternion operator-() const {
		return {-x, -y, -z, w};
	}
	v3f operator*(v3f v) const {
		return v + 2 * cross(xyz, w * v + cross(xyz, v));// / (x*x+y*y+z*z+w*w);
	}
	explicit operator m3() const {
		return {
			1 - 2 * (y*y + z*z),     2 * (x*y - z*w),     2 * (x*z + y*w),
				2 * (x*y + z*w), 1 - 2 * (x*x + z*z),     2 * (y*z - x*w),
				2 * (x*z - y*w),     2 * (y*z + x*w), 1 - 2 * (x*x + y*y),
		};
	}
	explicit operator m4() const {
		return to_m4(operator m3());
	}
	static quaternion identity() {
		return {0,0,0,1};
	}
	quaternion operator*(quaternion b) const {
		quaternion r;
		r.w = w * b.w - dot(xyz, b.xyz);
		r.xyz = w * b.xyz + b.w * xyz + cross(xyz, b.xyz);
		return r;
	}
	quaternion &operator*=(quaternion b) { return *this = *this * b, *this; }
};

quaternion Quaternion(v3f xyz, f32 w) {
	return {xyz.x, xyz.y, xyz.z, w};
}

quaternion normalize(quaternion q) {
	f32 il = 1 / length(v4f{q.x,q.y,q.z,q.w});
	return {q.x*il,q.y*il,q.z*il,q.w*il};
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
quaternion quaternion_from_euler(f32 ax, f32 ay, f32 az) {
	v3f half_angle = v3f{ax, ay, az} * 0.5f;
	v2f csx = cos_sin(half_angle.x);
	v2f csy = cos_sin(half_angle.y);
	v2f csz = cos_sin(half_angle.z);

	f32 a = csx.x;
	f32 b = csx.y;
	f32 c = csy.x;
	f32 d = csy.y;
	f32 e = csz.x;
	f32 f = csz.y;

	return {
		e*c*b + a*d*f,
		-c*b*f + e*a*d,
		c*a*f + -e*d*b,
		c*a*e + d*b*f
	};

	/*
	forceinline v3f cross(v3f a, v3f b) {
		return {
			a.y * b.z - a.z * b.y,
			a.z * b.x - a.x * b.z,
			a.x * b.y - a.y * b.x
		};
	}
	*/

	return Quaternion(
		v3f{csy.x*csx.y,csx.x*csy.y,-csy.y*csx.y},
		csy.x * csx.x
	) * quaternion {
		0,
		0,
		csz.y,
		csz.x,
	};

	return quaternion {
		0,
		csy.y,
		0,
		csy.x,
	} * quaternion {
		csx.y,
		0,
		0,
		csx.x,
	} * quaternion {
		0,
		0,
		csz.y,
		csz.x,
	};

	return quaternion_from_axis_angle({0,1,0}, ay) * quaternion_from_axis_angle({1,0,0}, ax) * quaternion_from_axis_angle({0,0,1}, az);

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
quaternion quaternion_from_euler(v3f v) {
	return quaternion_from_euler(v.x, v.y, v.z);
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
v3f to_euler_angles(quaternion q) {
	// Store the Euler angles in radians
    v3f pitchYawRoll;

    f32 sqw = q.w * q.w;
    f32 sqx = q.x * q.x;
    f32 sqy = q.y * q.y;
    f32 sqz = q.z * q.z;

    // If quaternion is normalised the unit is one, otherwise it is the correction factor
    f32 unit = sqx + sqy + sqz + sqw;
    f32 test = q.x * q.y + q.z * q.w;

    if (test > 0.4999f * unit)                              // 0.4999f OR 0.5f - EPSILON
    {
        // Singularity at north pole
        pitchYawRoll.y = 2 * tl::atan2(q.x, q.w);  // Yaw
        pitchYawRoll.x = pi * 0.5f;                         // Pitch
        pitchYawRoll.z = 0;                                // Roll
    }
    else if (test < -0.4999f * unit)                        // -0.4999f OR -0.5f + EPSILON
    {
        // Singularity at south pole
        pitchYawRoll.y = -2 * tl::atan2(q.x, q.w); // Yaw
        pitchYawRoll.x = -pi * 0.5f;                        // Pitch
        pitchYawRoll.z = 0;                                // Roll
    }
    else
    {
        pitchYawRoll.y = tl::atan2(2 * q.y * q.w - 2 * q.x * q.z, sqx - sqy - sqz + sqw);       // Yaw
        pitchYawRoll.x = asin(2 * test / unit);                                             // Pitch
        pitchYawRoll.z = tl::atan2(2 * q.x * q.w - 2 * q.y * q.z, -sqx + sqy - sqz + sqw);      // Roll
    }

	return {
		-tl::atan2(2*(q.w*q.x + q.y*q.z), 1 - 2*(q.x*q.x + q.y*q.y)),
		-asinf(2*(q.w*q.y - q.z*q.x)),
		-tl::atan2(2*(q.w*q.z + q.x*q.y), 1 - 2*(q.y*q.y + q.z*q.z)),
	};
    return pitchYawRoll;
}

struct Entity {
	v3f position = {};
	//v3f rotation = {};
	quaternion rotation = quaternion::identity();
	StaticList<ComponentIndex, 16> components;
	List<utf8> debug_name;
};

MaskedBlockList<Entity, 256> entities;

Entity &Component::entity() const {
	return entities[entity_index];
}

void destroy(Entity &entity) {
	for (auto &component_index : entity.components) {
		auto &storage = component_storages[component_index.type];
		storage.remove_at(component_index.index);
	}
	free(entity.debug_name);
	entities.remove(entity);
}

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
	return t3d::create_shader(with(temporary_allocator, concatenate(shader_header, source)));
}

Entity *selected_entity;

t3d::ComputeShader *average_shader;
t3d::ComputeBuffer *average_shader_buffer;

t3d::Texture *sky_box_texture;
t3d::Shader *sky_box_shader;

t3d::Texture *floor_lightmap;

Window *window;

enum InputEventKind {
	InputEvent_none,
	InputEvent_mouse_down,
	InputEvent_mouse_up,
	InputEvent_mouse_move,
};

struct InputEvent {
	InputEventKind kind = {};
	struct MouseDown { u8 button; v2s position; };
	struct MouseUp   { u8 button; v2s position; };
	struct MouseMove { v2s position; };
	union {
		MouseDown mouse_down;
		MouseUp   mouse_up;
		MouseMove mouse_move;
	};
};

enum InputResponseKind {
	InputResponse_none,
	InputResponse_begin_drag,
	InputResponse_end_grab,
};

struct InputResponse {
	InputResponseKind kind;
	struct InputHandler *sender;
};

struct InputHandler {
	InputResponse (*_on_input)(void *data, InputEvent event);
	InputResponse on_input(InputEvent event) {
		return _on_input(this, event);
	}
};

template <class T> InputResponse input_handler_on_input(void *data, InputEvent event) { return ((T *)data)->on_input(event); }

enum EditorWindowKind : u16 {
	EditorWindow_none,
	EditorWindow_scene_view,
};

struct EditorWindow : InputHandler {
	EditorWindowKind kind;
	t3d::Viewport viewport;
	void (*_resize)(void *data, t3d::Viewport viewport);
	void resize(t3d::Viewport viewport) {
		_resize(this, viewport);
	}

	void (*_render)(void *data);
	void render() {
		_render(this);
	}

	void (*_free)(void *data);
	void free() {
		_free(this);
	}
};

template <class T> void editor_window_resize(void *data, t3d::Viewport viewport) { ((T *)data)->resize(viewport); }
template <class T> void editor_window_render(void *data) { ((T *)data)->render(); }
template <class T> void editor_window_free(void *data) { ((T *)data)->free(); }

template <class T>
T *create_editor_window() {
	auto result = default_allocator.allocate<T>();
	result->_resize   = editor_window_resize<T>;
	result->_render   = editor_window_render<T>;
	result->_free     = editor_window_free<T>;
	result->_on_input = input_handler_on_input<T>;
	return result;
}

void render_scene(struct SceneViewWindow *);

Entity *current_camera_entity;
Camera *current_camera;
t3d::Viewport current_viewport;

v3f world_to_viewport(v4f point) {
	return map(current_camera->world_to_camera(point), {-1,-1,-1}, {1,1,1}, {0,0,0}, V3f((v2f)current_viewport.size, 1));
}
v3f world_to_viewport(v3f point) {
	return world_to_viewport(V4f(point, 1));
}
v2s get_mouse_position_in_current_viewport() {
	return v2s{window->mouse_position.x, (s32)window->client_size.y - window->mouse_position.y} - current_viewport.position;
}

struct SceneViewWindow : EditorWindow {
	Entity *camera_entity;
	Camera *camera;
	bool flying;

	void resize(t3d::Viewport viewport) {
		this->viewport = viewport;
		for (auto &effect : camera->post_effects) {
			effect.resize(viewport.size);
		}
		t3d::resize_texture(camera->source_target->color, viewport.size);
		t3d::resize_texture(camera->source_target->depth, viewport.size);
		t3d::resize_texture(camera->destination_target->color, viewport.size);
		t3d::resize_texture(camera->destination_target->depth, viewport.size);
	}
	void render() {
		current_camera_entity = camera_entity;
		current_camera = camera;
		current_viewport = viewport;
		render_scene(this);
		current_camera_entity = 0;
		current_camera = 0;
		current_viewport = {};
	}
	void free() {
		destroy(*camera_entity);
	}

	v3f get_drag_position(v2s mouse_position) {
		return {};
	}

	InputResponse on_input(InputEvent event) {
		switch (event.kind) {
			case InputEvent_mouse_down: return on_mouse_down(event.mouse_down);
			case InputEvent_mouse_up  : return on_mouse_up  (event.mouse_up  );
			case InputEvent_mouse_move: return on_mouse_move(event.mouse_move);
		}
		invalid_code_path();
		return {};
	}
	InputResponse on_mouse_down(InputEvent::MouseDown event) {
		event.position -= viewport.position;
		if (event.button == 1) {
			flying = true;
			return {.kind = InputResponse_begin_drag, .sender = this};
		}

		return {};
	}
	InputResponse on_mouse_up(InputEvent::MouseUp event) {
		if (event.button == 1) {
			flying = false;
			return {InputResponse_end_grab};
		}
		return {};
	}
	InputResponse on_mouse_move(InputEvent::MouseMove event) {
		return {};
	}
};

SceneViewWindow *create_scene_view() {
	auto result = create_editor_window<SceneViewWindow>();
	result->kind = EditorWindow_scene_view;
	result->camera_entity = &entities.add();
	result->camera_entity->debug_name = format(u8"scene_camera_%", result);
	result->camera = &add_component<Camera>(*result->camera_entity);
	return result;
}

struct SplitView : InputHandler {
	bool is_split;
	bool is_sizing;
	bool axis_is_x;
	t3d::Viewport viewport;
	EditorWindow *window;
	f32 split_t = 0.5f;
	SplitView *part1;
	SplitView *part2;
	void resize(t3d::Viewport viewport) {
		this->viewport = viewport;
		if (is_split) {
			resize_children();
		} else {
			viewport.x += 1;
			viewport.y += 1;
			viewport.w -= 2;
			viewport.h -= 2;
			window->resize(viewport);
		}
	}
	void resize_children() {
		if (axis_is_x) {
			t3d::Viewport viewport1 = viewport;
			viewport1.h *= split_t;
			part1->resize(viewport1);

			t3d::Viewport viewport2 = viewport;
			viewport2.y = viewport1.y + viewport1.h;
			viewport2.h = viewport.h - viewport1.h;
			part2->resize(viewport2);
		} else {
			t3d::Viewport viewport1 = viewport;
			viewport1.w *= split_t;
			part1->resize(viewport1);

			t3d::Viewport viewport2 = viewport;
			viewport2.x = viewport1.x + viewport1.w;
			viewport2.w = viewport.w - viewport1.w;
			part2->resize(viewport2);
		}
	}
	void render() {
		if (is_split) {

			if (is_sizing) {
				v2s mouse_position = {::window->mouse_position.x, (s32)::window->client_size.y - ::window->mouse_position.y};
				if (axis_is_x) {
					split_t = clamp(map<f32>(mouse_position.y, viewport.y, viewport.y + viewport.h, 0, 1), 0.1f, 0.9f);
				} else {
					split_t = clamp(map<f32>(mouse_position.x, viewport.x, viewport.x + viewport.w, 0, 1), 0.1f, 0.9f);
				}
				resize_children();
			}

			part1->render();
			part2->render();
		} else {
			window->render();
		}
	}

	InputResponse on_input(InputEvent event) {
		switch (event.kind) {
			case InputEvent_mouse_down: return on_mouse_down(event.mouse_down);
			case InputEvent_mouse_up  : return on_mouse_up  (event.mouse_up);
			case InputEvent_mouse_move: return on_mouse_move(event.mouse_move);
		}
		invalid_code_path();
		return {};
	}

	InputResponse on_mouse_down(InputEvent::MouseDown event) {
		v2s mouse_position = {::window->mouse_position.x, (s32)::window->client_size.y - ::window->mouse_position.y};
		if (event.button == 0) {
			f32 const grab_distance = 4;
			if (axis_is_x) {
				s32 bar_position = viewport.y + viewport.h * split_t;
				if (distance((v2f)mouse_position, (line<v2f>)line_begin_end(v2s{viewport.x, bar_position}, v2s{viewport.x + (s32)viewport.w, bar_position})) <= grab_distance) {
					is_sizing = true;
				}
			} else {
				s32 bar_position = viewport.x + viewport.w * split_t;
				if (distance((v2f)mouse_position, (line<v2f>)line_begin_end(v2s{bar_position, viewport.y}, v2s{bar_position, viewport.y + (s32)viewport.h})) <= grab_distance) {
					is_sizing = true;
				}
			}
		}
		if (is_sizing) {
			return {.kind = InputResponse_begin_drag, .sender = this};
		} else {
			InputHandler *target = part2;
			if (is_split) {
				if (in_bounds(mouse_position, aabb_min_size(part1->viewport.position, (v2s)part1->viewport.size))) {
					target = part1;
				}
			} else {
				target = window;
			}
			return target->on_input({.kind = InputEvent_mouse_down, .mouse_down = {.button = event.button}});
		}
	}
	InputResponse on_mouse_up(InputEvent::MouseUp event) {
		if (event.button == 0) {
			if (is_sizing) {
				is_sizing = false;
				return {InputResponse_end_grab};
			}
		}
		//if (is_split) {
		//	part1->on_input(event);
		//	part2->on_input(event);
		//} else {
		//	window->on_input(event);
		//}
		return {};
	}
	InputResponse on_mouse_move(InputEvent::MouseMove event) {
		InputHandler *target = 0;
		if (is_split) {
			if (in_bounds(event.position, aabb_min_size(part1->viewport.position, (v2s)part1->viewport.size))) {
				target = part1;
			} else if (in_bounds(event.position, aabb_min_size(part2->viewport.position, (v2s)part2->viewport.size))) {
				target = part2;
			}
		} else {
			target = window;
		}
		if (target)
			return target->on_input({.kind = InputEvent_mouse_move, .mouse_move = {.position = event.position}});
		else 
			return {};
	}
};

SplitView *create_split_view() {
	auto result = default_allocator.allocate<SplitView>();
	result->_on_input = input_handler_on_input<SplitView>;
	return result;
}

SplitView *main_view;

struct ManipulatedTransform {
	v3f position;
	quaternion rotation;
};

using ManipulateFlags = u8;
enum : ManipulateFlags {
	Manipulate_position = 0x1,
};

struct ManipulatorDrawRequest {
	ManipulateFlags flags;
	u8 highlighted_part_index;
	v3f position;
	quaternion rotation;
	f32 size;
};

List<ManipulatorDrawRequest> manipulator_draw_requests;

inline static constexpr u8 null_manipulator_part = -1;

struct ManipulatorState {
	u8 dragging_part_index = null_manipulator_part;
	v3f drag_offset;
	f32 original_scale;
};

struct ManipulatorStateKey {
	u32 id;
	Camera *camera;
	std::source_location location;
};

umm get_hash(ManipulatorStateKey const &key) {
	return key.id * 954277 + key.location.column() * 152753 + key.location.line() * 57238693 + (u32)key.location.file_name();
}
bool operator==(ManipulatorStateKey const &a, ManipulatorStateKey const &b) {
	return a.id == b.id && a.camera == b.camera && a.location == b.location;
}

StaticHashMap<ManipulatorStateKey, ManipulatorState, 256> manipulator_states;

ManipulatedTransform manipulate_transform(v3f position, quaternion rotation, ManipulateFlags flags, u32 id = 0, std::source_location source_location = std::source_location::current()) {
	ManipulatedTransform manipulated_transform = {
		.position = position,
		.rotation = rotation,
	};

	assert(flags == Manipulate_position, "other modes are not implemented");
	
	auto &state = manipulator_states.get_or_insert({.id = id, .camera = current_camera, .location = source_location});

	auto mouse_position = get_mouse_position_in_current_viewport();

	f32 const handle_size_scale = 0.25f;
	f32 const handle_grab_thickness = 0.05f;
	f32 handle_size = handle_size_scale * current_camera->fov / (pi * 0.5f);

	f32 handle_size_scaled_by_distance = handle_size * dot(position - current_camera_entity->position, current_camera_entity->rotation * v3f{0,0,-1});

	//m4 handle_matrix = m4::translation(position) * m4::rotation_r_zxy(rotation) * m4::scale(handle_size * dot(position - current_camera_entity.position, m4::rotation_r_zxy(current_camera_entity.rotation) * v3f{0,0,-1}));
	m4 handle_matrix = m4::translation(position) * (m4)-rotation * m4::scale(handle_size_scaled_by_distance);

	v3f handle_viewport_position = world_to_viewport(handle_matrix * v4f{0,0,0,1});
	u8 closest_element = null_manipulator_part;
	if (handle_viewport_position.z < 1) {
		Array<v3f, 3> handle_world_axis_tips = {
			(handle_matrix * v4f{1,0,0,1}).xyz,
			(handle_matrix * v4f{0,1,0,1}).xyz,
			(handle_matrix * v4f{0,0,1,1}).xyz
		};

		Array<v2f, 3> handle_viewport_axis_tips = {
			world_to_viewport(handle_world_axis_tips[0]).xy,
			world_to_viewport(handle_world_axis_tips[1]).xy,
			world_to_viewport(handle_world_axis_tips[2]).xy
		};

		Array<v2f, 12> handle_viewport_plane_points = {
			world_to_viewport((handle_matrix * v4f{0,   0.4f,0.4f,1}).xyz).xy,
			world_to_viewport((handle_matrix * v4f{0,   0.4f,0.8f,1}).xyz).xy,
			world_to_viewport((handle_matrix * v4f{0,   0.8f,0.4f,1}).xyz).xy,
			world_to_viewport((handle_matrix * v4f{0,   0.8f,0.8f,1}).xyz).xy,
			world_to_viewport((handle_matrix * v4f{0.4f,0,   0.4f,1}).xyz).xy,
			world_to_viewport((handle_matrix * v4f{0.4f,0,   0.8f,1}).xyz).xy,
			world_to_viewport((handle_matrix * v4f{0.8f,0,   0.4f,1}).xyz).xy,
			world_to_viewport((handle_matrix * v4f{0.8f,0,   0.8f,1}).xyz).xy,
			world_to_viewport((handle_matrix * v4f{0.4f,0.4f,0,   1}).xyz).xy,
			world_to_viewport((handle_matrix * v4f{0.4f,0.8f,0,   1}).xyz).xy,
			world_to_viewport((handle_matrix * v4f{0.8f,0.4f,0,   1}).xyz).xy,
			world_to_viewport((handle_matrix * v4f{0.8f,0.8f,0,   1}).xyz).xy,
		};

		Array<line_segment<v2f>, 12> handle_viewport_plane_lines = {
			line_segment_begin_end(handle_viewport_plane_points[0], handle_viewport_plane_points[1]),
			line_segment_begin_end(handle_viewport_plane_points[2], handle_viewport_plane_points[3]),
			line_segment_begin_end(handle_viewport_plane_points[0], handle_viewport_plane_points[2]),
			line_segment_begin_end(handle_viewport_plane_points[1], handle_viewport_plane_points[3]),

			line_segment_begin_end(handle_viewport_plane_points[4], handle_viewport_plane_points[5]),
			line_segment_begin_end(handle_viewport_plane_points[6], handle_viewport_plane_points[7]),
			line_segment_begin_end(handle_viewport_plane_points[4], handle_viewport_plane_points[6]),
			line_segment_begin_end(handle_viewport_plane_points[5], handle_viewport_plane_points[7]),

			line_segment_begin_end(handle_viewport_plane_points[ 8], handle_viewport_plane_points[ 9]),
			line_segment_begin_end(handle_viewport_plane_points[10], handle_viewport_plane_points[11]),
			line_segment_begin_end(handle_viewport_plane_points[ 8], handle_viewport_plane_points[10]),
			line_segment_begin_end(handle_viewport_plane_points[ 9], handle_viewport_plane_points[11]),
		};

		f32 closest_dist = max_value<f32>;
		for (u32 axis_index = 0; axis_index != 3; ++axis_index) {
			f32 dist = distance(line_segment_begin_end(handle_viewport_axis_tips[axis_index], handle_viewport_position.xy), (v2f)mouse_position);
			if (dist < closest_dist) {
				closest_dist = dist;
				closest_element = axis_index;
			}

			Array<line_segment<v2f>, 4> plane_lines = {
				line_segment_begin_end(handle_viewport_plane_points[axis_index*4 + 0], handle_viewport_plane_points[axis_index*4 + 1]),
				line_segment_begin_end(handle_viewport_plane_points[axis_index*4 + 2], handle_viewport_plane_points[axis_index*4 + 3]),
				line_segment_begin_end(handle_viewport_plane_points[axis_index*4 + 0], handle_viewport_plane_points[axis_index*4 + 2]),
				line_segment_begin_end(handle_viewport_plane_points[axis_index*4 + 1], handle_viewport_plane_points[axis_index*4 + 3]),
			};
			for (u32 line_index = 0; line_index != 4; ++line_index) {
				dist = min(dist, distance(plane_lines[line_index], (v2f)mouse_position));
			}
			if (dist < closest_dist) {
				closest_dist = dist;
				closest_element = axis_index + 3;
			}
		}
		if (closest_dist > window->client_size.y * handle_grab_thickness) {
			closest_element = null_manipulator_part;
		}

		bool begin_drag = false;
		if (closest_element != null_manipulator_part && mouse_down(0)) {
			begin_drag = true;
			state.dragging_part_index = closest_element;
			state.original_scale = handle_size_scaled_by_distance;
		}

		m4 camera_to_world_matrix = inverse(current_camera->world_to_camera_matrix);

		if (state.dragging_part_index != null_manipulator_part) {
			v3f new_position;
			if (state.dragging_part_index < 3) {
				v2f closest_in_viewport = closest_point(line_begin_end(handle_viewport_position.xy, handle_viewport_axis_tips[state.dragging_part_index]), (v2f)mouse_position);

				v4f end = camera_to_world_matrix * V4f(map(closest_in_viewport, {}, (v2f)current_viewport.size, {-1,-1}, {1,1}),1, 1);
				ray<v3f> cursor_ray = ray_origin_end(current_camera_entity->position, end.xyz / end.w);

				cursor_ray.direction = normalize(cursor_ray.direction);
				new_position = closest_point(line_begin_end(position, handle_world_axis_tips[state.dragging_part_index]), as_line(cursor_ray));
			} else {
				v3f plane_normal = handle_world_axis_tips[state.dragging_part_index - 3] - position;

				v4f end = camera_to_world_matrix * V4f(map((v2f)mouse_position, {}, (v2f)current_viewport.size, {-1,-1}, {1,1}),1, 1);
				ray<v3f> cursor_ray = ray_origin_end(current_camera_entity->position, end.xyz / end.w);

				auto d = dot(position, -plane_normal);
				auto t = -(d + dot(cursor_ray.origin, plane_normal)) / dot(cursor_ray.direction, plane_normal);
				new_position = cursor_ray.origin + t * cursor_ray.direction;
			}
			if (begin_drag) {
				state.drag_offset = manipulated_transform.position - new_position;
			} else {
				manipulated_transform.position = new_position + state.drag_offset * (handle_size_scaled_by_distance / state.original_scale);
			}
		}
	}
	print("%\n", handle_viewport_position.z);
	
	if (mouse_up(0)) {
		state.dragging_part_index = null_manipulator_part;
	}


	manipulator_draw_requests.add({
		.flags = flags,
		.highlighted_part_index = (state.dragging_part_index != null_manipulator_part) ? state.dragging_part_index : closest_element,
		.position = position,
		.rotation = rotation,
		.size = handle_size,
	});

	return manipulated_transform;
}

void render_scene(SceneViewWindow *view) {
	timed_function();

	//print("%\n", to_euler_angles(quaternion_from_euler(0, time, time)));
	//selected_entity->qrotation = quaternion_from_euler(to_euler_angles(quaternion_from_euler(0, time, time)));


	auto &camera = *view->camera;
	auto &camera_entity = *view->camera_entity;

	v2s mouse_position = v2s{
		window->mouse_position.x,
		((s32)window->client_size.y - window->mouse_position.y),
	} - view->viewport.position;

	v3f camera_position_delta = {};
	if (view->flying) {
		camera.fov = clamp(camera.fov - window->mouse_wheel * radians(10), radians(30.0f), radians(120.0f));

		camera_entity.rotation =
			quaternion_from_axis_angle({0,1,0}, window->mouse_delta.x * -0.005f * camera.fov) *
			quaternion_from_axis_angle(camera_entity.rotation * v3f{1,0,0}, window->mouse_delta.y * -0.005f * camera.fov) *
			camera_entity.rotation;

		if (key_held(Key_d)) camera_position_delta.x += 1;
		if (key_held(Key_a)) camera_position_delta.x -= 1;
		if (key_held(Key_e)) camera_position_delta.y += 1;
		if (key_held(Key_q)) camera_position_delta.y -= 1;
		if (key_held(Key_s)) camera_position_delta.z += 1;
		if (key_held(Key_w)) camera_position_delta.z -= 1;
		if (all_true(camera_position_delta == v3f{})) {
			camera_velocity = 1;
		} else {
			camera_velocity += frame_time;
		}
	}
	//camera_entity.position += m4::rotation_r_zxy(camera_entity.rotation) * camera_position_delta * frame_time * camera_velocity;
	camera_entity.position += camera_entity.rotation * camera_position_delta * frame_time * camera_velocity;

	m4 camera_projection_matrix = m4::perspective_right_handed((f32)view->viewport.size.x / view->viewport.size.y, camera.fov, 0.1f, 100.0f);
	m4 camera_translation_matrix = m4::translation(-camera_entity.position);
	//m4 camera_rotation_matrix = m4::rotation_r_yxz(-camera_entity.rotation);
	//m4 camera_rotation_matrix = m4::rotation_r_yxz(-to_euler_angles(camera_entity.rotation));
	m4 camera_rotation_matrix = (m4)camera_entity.rotation;
	camera.world_to_camera_matrix = camera_projection_matrix * camera_rotation_matrix * camera_translation_matrix;

	t3d::update_shader_constants(global_constants, {
		.camera_rotation_projection_matrix = camera_projection_matrix * camera_rotation_matrix,
		.camera_position = camera_entity.position,
		//.camera_forward = m3::rotation_r_zxy(camera_entity.rotation) * v3f{0,0,-1},
		.camera_forward = camera_entity.rotation * v3f{0,0,-1},
	});

	t3d::set_render_target(camera.destination_target);
	t3d::set_viewport(camera.destination_target->color->size);
	t3d::clear(camera.destination_target, t3d::ClearFlags_color | t3d::ClearFlags_depth, {.9,.1,.9,1}, 1);

	t3d::set_rasterizer({
		.depth_test = true,
		.depth_write = true,
		.depth_func = t3d::Comparison_less,
	});
	t3d::set_blend(t3d::BlendFunction_disable, {}, {});

	u32 light_index = 0;
	for_each_component_of_type(Light, light) {
		timed_block("Light"s);

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
			timed_block("MeshRenderer"s);
			auto &mesh_entity = entities[mesh_renderer.entity_index];

			t3d::set_shader(mesh_renderer.material->shader);
			t3d::set_shader_constants(mesh_renderer.material->constants, 0);


			EntityConstants entity_data = {};
			entity_data.local_to_camera_matrix = camera.world_to_camera_matrix * m4::translation(mesh_entity.position) * (m4)mesh_entity.rotation;
			//entity_data.local_to_camera_matrix = camera.world_to_camera_matrix * m4::translation(mesh_entity.position) * m4::rotation_r_zxy(mesh_entity.rotation);
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

	swap(camera.source_target, camera.destination_target);

	{
		timed_block("Post effects"s);
		if (is_editor) {
			for (auto &effect : camera.post_effects) {
				effect.render(camera.source_target, camera.destination_target);
				swap(camera.source_target, camera.destination_target);
			}

			t3d::set_render_target(t3d::back_buffer);
			t3d::set_viewport(view->viewport);
			t3d::set_shader(blit_shader);
			t3d::set_texture(camera.source_target->color, 0);
			t3d::draw(3);
		} else {
			for (auto &effect : camera.post_effects) {
				if (&effect == &camera.post_effects.back()) {
					effect.render(camera.source_target, t3d::back_buffer);
				} else {
					effect.render(camera.source_target, camera.destination_target);
				}
				swap(camera.source_target, camera.destination_target);
			}
		}
	}

	t3d::set_blend(t3d::BlendFunction_disable, {}, {});

	t3d::clear(t3d::back_buffer, t3d::ClearFlags_depth, {}, 1);

	t3d::set_rasterizer({
		.depth_test = true,
		.depth_write = true,
		.depth_func = t3d::Comparison_less,
	});

	selected_entity->position = manipulate_transform(selected_entity->position, selected_entity->rotation, Manipulate_position).position;

	for (auto &request : manipulator_draw_requests) {
		assert(request.flags == Manipulate_position, "other modes are not implemented");

		EntityConstants entity_data = {};
		v3f camera_to_handle_direction = normalize(request.position - camera_entity.position);
		entity_data.local_to_camera_matrix =
			camera.world_to_camera_matrix
			* m4::translation(camera_entity.position + camera_to_handle_direction)
			* (m4)-request.rotation
			* m4::scale(request.size * dot(camera_to_handle_direction, camera_entity.rotation * v3f{0,0,-1}));
		t3d::update_shader_constants(entity_constants, entity_data);
		t3d::set_shader(handle_shader);
		t3d::set_shader_constants(handle_constants, 0);


		u32 selected_element = request.highlighted_part_index;

		t3d::update_shader_constants(handle_constants, {.color = V3f(1), .selected = (f32)(selected_element != -1)});
		draw_mesh(handle_center_mesh);

		t3d::update_shader_constants(handle_constants, {.color = V3f(1,0,0), .selected = (f32)(selected_element == 0)});
		draw_mesh(handle_axis_x_mesh);

		t3d::update_shader_constants(handle_constants, {.color = V3f(0,1,0), .selected = (f32)(selected_element == 1)});
		draw_mesh(handle_axis_y_mesh);

		t3d::update_shader_constants(handle_constants, {.color = V3f(0,0,1), .selected = (f32)(selected_element == 2)});
		draw_mesh(handle_axis_z_mesh);

		t3d::update_shader_constants(handle_constants, {.color = V3f(1,0,0), .selected = (f32)(selected_element == 3)});
		draw_mesh(handle_plane_x_mesh);

		t3d::update_shader_constants(handle_constants, {.color = V3f(0,1,0), .selected = (f32)(selected_element == 4)});
		draw_mesh(handle_plane_y_mesh);

		t3d::update_shader_constants(handle_constants, {.color = V3f(0,0,1), .selected = (f32)(selected_element == 5)});
		draw_mesh(handle_plane_z_mesh);
	}
	manipulator_draw_requests.clear();
}

void run() {
	manipulator_draw_requests = {};
	manipulator_states = {};

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

		//t3d::set_vsync(false);

		global_constants = t3d::create_shader_constants<GlobalConstants>();
		t3d::set_shader_constants(global_constants, GLOBAL_CONSTANTS_SLOT);

		entity_constants = t3d::create_shader_constants<EntityConstants>();
		t3d::set_shader_constants(entity_constants, ENTITY_CONSTANTS_SLOT);

		light_constants = t3d::create_shader_constants<LightConstants>();
		t3d::set_shader_constants(light_constants, LIGHT_CONSTANTS_SLOT);

		average_shader_buffer = t3d::create_compute_buffer(sizeof(u32));
		t3d::set_compute_buffer(average_shader_buffer, AVERAGE_COMPUTE_SLOT);

		main_view = create_split_view();

		main_view->is_split = true;
		main_view->part1 = create_split_view();
		main_view->part1->window = create_scene_view();

		main_view->part2 = create_split_view();
		main_view->part2->is_split = true;
		main_view->part2->axis_is_x = true;
		main_view->part2->part1 = create_split_view();
		main_view->part2->part1->window = create_scene_view();
		main_view->part2->part2 = create_split_view();
		main_view->part2->part2->window = create_scene_view();

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
		suzanne.debug_name = as_list(u8"suzanne"s);
		auto &floor           = entities.add();
		floor.debug_name = as_list(u8"floor"s);

		{
			auto &mr = add_component<MeshRenderer>(suzanne);
			mr.mesh = load_mesh(tl_file_string("../data/suzanne.glb"ts));
			mr.material = &surface_material;
		}

		{
			auto &mr = add_component<MeshRenderer>(floor);
			mr.mesh = load_mesh(tl_file_string("../data/floor.glb"ts));
			mr.material = &surface_material;
			mr.lightmap = t3d::load_texture(tl_file_string("../data/floor_lightmap.png"ts));
		}

		auto handle_meshes = parse_glb_from_file(tl_file_string("../data/handle.glb"ts)).get();
		handle_center_mesh  = create_mesh(*handle_meshes.get_node(u8"Center"s).mesh);
		handle_axis_x_mesh  = create_mesh(*handle_meshes.get_node(u8"AxisX"s ).mesh);
		handle_axis_y_mesh  = create_mesh(*handle_meshes.get_node(u8"AxisY"s ).mesh);
		handle_axis_z_mesh  = create_mesh(*handle_meshes.get_node(u8"AxisZ"s ).mesh);
		handle_plane_x_mesh = create_mesh(*handle_meshes.get_node(u8"PlaneX"s).mesh);
		handle_plane_y_mesh = create_mesh(*handle_meshes.get_node(u8"PlaneY"s).mesh);
		handle_plane_z_mesh = create_mesh(*handle_meshes.get_node(u8"PlaneZ"s).mesh);

		auto light_texture = t3d::load_texture(tl_file_string("../data/spotlight_mask.png"ts));

		{
			auto &light = entities.add();
			light.debug_name = as_list(u8"light1"s);
			light.position = {0,2,6};
			light.rotation = quaternion_from_euler(-pi/10,0,pi/6);
			add_component<Light>(light).texture = light_texture;
			selected_entity = &light;
		}

		{
			auto &light = entities.add();
			light.debug_name = as_list(u8"light2"s);
			light.position = {6,2,-6};
			light.rotation = quaternion_from_euler(-pi/10,pi*0.75,0);
			add_component<Light>(light).texture = light_texture;
		}

		sky_box_texture = t3d::load_texture({
			.left   = tl_file_string("../data/sky_x+.hdr"ts),
			.right  = tl_file_string("../data/sky_x-.hdr"ts),
			.top    = tl_file_string("../data/sky_y+.hdr"ts),
			.bottom = tl_file_string("../data/sky_y-.hdr"ts),
			.front  = tl_file_string("../data/sky_z-.hdr"ts),
			.back   = tl_file_string("../data/sky_z+.hdr"ts),
		});

		print("%\n%\n", m4::rotation_r_zxy(1,2,3), transpose(m4::rotation_r_yxz(-1,-2,-3)));
	};
	info.on_draw = [](Window &window) {
		static v2u old_window_size;
		if (old_window_size != window.client_size) {
			old_window_size = window.client_size;

			main_view->resize({.position = {}, .size = window.client_size});

			t3d::resize_render_targets(window.client_size);
		}

		if (key_down(Key_f1)) {
			Profiler::enabled = true;
			Profiler::reset();
		}
		defer {
			if (key_down(Key_f1)) {
				Profiler::enabled = false;
				write_entire_file(tl_file_string("update.tmd"ts), Profiler::output_for_timed());
			}
		};
		timed_block("frame"s);

		static v2s old_mouse_position;
		v2s mouse_position = {window.mouse_position.x, (s32)window.client_size.y - window.mouse_position.y};
		if (any_true(window.mouse_position != old_mouse_position)) {
			main_view->on_input({.kind = InputEvent_mouse_move, .mouse_move = {.position = mouse_position}});
			old_mouse_position = mouse_position;
		}

		{
			timed_block("Shadows"s);

			t3d::clear(t3d::back_buffer, t3d::ClearFlags_color | t3d::ClearFlags_depth, V4f(.1), 1);

			t3d::set_rasterizer(
				t3d::get_rasterizer()
					.set_depth_test(true)
					.set_depth_write(true)
					.set_depth_func(t3d::Comparison_less)
			);
			t3d::set_blend(t3d::BlendFunction_disable, {}, {});

			for_each_component_of_type(Light, light) {
				timed_block("Light"s);
				auto &light_entity = entities[light.entity_index];

				t3d::set_render_target(light.shadow_map);
				t3d::set_viewport(shadow_map_resolution, shadow_map_resolution);
				t3d::clear(light.shadow_map, t3d::ClearFlags_depth, {}, 1);

				t3d::set_shader(shadow_map_shader);

				//light.world_to_light_matrix = m4::perspective_right_handed(1, radians(90), 0.1f, 100.0f) * m4::rotation_r_yxz(-light_entity.rotation) * m4::translation(-light_entity.position);
				light.world_to_light_matrix = m4::perspective_right_handed(1, radians(90), 0.1f, 100.0f) * (m4)light_entity.rotation * m4::translation(-light_entity.position);

				for_each_component_of_type(MeshRenderer, mesh_renderer) {
					auto &mesh_entity = entities[mesh_renderer.entity_index];
					t3d::update_shader_constants(entity_constants, shader_value_location(EntityConstants, local_to_camera_matrix), light.world_to_light_matrix * m4::translation(mesh_entity.position));
					draw_mesh(mesh_renderer.mesh);
				};
			};
		}

		{
			timed_block("main_view->render()"s);
			main_view->render();
		}

		{
			timed_block("present"s);
			t3d::present();
		}

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

		clear_temporary_storage();
	};

	window = create_window(info);
	defer { free(window); };

	assert_always(window);

	static InputHandler *grabbed = 0;
	on_mouse_down = [](u8 button){
		v2s mouse_position = {window->mouse_position.x, (s32)window->client_size.y - window->mouse_position.y};
		auto response = main_view->on_input({
			.kind = InputEvent_mouse_down,
			.mouse_down = {.button = button, .position = mouse_position}
		});
		if (response.kind == InputResponse_begin_drag) {
			grabbed = response.sender;
		}
	};
	on_mouse_up   = [](u8 button){
		v2s mouse_position = {window->mouse_position.x, (s32)window->client_size.y - window->mouse_position.y};
		main_view->on_input({
			.kind = InputEvent_mouse_up,
			.mouse_up = {.button = button, .position = mouse_position}
		});
		if (grabbed) {
			grabbed->on_input({
				.kind = InputEvent_mouse_up,
				.mouse_up = {.button = button, .position = mouse_position}
			});
		}
	};

	frame_timer = create_precise_timer();

	Profiler::enabled = false;
	Profiler::reset();

	while (update(window)) {
	}

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
	current_printer = console_printer;

	run();

#if TRACK_ALLOCATIONS
	auto file_printer = create_file_printer(tl_file_string("log.txt"ts));
	defer { close(file_printer.file); };

	current_printer = file_printer;

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
