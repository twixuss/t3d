#include "../include/t3d.h"
#include "../dep/tl/include/tl/console.h"
#include "../dep/tl/include/tl/opengl.h"
#include "../dep/tl/include/tl/masked_block_list.h"

namespace t3d::gl {

using namespace OpenGL;

struct ShaderImpl : Shader {
	GLuint program;
	GLuint uniform_buffer;
	void *uniform_data;
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

struct RenderTargetImpl : RenderTarget {
	GLuint frame_buffer;
	GLuint color_attachment;
	GLuint depth_attachment;
};

static MaskedBlockList<ShaderImpl, 256> shaders;
static MaskedBlockList<VertexBufferImpl, 256> vertex_buffers;
static MaskedBlockList<IndexBufferImpl, 256> index_buffers;
static MaskedBlockList<RenderTargetImpl, 256> render_targets;

static IndexBufferImpl *current_index_buffer;

static RenderTargetImpl back_buffer;
static RenderTargetImpl *currently_bound_render_target;

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

void bind_render_target(RenderTargetImpl *render_target) {
	if (render_target == currently_bound_render_target)
		return;

	currently_bound_render_target = render_target;
	glBindFramebuffer(GL_FRAMEBUFFER, render_target->frame_buffer);
}

bool init(InitInfo init_info) {

	if (!init_opengl(init_info.window, init_info.debug)) {
		return false;
	}

	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glEnable(GL_DEPTH_TEST);

	_clear = [](RenderTarget *_render_target, ClearFlags flags, v4f color, f32 depth) {
		auto render_target = (RenderTargetImpl *)_render_target;
		if (!render_target)
			render_target = &back_buffer;

		auto previously_bound_render_target = currently_bound_render_target;
		bind_render_target(render_target);

		GLbitfield mask = 0;
		if (flags & ClearFlags_color) { mask |= GL_COLOR_BUFFER_BIT; glClearColor(color.x, color.y, color.z, color.w); }
		if (flags & ClearFlags_depth) { mask |= GL_DEPTH_BUFFER_BIT; glClearDepth(depth); }
		glClear(mask);

		bind_render_target(previously_bound_render_target);
	};
	_present = []() {
		OpenGL::present();
	};
	_draw = [](u32 vertex_count, u32 start_vertex) {
		glDrawArrays(GL_TRIANGLES, start_vertex, vertex_count);
	};
	_draw_indexed = [](u32 index_count) {
		assert(current_index_buffer, "Index buffer was not bound");
		glDrawElements(GL_TRIANGLES, index_count, current_index_buffer->type, 0);
	};
	_set_viewport = [](u32 x, u32 y, u32 w, u32 h) {
		glViewport(x, y, w, h);
	};
	_resize = [](RenderTarget *render_target, u32 w, u32 h) {
		if (render_target == 0) {
		}
	};
	_set_shader = [](Shader *_shader) {
		auto &shader = *(ShaderImpl *)_shader;
		glUseProgram(shader.program);
		glBindBuffer(GL_UNIFORM_BUFFER, shader.uniform_buffer);
	};
	_set_value = [](Shader *_shader, ShaderValueLocation dest, void const *source) {
		auto &shader = *(ShaderImpl *)_shader;
		glBindBuffer(GL_UNIFORM_BUFFER, shader.uniform_buffer);
		glBufferSubData(GL_UNIFORM_BUFFER, dest.start, dest.size, source);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
	};
	_create_shader = [](Span<utf8> source, umm values_size) -> Shader * {
		auto &shader = shaders.add();
		if (&shader) {
			auto vertex_shader = OpenGL::create_shader(GL_VERTEX_SHADER, 330, true, (Span<char>)source);
			auto fragment_shader = OpenGL::create_shader(GL_FRAGMENT_SHADER, 330, true, (Span<char>)source);
			shader.program = create_program(vertex_shader, fragment_shader);
			{
				glGenBuffers(1, &shader.uniform_buffer);
				glBindBuffer(GL_UNIFORM_BUFFER, shader.uniform_buffer);
				glBufferData(GL_UNIFORM_BUFFER, values_size, NULL, GL_STATIC_DRAW);
				glBindBufferRange(GL_UNIFORM_BUFFER, 0, shader.uniform_buffer, 0, values_size);
				glBindBuffer(GL_UNIFORM_BUFFER, 0);
				shader.uniform_data = default_allocator.allocate(values_size, 16);
			}
		}
		return &shader;
	};
	_calculate_perspective_matrices = [](v3f position, v3f rotation, f32 aspect_ratio, f32 fov, f32 near_plane, f32 far_plane) {
		CameraMatrices result;
		result.mvp = m4::perspective_right_handed(aspect_ratio, fov, near_plane, far_plane)
				   * m4::rotation_r_yxz(-rotation)
			       * m4::translation(-position);
		return result;
	};
	_create_vertex_buffer = [](Span<u8> buffer, Span<ElementType> vertex_descriptor) -> VertexBuffer * {
		VertexBufferImpl &result = vertex_buffers.add();
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
		IndexBufferImpl &result = index_buffers.add();
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
		current_index_buffer = buffer;
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer ? buffer->buffer : 0);
	};

	_set_vsync = [](bool enable) {
		wglSwapIntervalEXT(enable);
	};
	_set_render_target = [](RenderTarget *_render_target) {
		auto render_target = (RenderTargetImpl *)_render_target;
		if (!render_target)
			render_target = &back_buffer;

		bind_render_target(render_target);
	};
	_create_render_target = [](CreateRenderTargetFlags flags, TextureFormat format, u32 width, u32 height) -> RenderTarget * {
		auto &result = render_targets.add();

		glGenTextures(1, &result.depth_attachment);
		glBindTexture(GL_TEXTURE_2D, result.depth_attachment);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
		glBindTexture(GL_TEXTURE_2D, 0);

		glGenFramebuffers(1, &result.frame_buffer);
		glBindFramebuffer(GL_FRAMEBUFFER, result.frame_buffer);
		glDrawBuffer(GL_NONE);
		glReadBuffer(GL_NONE);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, result.depth_attachment, 0);

		assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		return &result;
	};

	return true;
}

}
