#pragma once
#include "../dep/tl/include/tl/common.h"
#include "../dep/tl/include/tl/vector.h"
#include "../dep/tl/include/tl/math.h"

#define T3D_API

namespace t3d {

using namespace TL;

enum GraphicsApi {
	GraphicsApi_null,
	GraphicsApi_d3d11,
	GraphicsApi_opengl,
};

struct InitInfo {
	NativeWindowHandle window = {};
	v2u window_size = {};
	bool debug = false;
};

struct RenderTarget {};
struct Shader {};
struct VertexBuffer {};
struct IndexBuffer {};

struct ShaderValueLocation {
	umm start;
	umm size;
};

struct Mesh {
	void *vertices;
	void *indices;
};

struct CameraMatrices {
	m4 mvp;
};

enum ElementType {
	Element_f32x1,
	Element_f32x2,
	Element_f32x3,
	Element_f32x4,
};

enum CreateRenderTargetFlags {
	CreateRenderTarget_default            = 0x0,
	CreateRenderTarget_resize_with_window = 0x1,
};

enum TextureFormat {
	TextureFormat_null,
	TextureFormat_r_f32,
};

using ClearFlags = u32;
enum : ClearFlags {
	ClearFlags_none  = 0x0,
	ClearFlags_color = 0x1,
	ClearFlags_depth = 0x2,
};

#define APIS(A) \
A(void, clear, (RenderTarget *render_target, ClearFlags flags, v4f color, f32 depth), (render_target, flags, color, depth)) \
A(void, present, (), ()) \
A(void, draw, (u32 vertex_count, u32 start_vertex), (vertex_count, start_vertex)) \
A(void, draw_indexed, (u32 index_count), (index_count)) \
A(void, set_viewport, (u32 x, u32 y, u32 w, u32 h), (x, y, w, h)) \
A(void, resize, (RenderTarget *render_target, u32 w, u32 h), (render_target, w, h)) \
A(void, set_shader, (Shader *shader), (shader)) \
A(void, set_value, (Shader *shader, ShaderValueLocation dest, void const *source), (shader, dest, source)) \
A(Shader *, create_shader, (Span<utf8> source, umm values_size), (source, values_size)) \
A(CameraMatrices, calculate_perspective_matrices, (v3f position, v3f rotation, f32 aspect_ratio, f32 fov_radians, f32 near_plane, f32 far_plane), (position, rotation, aspect_ratio, fov_radians, near_plane, far_plane)) \
A(VertexBuffer *, create_vertex_buffer, (Span<u8> buffer, Span<ElementType> vertex_descriptor), (buffer, vertex_descriptor)) \
A(void, set_vertex_buffer, (VertexBuffer *buffer), (buffer)) \
A(IndexBuffer *, create_index_buffer, (Span<u8> buffer, u32 index_size), (buffer, index_size)) \
A(void, set_index_buffer, (IndexBuffer *buffer), (buffer)) \
A(void, set_vsync, (bool enable), (enable)) \
A(void, set_render_target, (RenderTarget *target), (target)) \
A(RenderTarget *, create_render_target, (CreateRenderTargetFlags flags, TextureFormat format, u32 width, u32 height), (flags, format, width, height)) \

#define A(ret, name, args, values) extern T3D_API ret (*_##name) args;
APIS(A)
#undef A

#define A(ret, name, args, values) inline ret name args { return _##name values; }
APIS(A)
#undef A

inline void draw(u32 vertex_count) { return _draw(vertex_count, 0); }
inline void set_viewport(u32 w, u32 h) { return _set_viewport(0, 0, w, h); }
inline void set_viewport(v2u size) { return _set_viewport(0, 0, size.x, size.y); }
inline void resize(RenderTarget *render_target, v2u size) { return _resize(render_target, size.x, size.y); }
template <class T>
inline void set_value(Shader *shader, ShaderValueLocation dest, T const &source) {
	assert(sizeof(source) == dest.size);
	return _set_value(shader, dest, &source);
}
template <class T>
inline void set_value(Shader *shader, T const &source) {
	return _set_value(shader, {0, sizeof(source)}, &source);
}
template <class T>
Shader *create_shader(Span<utf8> source) { return _create_shader(source, sizeof(T)); }


struct Shaders {
	struct Color {
		inline static T3D_API Shader *shader;
		inline static constexpr ShaderValueLocation color_location = {0, 16};
		inline static constexpr umm size = 16;
	};
};

#ifndef T3D_IMPL
#undef APIS
#endif

bool init(GraphicsApi api, InitInfo init_info);

}
