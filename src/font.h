#pragma once
#include <t3d/common.h>

FontCollection *font_collection;
tg::VertexBuffer *text_vertex_buffer;
tg::Shader *text_shader;
struct TextShaderConstants {
	v2f inv_half_viewport_size;
	v2f offset;
};
tg::TypedShaderConstants<TextShaderConstants> text_shader_constants;

void init_font() {
	Span<pathchar> font_paths[] = {
		tl_file_string("../data/segoeui.ttf"ts),
	};
	font_collection = create_font_collection(font_paths);
	font_collection->update_atlas = [](TL_FONT_TEXTURE_HANDLE texture, void *data, v2u size) -> TL_FONT_TEXTURE_HANDLE {
		if (texture) {
			tg::update_texture(texture, size, data);
		} else {
			texture = tg::create_texture_2d(size, data, tg::Format_rgb_u8n);
		}
		return texture;
	};

	text_shader_constants = tg::create_shader_constants<TextShaderConstants>();
	text_shader = tg::create_shader(u8R"(
#ifdef VERTEX_SHADER
#define V2F out
#else
#define V2F in
#endif

layout(binding=0,std140) uniform _ {
	vec2 inv_half_viewport_size;
	vec2 text_position;
};

layout(binding=0) uniform sampler2D main_texture;

V2F vec2 vertex_uv;

#ifdef VERTEX_SHADER
layout(location=0) in vec2 position;
layout(location=1) in vec2 uv;

void main() {
	vertex_uv = uv;
	gl_Position = vec4((position + text_position) * inv_half_viewport_size + vec2(-1,1), 0, 1);
}
#endif
#ifdef FRAGMENT_SHADER
layout(location = 0, index = 0) out vec4 fragment_text_color;
layout(location = 0, index = 1) out vec4 fragment_text_mask;

void main() {
	vec4 color = vec4(1);
	fragment_text_color = color;
	fragment_text_mask = color.a * texture(main_texture, vertex_uv);
}

#endif
)"s);
}
