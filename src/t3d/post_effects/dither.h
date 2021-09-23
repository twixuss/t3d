#pragma once
#include "../post_effect.h"

struct Dither {
	struct Constants {
		f32 time;
		u32 frame_index;
	};

	tg::Shader *shader;
	tg::TypedShaderConstants<Constants> constants;

	void init() {
		constants = shared->tg->create_shader_constants<Dither::Constants>();

		shader = shared->tg->create_shader(u8R"(
#ifdef VERTEX_SHADER
#define V2F out
#else
#define V2F in
#endif
layout (std140, binding=0) uniform _ {
	float time;
	uint frame_index;
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
	float random01 = fract(sin(dot(gl_FragCoord.xy + fract(time), vec2(12.9898, 78.233))) * 43758.5453);
	//float random01 = float((uint(gl_FragCoord.x) ^ uint(gl_FragCoord.y) ^ frame_index) & 1u);

	fragment_color += (random01 - 0.5f) / 256;
	//fragment_color = vec4(random01);
}
#endif
)"s);
	}

	void render(tg::RenderTarget *source, tg::RenderTarget *destination) {
		timed_block("Dither::render"s);
		shared->tg->set_rasterizer(
			shared->tg->get_rasterizer()
				.set_depth_test(false)
				.set_depth_write(false)
		);
		shared->tg->disable_blend();

		shared->tg->set_shader(shader);
		shared->tg->set_shader_constants(constants, 0);

		shared->tg->update_shader_constants(constants, {.time = shared->time, .frame_index = shared->frame_index});

		shared->tg->set_render_target(destination);
		shared->tg->set_sampler(tg::Filtering_nearest, 0);
		shared->tg->set_texture(source->color, 0);
		shared->tg->draw(3);
	}

	void resize(v2u size) {}
	void free() {}
};
