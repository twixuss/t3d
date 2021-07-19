#pragma once
#include <t3d.h>

struct Dither {
	struct Constants {
		f32 time;
	};

	t3d::Shader *shader;
	t3d::TypedShaderConstants<Constants> constants;

	void init() {
		constants = t3d::create_shader_constants<Dither::Constants>();

		shader = t3d::create_shader(u8R"(
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

	void render(t3d::RenderTarget *source, t3d::RenderTarget *destination) {
		timed_block("Dither::render"s);
		t3d::set_rasterizer(
			t3d::get_rasterizer()
				.set_depth_test(false)
				.set_depth_write(false)
		);
		t3d::set_blend(t3d::BlendFunction_disable, {}, {});

		t3d::set_shader(shader);
		t3d::set_shader_constants(constants, 0);

		t3d::update_shader_constants(constants, {.time = time});

		t3d::set_render_target(destination);
		t3d::set_texture(source->color, 0);
		t3d::draw(3);
	}

	void resize(v2u size) {}
	void free() {}
};
