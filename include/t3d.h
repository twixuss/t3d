#pragma once
#include <tl/common.h>
#include <tl/vector.h>
#include <tl/file.h>
#include <tl/math.h>
#include <tl/console.h>

#define T3D_API

using namespace tl;

namespace t3d {

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
	TextureFormat_rgb_f32,
	TextureFormat_rgba_u8n,
	TextureFormat_rgba_f16,
	TextureFormat_rgba_f32,
};

enum TextureFiltering : u8 {
	TextureFiltering_none,    // texture will be unsamplable
	TextureFiltering_nearest,
	TextureFiltering_linear,
};

enum Comparison : u8 {
	Comparison_none,
	Comparison_always,
	Comparison_equal,
	Comparison_less,
	Comparison_count,
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
	u8 depth_func  : CE::log2(CE::ceil_to_power_of_2(Comparison_count));
	RasterizerState &set_depth_test (bool       value) { return depth_test  = value, *this; }
	RasterizerState &set_depth_write(bool       value) { return depth_write = value, *this; }
	RasterizerState &set_depth_func (Comparison value) { return depth_func  = value, *this; }
};

enum BlendFunction {
	BlendFunction_disable,
	BlendFunction_add,
};

enum Blend {
	Blend_null,
	Blend_one,
	Blend_source_alpha,
	Blend_one_minus_source_alpha,
};

enum Topology {
	Topology_triangle_list,
	Topology_line_list,
};

union CubeTexturePaths {
	struct {
		Span<filechar> left;
		Span<filechar> right;
		Span<filechar> top;
		Span<filechar> bottom;
		Span<filechar> front;
		Span<filechar> back;
	};
	Span<filechar> paths[6];
};

// position is bottom left
union Viewport {
	struct {
		s32 x, y;
		u32 w, h;
	};
	struct {
		v2s position;
		v2u size;
	};
	v2s top_left() const {
		return { x, y + (s32)h };
	}
	v2s bottom_right() const {
		return { x + (s32)w, y };
	}
};

#define APIS(A) \
A(void, clear, (RenderTarget *render_target, ClearFlags flags, v4f color, f32 depth), (render_target, flags, color, depth)) \
A(void, present, (), ()) \
A(void, draw, (u32 vertex_count, u32 start_vertex), (vertex_count, start_vertex)) \
A(void, draw_indexed, (u32 index_count), (index_count)) \
A(void, set_viewport, (Viewport viewport), (viewport)) \
A(void, resize_render_targets, (u32 w, u32 h), (w, h)) \
A(void, set_shader, (Shader *shader), (shader)) \
A(void, update_shader_constants, (ShaderConstants *constants, ShaderValueLocation dest, void const *source), (constants, dest, source)) \
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
A(Texture *, create_texture, (CreateTextureFlags flags, u32 width, u32 height, void *data, TextureFormat format, TextureFiltering filtering, Comparison comparison), (flags, width, height, data, format, filtering, comparison)) \
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
A(void, set_blend, (BlendFunction function, Blend source, Blend destination), (function, source, destination)) \
A(Texture *, create_cube_texture, (CreateTextureFlags flags, u32 width, u32 height, void *data[6], TextureFormat format, TextureFiltering filtering, Comparison comparison), (flags, width, height, data, format, filtering, comparison)) \
A(void, set_topology, (Topology topology), (topology)) \
A(void, update_vertex_buffer, (VertexBuffer *buffer, Span<u8> data), (buffer, data)) \

#define A(ret, name, args, values) extern T3D_API ret (*_##name) args;
APIS(A)
#undef A

#define A(ret, name, args, values) inline ret name args { return _##name values; }
APIS(A)
#undef A

extern T3D_API RenderTarget *back_buffer;
extern T3D_API v2u min_texture_size;

inline void draw(u32 vertex_count) { return _draw(vertex_count, 0); }
inline void set_viewport(u32 w, u32 h) { return _set_viewport({.position = {}, .size = {w, h}}); }
inline void set_viewport(v2u size) { return _set_viewport({.position={}, .size=size}); }
inline void resize_render_targets(v2u size) { return _resize_render_targets(size.x, size.y); }
template <class T>
inline void update_shader_constants(ShaderConstants *constants, ShaderValueLocation dest, T const &source) {
	assert(sizeof(source) == dest.size);
	return _update_shader_constants(constants, dest, &source);
}
template <class T>
inline void update_shader_constants(ShaderConstants *constants, T const &source) {
	return _update_shader_constants(constants, {0, sizeof(source)}, &source);
}

struct Pixels {
	void *data;
	v2u size;
	TextureFormat format;
	void (*free)(void *data);
};

T3D_API Pixels load_pixels(Span<filechar> path);

inline Texture *load_texture(Span<filechar> path) {
	auto pixels = load_pixels(path);
	if (!pixels.data) {
		return 0;
	}
	defer { pixels.free(pixels.data); };
	return _create_texture(CreateTexture_default, pixels.size.x, pixels.size.y, pixels.data, pixels.format, TextureFiltering_linear, Comparison_none);
}

inline Texture *load_texture(CubeTexturePaths paths) {
	Pixels pixels[6];
	void *datas[6];
	for (u32 i = 0; i < 6; ++i) {
		pixels[i] = load_pixels(paths.paths[i]);
		if (!pixels[i].data) {
			return 0;
		}
		if (i != 0) {
			bool fail = false;
			Span<char> reason;
			if (any_true(pixels[i].size != pixels[0].size)) {
				fail = true;
				reason = "sizes of faces do not match"s;
			}
			if (pixels[i].format != pixels[0].format) {
				fail = true;
				reason = "formats of faces do not match"s;
			}
			if (fail) {
				print(Print_error, "Failed to load cube texture (%) with these paths:\n\t%\n\t%\n\t%\n\t%\n\t%\n\t%\n"
					, reason
					, paths.paths[0]
					, paths.paths[1]
					, paths.paths[2]
					, paths.paths[3]
					, paths.paths[4]
					, paths.paths[5]
				);
				return 0;
			}
		}
		datas[i] = pixels[i].data;
	}
	defer {
		for (u32 i = 0; i < 6; ++i) {
			pixels[i].free(pixels[i].data);
		}
	};
	return _create_cube_texture(CreateTexture_default, pixels[0].size.x, pixels[0].size.y, datas, pixels[0].format, TextureFiltering_linear, Comparison_none);
}

inline void resize_texture(Texture *texture, v2u size) { return _resize_texture(texture, size.x, size.y); }

template <class T>
struct TypedShaderConstants {
	ShaderConstants *constants;
};

template <class T>
TypedShaderConstants<T> create_shader_constants() {
	TypedShaderConstants<T> result = {
		.constants = _create_shader_constants(sizeof(T)),
	};
	return result;
}

template <class T>
inline void update_shader_constants(TypedShaderConstants<T> &constants, T const &value) {
	return _update_shader_constants(constants.constants, {0, sizeof(T)}, &value);
}

template <class T, class U>
inline void update_shader_constants(TypedShaderConstants<T> &constants, ShaderValueLocation dest, U const &source) {
	assert(sizeof(source) == dest.size);
	return _update_shader_constants(constants.constants, dest, &source);
}

template <class T>
void set_shader_constants(TypedShaderConstants<T> const &constants, u32 slot) {
	return _set_shader_constants(constants.constants, slot);
}

#ifndef T3D_IMPL
#undef APIS
#endif

bool init(GraphicsApi api, InitInfo init_info);
void free();

}
