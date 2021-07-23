#pragma once
#include <t3d.h>

FontCollection *font_collection;
t3d::VertexBuffer *text_vertex_buffer;
t3d::Shader *text_shader;
struct TextShaderConstants {
	v2f inv_half_viewport_size;
};
t3d::TypedShaderConstants<TextShaderConstants> text_shader_constants;

void init_font() {
	Span<filechar> font_paths[] = {
		tl_file_string("../data/segoeui.ttf"ts),
	};
	font_collection = create_font_collection(font_paths);
	font_collection->update_atlas = [](TL_FONT_TEXTURE_HANDLE texture, void *data, v2u size) -> TL_FONT_TEXTURE_HANDLE {
		if (texture) {
			t3d::update_texture(texture, size.x, size.y, data);
		} else {
			texture = t3d::create_texture(t3d::CreateTexture_default, size.x, size.y, data, t3d::TextureFormat_rgb_u8n, t3d::TextureFiltering_nearest, t3d::Comparison_none);
		}
		return texture;
	};
	
	text_shader_constants = t3d::create_shader_constants<TextShaderConstants>();
	text_shader = t3d::create_shader(u8R"(
#ifdef VERTEX_SHADER
#define V2F out
#else
#define V2F in
#endif

layout(binding=0,std140) uniform _ {
	vec2 inv_half_viewport_size;
};

layout(binding=0) uniform sampler2D main_texture;

V2F vec2 vertex_uv;

#ifdef VERTEX_SHADER
layout(location=0) in vec2 position;
layout(location=1) in vec2 uv;

void main() {
	vertex_uv = uv;
	gl_Position = vec4(position * inv_half_viewport_size + vec2(-1,1), 0, 1);
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

void draw_text(Span<utf8> string) {
	auto font = get_font_at_size(font_collection, 12);
	ensure_all_chars_present(string, font);
	auto placed_text = with(temporary_allocator, place_text(string, font));

	struct Vertex {
		v2f position;
		v2f uv;
	};

	List<Vertex> vertices;
	vertices.allocator = temporary_allocator;

	for (auto &c : placed_text) {
		Span<Vertex> quad = {
			{{c.position.min.x, c.position.min.y}, {c.uv.min.x, c.uv.min.y}},
			{{c.position.max.x, c.position.min.y}, {c.uv.max.x, c.uv.min.y}},
			{{c.position.max.x, c.position.max.y}, {c.uv.max.x, c.uv.max.y}},
			{{c.position.min.x, c.position.max.y}, {c.uv.min.x, c.uv.max.y}},
		};
		vertices += {
			quad[1], quad[0], quad[2],
			quad[2], quad[0], quad[3],
		};
	}

	if (text_vertex_buffer) {
		t3d::update_vertex_buffer(text_vertex_buffer, as_bytes(vertices));
	} else {
		text_vertex_buffer = t3d::create_vertex_buffer(as_bytes(vertices), {
			t3d::Element_f32x2, // position	
			t3d::Element_f32x2, // uv	
		});
	}
	t3d::set_topology(t3d::Topology_triangle_list);
	t3d::set_blend(t3d::BlendFunction_add, t3d::Blend_secondary_color, t3d::Blend_one_minus_secondary_color);
	t3d::set_shader(text_shader);
	t3d::set_shader_constants(text_shader_constants, 0);
	t3d::update_shader_constants(text_shader_constants, {
		.inv_half_viewport_size = v2f{2,-2} / (v2f)current_viewport.size
	});
	t3d::set_vertex_buffer(text_vertex_buffer);
	t3d::set_texture(font->texture, 0);
	t3d::draw(vertices.size);
}
void draw_text(utf8 const *string) { draw_text(as_span(string)); }
void draw_text(char const *string) { draw_text((Span<utf8>)as_span(string)); }
