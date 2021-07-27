#include "../include/t3d.h"
#include "../dep/tl/include/tl/console.h"
#include "../dep/tl/include/tl/opengl.h"
#include "../dep/tl/include/tl/masked_block_list.h"
#include "../dep/tl/include/tl/hash_map.h"

namespace t3d::gl {

using namespace tl::gl;

struct ShaderImpl : Shader {
	GLuint program;
};

struct ShaderConstantsImpl : ShaderConstants {
	GLuint uniform_buffer;
	u32 values_size;
};

struct VertexBufferImpl : VertexBuffer {
	GLuint buffer;
	GLuint array;
};

struct IndexBufferImpl : IndexBuffer {
	GLuint buffer;
	GLuint type;
	u32 count;
};

struct TextureImpl : Texture {
	GLuint texture;
	GLuint sampler;
	GLuint format;
	GLuint internal_format;
	GLuint type;
	GLuint target;
	u32 bytes_per_texel;
};

struct RenderTargetImpl : RenderTarget {
	GLuint frame_buffer;
};

struct ComputeShaderImpl : ComputeShader {
	GLuint program;
};
struct ComputeBufferImpl : ComputeBuffer {
	GLuint buffer;
	u32 size;
};

struct SamplerKey {
	TextureFiltering filtering;
	Comparison comparison;
	bool operator==(SamplerKey const &that) const {
		return filtering == that.filtering && comparison == that.comparison;
	}
};

umm get_hash(SamplerKey key) {
	return key.filtering ^ key.comparison;
}

struct State {
	MaskedBlockList<ShaderImpl, 256> shaders;
	MaskedBlockList<VertexBufferImpl, 256> vertex_buffers;
	MaskedBlockList<IndexBufferImpl, 256> index_buffers;
	MaskedBlockList<RenderTargetImpl, 256> render_targets;
	MaskedBlockList<TextureImpl, 256> textures;
	MaskedBlockList<ShaderConstantsImpl, 256> shader_constants;
	MaskedBlockList<ComputeShaderImpl, 256> compute_shaders;
	MaskedBlockList<ComputeBufferImpl, 256> compute_buffers;
	IndexBufferImpl *current_index_buffer;
	RenderTargetImpl back_buffer;
	TextureImpl back_buffer_color;
	TextureImpl back_buffer_depth;
	RenderTargetImpl *currently_bound_render_target;
	StaticHashMap<SamplerKey, GLuint, 256> samplers;
	v2u window_size;
	List<TextureImpl *> window_sized_textures;
	RasterizerState rasterizer;
	BlendFunction blend_function;
	Blend blend_source;
	Blend blend_destination;
	GLuint topology = GL_TRIANGLES;
};
static State state;

u32 get_element_scalar_count(ElementType element) {
	switch (element) {
		case Element_f32x1: return 1;
		case Element_f32x2:	return 2;
		case Element_f32x3:	return 3;
		case Element_f32x4:	return 4;
	}
	invalid_code_path();
	return 0;
}

u32 get_element_size(ElementType element) {
	switch (element) {
		case Element_f32x1: return 4;
		case Element_f32x2:	return 8;
		case Element_f32x3:	return 12;
		case Element_f32x4:	return 16;
	}
	invalid_code_path();
	return 0;
}

u32 get_element_type(ElementType element) {
	switch (element) {
		case Element_f32x1: return GL_FLOAT;
		case Element_f32x2:	return GL_FLOAT;
		case Element_f32x3:	return GL_FLOAT;
		case Element_f32x4:	return GL_FLOAT;
	}
	invalid_code_path();
	return 0;
}

u32 get_index_type_from_size(u32 size) {
	switch (size) {
		case 2: return GL_UNSIGNED_SHORT;
		case 4: return GL_UNSIGNED_INT;
	}
	invalid_code_path();
	return 0;
}

GLuint get_min_filter(TextureFiltering filter) {
	switch (filter) {
		case TextureFiltering_nearest:        return GL_NEAREST;
		case TextureFiltering_linear:         return GL_LINEAR;
		case TextureFiltering_linear_mipmap:  return GL_LINEAR_MIPMAP_LINEAR;
	}
	invalid_code_path();
	return 0;
}
GLuint get_mag_filter(TextureFiltering filter) {
	switch (filter) {
		case TextureFiltering_nearest:        return GL_NEAREST;
		case TextureFiltering_linear:         return GL_LINEAR;
		case TextureFiltering_linear_mipmap:  return GL_LINEAR;
	}
	invalid_code_path();
	return 0;
}
GLuint get_func(Comparison comparison) {
	switch (comparison) {
		case Comparison_none:   return GL_NONE;
		case Comparison_always: return GL_ALWAYS;
		case Comparison_equal:  return GL_EQUAL;
		case Comparison_less:   return GL_LESS;
	}
	invalid_code_path();
	return 0;
}

GLuint get_format(TextureFormat format) {
	switch (format) {
		case TextureFormat_depth:    return GL_DEPTH_COMPONENT;
		case TextureFormat_r_f32:    return GL_RED;
		case TextureFormat_rgb_u8n:  return GL_RGB;
		case TextureFormat_rgb_f16:  return GL_RGB;
		case TextureFormat_rgb_f32:  return GL_RGB;
		case TextureFormat_rgba_u8n: return GL_RGBA;
		case TextureFormat_rgba_f16: return GL_RGBA;
		case TextureFormat_rgba_f32: return GL_RGBA;
	}
	invalid_code_path();
	return 0;
}

GLuint get_internal_format(TextureFormat format) {
	switch (format) {
		case TextureFormat_depth:    return GL_DEPTH_COMPONENT;
		case TextureFormat_r_f32:    return GL_R32F;
		case TextureFormat_rgb_u8n:  return GL_RGB8;
		case TextureFormat_rgb_f16:  return GL_RGB16F;
		case TextureFormat_rgb_f32:  return GL_RGB32F;
		case TextureFormat_rgba_u8n: return GL_RGBA8;
		case TextureFormat_rgba_f16: return GL_RGBA16F;
		case TextureFormat_rgba_f32: return GL_RGBA32F;
	}
	invalid_code_path();
	return 0;
}

GLuint get_type(TextureFormat format) {
	switch (format) {
		case TextureFormat_depth:    return GL_FLOAT;
		case TextureFormat_r_f32:    return GL_FLOAT;
		case TextureFormat_rgb_u8n:  return GL_UNSIGNED_BYTE;
		case TextureFormat_rgb_f16:  return GL_FLOAT;
		case TextureFormat_rgb_f32:  return GL_FLOAT;
		case TextureFormat_rgba_u8n: return GL_UNSIGNED_BYTE;
		case TextureFormat_rgba_f16: return GL_FLOAT;
		case TextureFormat_rgba_f32: return GL_FLOAT;
	}
	invalid_code_path();
	return 0;
}
u32 get_bytes_per_texel(TextureFormat format) {
	switch (format) {
		case TextureFormat_depth:    return 4;
		case TextureFormat_r_f32:    return 4;
		case TextureFormat_rgb_u8n:  return 3;
		case TextureFormat_rgb_f16:  return 6;
		case TextureFormat_rgb_f32:  return 12;
		case TextureFormat_rgba_u8n: return 4;
		case TextureFormat_rgba_f16: return 8;
		case TextureFormat_rgba_f32: return 16;
	}
	invalid_code_path();
	return 0;
}

GLuint get_blend(Blend blend) {
	switch (blend) {
		case t3d::Blend_one:                       return GL_ONE;
		case t3d::Blend_source_alpha:              return GL_SRC_ALPHA;
		case t3d::Blend_one_minus_source_alpha:    return GL_ONE_MINUS_SRC_ALPHA;
		case t3d::Blend_secondary_color:           return GL_SRC1_COLOR;
		case t3d::Blend_one_minus_secondary_color: return GL_ONE_MINUS_SRC1_COLOR;
	}
	invalid_code_path();
	return 0;
}

GLuint get_equation(BlendFunction function) {
	switch (function) {
		case t3d::BlendFunction_add: return GL_FUNC_ADD;
	}
	invalid_code_path();
	return 0;
}

GLuint get_topology(Topology topology) {
	switch (topology) {
		case Topology_triangle_list: return GL_TRIANGLES;
		case Topology_line_list:     return GL_LINES;
	}
	invalid_code_path();
	return 0;
}

void bind_render_target(RenderTargetImpl &render_target) {
	if (&render_target == state.currently_bound_render_target)
		return;

	state.currently_bound_render_target = &render_target;
	glBindFramebuffer(GL_FRAMEBUFFER, render_target.frame_buffer);
}

GLuint get_sampler(TextureFiltering filtering, Comparison comparison) {
	if (filtering == TextureFiltering_none)
		return 0;

	auto &result = state.samplers.get_or_insert({filtering, comparison});
	if (!result) {
		glGenSamplers(1, &result);
		if (comparison != Comparison_none) {
			glSamplerParameteri(result, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
			auto func = get_func(comparison);
			glSamplerParameteri(result, GL_TEXTURE_COMPARE_FUNC, func);
		}

		glSamplerParameteri(result, GL_TEXTURE_MIN_FILTER, get_min_filter(filtering));
		glSamplerParameteri(result, GL_TEXTURE_MAG_FILTER, get_mag_filter(filtering));
		glSamplerParameteri(result, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glSamplerParameteri(result, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}
	return result;
}

void resize_texture_gl(Texture *_texture, u32 width, u32 height) {
	auto &texture = *(TextureImpl *)_texture;
	texture.size = {width, height};
	glBindTexture(texture.target, texture.texture);
	glTexImage2D(texture.target, 0, texture.internal_format, width, height, 0, texture.format, texture.type, NULL);
	glBindTexture(texture.target, 0);
}

bool init(InitInfo init_info) {

	if (!init_opengl(init_info.window, init_info.debug)) {
		return false;
	}

	new (&state) State();

	state.window_size = init_info.window_size;

	back_buffer = &state.back_buffer;
	state.back_buffer.color = &state.back_buffer_color;
	state.back_buffer.depth = &state.back_buffer_depth;

	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);

	_clear = [](RenderTarget *_render_target, ClearFlags flags, v4f color, f32 depth) {
		assert(_render_target);
		auto &render_target = *(RenderTargetImpl *)_render_target;

		auto previously_bound_render_target = state.currently_bound_render_target;
		bind_render_target(render_target);

		GLbitfield mask = 0;
		if (flags & ClearFlags_color) { mask |= GL_COLOR_BUFFER_BIT; glClearColor(color.x, color.y, color.z, color.w); }
		if (flags & ClearFlags_depth) { mask |= GL_DEPTH_BUFFER_BIT; glClearDepth(depth); }
		glClear(mask);

		if (previously_bound_render_target) {
			bind_render_target(*previously_bound_render_target);
		}
	};
	_present = []() {
		gl::present();
	};
	_draw = [](u32 vertex_count, u32 start_vertex) {
		assert(vertex_count, "t3d::draw called with 0 vertices");
		glDrawArrays(state.topology, start_vertex, vertex_count);
	};
	_draw_indexed = [](u32 index_count) {
		assert(state.current_index_buffer, "Index buffer was not bound");
		glDrawElements(state.topology, index_count, state.current_index_buffer->type, 0);
	};
	_set_viewport = [](Viewport viewport) {
		glViewport(viewport.x, viewport.y, viewport.w, viewport.h);
	};
	_resize_render_targets = [](u32 width, u32 height) {
		for (auto texture : state.window_sized_textures) {
			if (texture) {
				resize_texture_gl(texture, width, height);
			}
		}
		state.window_size = state.back_buffer_color.size = state.back_buffer_depth.size = {width, height};
	};
	_set_shader = [](Shader *_shader) {
		assert(_shader);
		auto &shader = *(ShaderImpl *)_shader;
		glUseProgram(shader.program);
	};
	_set_shader_constants = [](ShaderConstants *_constants, u32 slot) {
		assert(_constants);
		auto &constants = *(ShaderConstantsImpl *)_constants;
		glBindBuffer(GL_UNIFORM_BUFFER, constants.uniform_buffer);
		glBindBufferBase(GL_UNIFORM_BUFFER, slot, constants.uniform_buffer);
	};
	_update_shader_constants = [](ShaderConstants *_constants, ShaderValueLocation dest, void const *source) {
		assert(_constants);
		auto &constants = *(ShaderConstantsImpl *)_constants;
		glBindBuffer(GL_UNIFORM_BUFFER, constants.uniform_buffer);
		glBufferSubData(GL_UNIFORM_BUFFER, dest.start, dest.size, source);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
	};
	_create_shader = [](Span<utf8> source) -> Shader * {
		auto &shader = state.shaders.add();
		auto vertex   = tl::gl::create_shader(GL_VERTEX_SHADER, 430, true, (Span<char>)source);
		auto fragment = tl::gl::create_shader(GL_FRAGMENT_SHADER, 430, true, (Span<char>)source);
		assert(vertex);
		assert(fragment);
		shader.program = create_program({
			.vertex = vertex,
			.fragment = fragment,
		});
		assert(shader.program);
		return &shader;
	};
	_create_shader_constants = [](umm size) -> ShaderConstants * {
		auto &constants = state.shader_constants.add();
		glGenBuffers(1, &constants.uniform_buffer);
		glBindBuffer(GL_UNIFORM_BUFFER, constants.uniform_buffer);
		glBufferData(GL_UNIFORM_BUFFER, size, NULL, GL_STATIC_DRAW);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
		constants.values_size = size;
		return &constants;
	};
	_calculate_perspective_matrices = [](v3f position, v3f rotation, f32 aspect_ratio, f32 fov, f32 near_plane, f32 far_plane) {
		CameraMatrices result;
		result.mvp = m4::perspective_right_handed(aspect_ratio, fov, near_plane, far_plane)
				   * m4::rotation_r_yxz(-rotation)
			       * m4::translation(-position);
		return result;
	};
	_create_vertex_buffer = [](Span<u8> buffer, Span<ElementType> vertex_descriptor) -> VertexBuffer * {
		VertexBufferImpl &result = state.vertex_buffers.add();
		glGenBuffers(1, &result.buffer);
		glGenVertexArrays(1, &result.array);

		glBindVertexArray(result.array);

		glBindBuffer(GL_ARRAY_BUFFER, result.buffer);
		glBufferData(GL_ARRAY_BUFFER, buffer.size, buffer.data, GL_STATIC_DRAW);

		u32 stride = 0;
		for (auto &element : vertex_descriptor) {
			stride += get_element_size(element);
		}

		u32 offset = 0;
		for (u32 element_index = 0; element_index < vertex_descriptor.size; ++element_index) {
			auto &element = vertex_descriptor[element_index];
			glVertexAttribPointer(element_index, get_element_scalar_count(element), get_element_type(element), false, stride, (void const *)offset);
			glEnableVertexAttribArray(element_index);
			offset += get_element_size(element);
		}

		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		return &result;
	};

	_set_vertex_buffer = [](VertexBuffer *_buffer) {
		auto buffer = (VertexBufferImpl *)_buffer;
		glBindVertexArray(buffer ? buffer->array : 0);
	};

	_create_index_buffer = [](Span<u8> buffer, u32 index_size) -> IndexBuffer * {
		IndexBufferImpl &result = state.index_buffers.add();
		result.type = get_index_type_from_size(index_size);
		result.count = buffer.size / index_size;

		glGenBuffers(1, &result.buffer);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, result.buffer);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, buffer.size, buffer.data, GL_STATIC_DRAW);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

		return &result;
	};

	_set_index_buffer = [](IndexBuffer *_buffer) {
		auto buffer = (IndexBufferImpl *)_buffer;
		state.current_index_buffer = buffer;
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer ? buffer->buffer : 0);
	};

	_set_vsync = [](bool enable) {
		wglSwapIntervalEXT(enable);
	};
	_set_render_target = [](RenderTarget *_render_target) {
		assert(_render_target);
		auto &render_target = *(RenderTargetImpl *)_render_target;
		bind_render_target(render_target);
	};
	_create_render_target = [](Texture *_color, Texture *_depth) -> RenderTarget * {
		assert(_color || _depth);
		auto color = (TextureImpl *)_color;
		auto depth = (TextureImpl *)_depth;

		auto &result = state.render_targets.add();

		result.color = color;
		result.depth = depth;

		glGenFramebuffers(1, &result.frame_buffer);
		glBindFramebuffer(GL_FRAMEBUFFER, result.frame_buffer);
		if (depth) {
			if (!color) {
				glDrawBuffer(GL_NONE);
				glReadBuffer(GL_NONE);
			}
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth->texture, 0);
		}
		if (color) {
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color->texture, 0);
		}

		switch(glCheckFramebufferStatus(GL_FRAMEBUFFER)) {
#define C(x) case x: print(#x "\n"); invalid_code_path(); break;
			C(GL_FRAMEBUFFER_UNDEFINED)
			C(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT)
			C(GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT)
			C(GL_FRAMEBUFFER_UNSUPPORTED)
			C(GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE)
#undef C
			case GL_FRAMEBUFFER_COMPLETE: break;
		}
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		return &result;
	};
	_set_texture = [](Texture *_texture, u32 slot) {
		auto &texture = *(TextureImpl *)_texture;
		glActiveTexture(GL_TEXTURE0 + slot);
		if (_texture) {
			glBindTexture(texture.target, texture.texture);
			glBindSampler(slot, texture.sampler);
		} else {
			glBindTexture(texture.target, 0);
			glBindSampler(slot, 0);
		}
	};
	_create_texture = [](CreateTextureFlags flags, u32 width, u32 height, void *data, TextureFormat format, TextureFiltering filtering, Comparison comparison) -> Texture * {
		auto &result = state.textures.add();

		result.size = {width, height};

		if (flags & CreateTexture_resize_with_window) {
			state.window_sized_textures.add(&result);
			width  = state.window_size.x;
			height = state.window_size.y;
		}

		result.internal_format = get_internal_format(format);
		result.format          = get_format(format);
		result.type            = get_type(format);
		result.bytes_per_texel = get_bytes_per_texel(format);
		result.target = GL_TEXTURE_2D;

		glGenTextures(1, &result.texture);
		glBindTexture(GL_TEXTURE_2D, result.texture);
		glTexImage2D(GL_TEXTURE_2D, 0, result.internal_format, width, height, 0, result.format, result.type, data);
		glBindTexture(GL_TEXTURE_2D, 0);

		result.sampler = get_sampler(filtering, comparison);

		return &result;
	};
	_set_rasterizer = [](RasterizerState rasterizer) {
		if (rasterizer.depth_test != state.rasterizer.depth_test) {
			if (rasterizer.depth_test) {
				glEnable(GL_DEPTH_TEST);
			} else {
				glDisable(GL_DEPTH_TEST);
			}
		}

		if (rasterizer.depth_test) {
			if (rasterizer.depth_func != state.rasterizer.depth_func) {
				glDepthFunc(get_func((Comparison)rasterizer.depth_func));
			}
		}

		//if (rasterizer.depth_write != state.rasterizer.depth_write) {
		//	glDepthMask(rasterizer.depth_write);
		//}

		state.rasterizer = rasterizer;
	};
	_get_rasterizer = []() -> RasterizerState {
		return state.rasterizer;
	};
	_create_compute_shader = [](Span<utf8> source) -> ComputeShader * {
		auto &result = state.compute_shaders.add();
		result.program = create_program({
			.compute = tl::gl::create_shader(GL_COMPUTE_SHADER, 430, true, (Span<char>)source),
		});
		return &result;
	};
	_set_compute_shader = [](ComputeShader *_shader) {
		assert(_shader);
		auto &shader = *(ComputeShaderImpl *)_shader;
		glUseProgram(shader.program);
	};
	_dispatch_compute_shader = [](u32 x, u32 y, u32 z) {
		glDispatchCompute(x, y, z);
	};
	_resize_texture = resize_texture_gl;
	_create_compute_buffer = [](u32 size) -> ComputeBuffer * {
		auto &result = state.compute_buffers.add();
		result.size = size;
		glGenBuffers(1, &result.buffer);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, result.buffer);
		glBufferData(GL_SHADER_STORAGE_BUFFER, size, 0, GL_STATIC_COPY);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, result.buffer);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

		return &result;
	};
	_set_compute_buffer =  [](ComputeBuffer *_buffer, u32 slot) {
		assert(_buffer);
		auto &buffer = *(ComputeBufferImpl *)_buffer;
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer.buffer);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, buffer.buffer);
	};
	_read_compute_buffer = [](ComputeBuffer *_buffer, void *data) {
		assert(_buffer);
		auto &buffer = *(ComputeBufferImpl *)_buffer;

		//glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);


		glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer.buffer);
		void* resultData = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, buffer.size, GL_MAP_READ_BIT);
		memcpy(data, resultData, buffer.size);
		glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
	};
	_set_compute_texture = [](Texture *_texture, u32 slot) {
		assert(_texture);
		auto &texture = *(TextureImpl *)_texture;
		glBindImageTexture(slot, texture.texture, 0, GL_FALSE, 0, GL_READ_ONLY, texture.internal_format);
	};
	_read_texture = [](Texture *_texture, Span<u8> data) {
		assert(_texture);
		auto &texture = *(TextureImpl *)_texture;
		glGetTextureImage(texture.texture, 0, texture.format, texture.type, data.size, data.data);
	};
	_set_blend = [](BlendFunction function, Blend source, Blend destination) {
		if (function != state.blend_function) {
			state.blend_function = function;

			if (function == BlendFunction_disable) {
				glDisable(GL_BLEND);
			} else {
				glEnable(GL_BLEND);
				glBlendEquation(get_equation(function));
			}
		}
		if (source != state.blend_source || destination != state.blend_destination) {
			state.blend_source = source;
			state.blend_destination = destination;
			if (function != BlendFunction_disable) {
				glBlendFunc(get_blend(source), get_blend(destination));
			}
		}
	};
	_create_cube_texture = [](CreateTextureFlags flags, u32 width, u32 height, void *data[6], TextureFormat format, TextureFiltering filtering, Comparison comparison) -> Texture * {
		auto &result = state.textures.add();

		result.size = {width, height};

		if (flags & CreateTexture_resize_with_window) {
			state.window_sized_textures.add(&result);
			width  = state.window_size.x;
			height = state.window_size.y;
		}

		result.internal_format = get_internal_format(format);
		result.format          = get_format(format);
		result.type            = get_type(format);
		result.bytes_per_texel = get_bytes_per_texel(format);
		result.target          = GL_TEXTURE_CUBE_MAP;

		glGenTextures(1, &result.texture);
		glBindTexture(GL_TEXTURE_CUBE_MAP, result.texture);
		for (u32 i = 0; i < 6; ++i) {
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, result.internal_format, width, height, 0, result.format, result.type, data[i]);
		}
		glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

		result.sampler = get_sampler(filtering, comparison);

		return &result;
	};
	_set_topology = [](Topology topology) {
		state.topology = get_topology(topology);
	};
	_update_vertex_buffer = [](VertexBuffer *_buffer, Span<u8> data) {
		auto &buffer = *(VertexBufferImpl *)_buffer;
		glBindBuffer(GL_ARRAY_BUFFER, buffer.buffer);
		glBufferData(GL_ARRAY_BUFFER, data.size, data.data, GL_STATIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	};
	_update_texture = [](Texture *_texture, u32 width, u32 height, void *data) {
		auto &texture = *(TextureImpl *)_texture;
		glBindTexture(texture.target, texture.texture);
		glTexImage2D(texture.target, 0, texture.internal_format, width, height, 0, texture.format, texture.type, data);
		glBindTexture(texture.target, 0);
	};
	_generate_mipmaps = [](Texture *_texture) {
		assert(_texture);
		auto &texture = *(TextureImpl *)_texture;
		glBindTexture(GL_TEXTURE_2D, texture.texture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glGenerateMipmap(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, 0);
	};
	return true;
}

void free() {
	/*
	MaskedBlockList<ShaderImpl, 256> shaders;
	MaskedBlockList<VertexBufferImpl, 256> vertex_buffers;
	MaskedBlockList<IndexBufferImpl, 256> index_buffers;
	MaskedBlockList<RenderTargetImpl, 256> render_targets;
	MaskedBlockList<TextureImpl, 256> textures;
	MaskedBlockList<ShaderConstantsImpl, 256> shader_constants;
	MaskedBlockList<ComputeShaderImpl, 256> compute_shaders;
	MaskedBlockList<ComputeBufferImpl, 256> compute_buffers;
	IndexBufferImpl *current_index_buffer;
	RenderTargetImpl back_buffer;
	TextureImpl back_buffer_color;
	TextureImpl back_buffer_depth;
	RenderTargetImpl *currently_bound_render_target;
	StaticHashMap<SamplerKey, GLuint, 256> samplers;
	v2u window_size;
	List<TextureImpl *> window_sized_textures;
	RasterizerState rasterizer;
	BlendFunction blend_function;
	Blend blend_source;
	Blend blend_destination;
	*/
	free(state.shaders);
	free(state.vertex_buffers);
	free(state.index_buffers);
	free(state.render_targets);
	free(state.textures);
	free(state.shader_constants);
	free(state.compute_shaders);
	free(state.compute_buffers);
	free(state.samplers);
	free(state.window_sized_textures);
}

}
