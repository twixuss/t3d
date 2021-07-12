#pragma once
#include <tl/common.h>
#include <tl/vector.h>
#include <tl/file.h>
#include <tl/math.h>
#include <stb_image.h>

#define T3D_API

namespace t3d {

using namespace tl;

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

struct Texture {
	v2u size;
};
struct RenderTarget {
	Texture *color;
	Texture *depth;
};
struct Shader {};
struct VertexBuffer {};
struct IndexBuffer {};
struct ShaderConstants {};
struct ComputeShader {};
struct ComputeBuffer {};

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

enum ElementType : u8 {
	Element_f32x1,
	Element_f32x2,
	Element_f32x3,
	Element_f32x4,
};

enum CreateTextureFlags : u8 {
	CreateTexture_default            = 0x0,
	CreateTexture_resize_with_window = 0x1,
};

enum TextureFormat : u8 {
	TextureFormat_null,
	TextureFormat_depth,
	TextureFormat_r_f32,
	TextureFormat_rgb_f16,
	TextureFormat_rgba_u8n,
	TextureFormat_rgba_f16,
};

enum TextureFiltering : u8 {
	TextureFiltering_none,    // texture will be unsamplable
	TextureFiltering_nearest,
	TextureFiltering_linear,
};

enum TextureComparison : u8 {
	TextureComparison_none,
	TextureComparison_less,
};

using ClearFlags = u8;
enum : ClearFlags {
	ClearFlags_none  = 0x0,
	ClearFlags_color = 0x1,
	ClearFlags_depth = 0x2,
};

struct RasterizerState {
	u8 depth_test  : 1;
	u8 depth_write : 1;
	RasterizerState &set_depth_test (bool value) { return depth_test  = value, *this; }
	RasterizerState &set_depth_write(bool value) { return depth_write = value, *this; }
};

#define APIS(A) \
A(void, clear, (RenderTarget *render_target, ClearFlags flags, v4f color, f32 depth), (render_target, flags, color, depth)) \
A(void, present, (), ()) \
A(void, draw, (u32 vertex_count, u32 start_vertex), (vertex_count, start_vertex)) \
A(void, draw_indexed, (u32 index_count), (index_count)) \
A(void, set_viewport, (u32 x, u32 y, u32 w, u32 h), (x, y, w, h)) \
A(void, resize_render_targets, (u32 w, u32 h), (w, h)) \
A(void, set_shader, (Shader *shader), (shader)) \
A(void, set_value, (ShaderConstants *constants, ShaderValueLocation dest, void const *source), (constants, dest, source)) \
A(Shader *, create_shader, (Span<utf8> source), (source)) \
A(CameraMatrices, calculate_perspective_matrices, (v3f position, v3f rotation, f32 aspect_ratio, f32 fov_radians, f32 near_plane, f32 far_plane), (position, rotation, aspect_ratio, fov_radians, near_plane, far_plane)) \
A(VertexBuffer *, create_vertex_buffer, (Span<u8> buffer, Span<ElementType> vertex_descriptor), (buffer, vertex_descriptor)) \
A(void, set_vertex_buffer, (VertexBuffer *buffer), (buffer)) \
A(IndexBuffer *, create_index_buffer, (Span<u8> buffer, u32 index_size), (buffer, index_size)) \
A(void, set_index_buffer, (IndexBuffer *buffer), (buffer)) \
A(void, set_vsync, (bool enable), (enable)) \
A(void, set_render_target, (RenderTarget *target), (target)) \
A(RenderTarget *, create_render_target, (Texture *color, Texture *depth), (color, depth)) \
A(void, set_texture, (Texture *texture, u32 slot), (texture, slot)) \
A(Texture *, create_texture, (CreateTextureFlags flags, u32 width, u32 height, void *data, TextureFormat format, TextureFiltering filtering, TextureComparison comparison), (flags, width, height, data, format, filtering, comparison)) \
A(ShaderConstants *, create_shader_constants, (umm size), (size))\
A(void, set_shader_constants, (ShaderConstants *constants, u32 slot), (constants, slot)) \
A(void, set_rasterizer, (RasterizerState state), (state)) \
A(RasterizerState, get_rasterizer, (), ()) \
A(ComputeShader *, create_compute_shader, (Span<utf8> source), (source)) \
A(void, set_compute_shader, (ComputeShader *shader), (shader)) \
A(void, dispatch_compute_shader, (u32 x, u32 y, u32 z), (x, y, z)) \
A(void, resize_texture, (Texture *texture, u32 w, u32 h), (texture, w, h)) \
A(ComputeBuffer *, create_compute_buffer, (u32 size), (size)) \
A(void, read_compute_buffer, (ComputeBuffer *buffer, void *data), (buffer, data)) \
A(void, set_compute_buffer, (ComputeBuffer *buffer, u32 slot), (buffer, slot)) \
A(void, set_compute_texture, (Texture *texture, u32 slot), (texture, slot)) \
A(void, read_texture, (Texture *texture, Span<u8> data), (texture, data)) \

#define A(ret, name, args, values) extern T3D_API ret (*_##name) args;
APIS(A)
#undef A

#define A(ret, name, args, values) inline ret name args { return _##name values; }
APIS(A)
#undef A

inline void draw(u32 vertex_count) { return _draw(vertex_count, 0); }
inline void set_viewport(u32 w, u32 h) { return _set_viewport(0, 0, w, h); }
inline void set_viewport(v2u size) { return _set_viewport(0, 0, size.x, size.y); }
inline void resize_render_targets(v2u size) { return _resize_render_targets(size.x, size.y); }
template <class T>
inline void set_value(ShaderConstants *constants, ShaderValueLocation dest, T const &source) {
	assert(sizeof(source) == dest.size);
	return _set_value(constants, dest, &source);
}
template <class T>
inline void set_value(ShaderConstants *constants, T const &source) {
	return _set_value(constants, {0, sizeof(source)}, &source);
}
template <class T>
ShaderConstants *create_shader_constants() { return _create_shader_constants(sizeof(T)); }

inline Texture *load_texture(Span<filechar> path) {
	auto file = read_entire_file(path);
	if (!file.data) {
		return 0;
	}

	int width, height;
	void *pixels = stbi_load_from_memory(file.data, file.size, &width, &height, 0, 4);
	return create_texture(CreateTexture_default, width, height, pixels, TextureFormat_rgba_u8n, TextureFiltering_linear, TextureComparison_none);
}

inline void resize_texture(Texture *texture, v2u size) { return _resize_texture(texture, size.x, size.y); }

#ifndef T3D_IMPL
#undef APIS
#endif

bool init(GraphicsApi api, InitInfo init_info);

}
