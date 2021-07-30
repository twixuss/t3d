#pragma once
#include <t3d.h>

struct Bloom {
	struct Constants {
		v2f texel_size;
		f32 threshold;
	};

	t3d::Shader *downsample_shader;
	t3d::Shader *downsample_filter_shader;
	t3d::Shader *blur_x_shader;
	t3d::Shader *blur_y_shader;
	t3d::TypedShaderConstants<Constants> constants;

	struct TempRenderTarget {
		t3d::RenderTarget *source;
		t3d::RenderTarget *destination;
	};
	List<TempRenderTarget> temp_targets;
	f32 threshold = 1;
	f32 intensity = 0.2f;

	void init() {
		constants = t3d::create_shader_constants<Bloom::Constants>();

		constexpr auto header = u8R"(
#ifdef VERTEX_SHADER
#define V2F out
#else
#define V2F in
#endif

layout (std140, binding=0) uniform _ {
	vec2 texel_size;
	float threshold;
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

vec4 get_sample(vec2 vertex_uv, float offset) {
	// box filter reduces flickering for small and bright areas
	return (texture(main_texture, vertex_uv + texel_size * vec2( offset, offset))
		  + texture(main_texture, vertex_uv + texel_size * vec2(-offset, offset))
		  + texture(main_texture, vertex_uv + texel_size * vec2( offset,-offset))
		  + texture(main_texture, vertex_uv + texel_size * vec2(-offset,-offset))) * 0.25f;
}

vec4 apply_filter(vec4 c) {
	float brightness = max(c.r, max(c.g, c.b));
	float contribution = max(0, brightness - threshold);
	contribution /= max(brightness, 0.00001);
	return c * contribution;
}

#define KERNEL_RADIUS 8

float gauss(float x) {
	float c = 3;
	return exp(-x*x/(2*c*c));
}

vec4 blurred_sample(vec2 vertex_uv) {
	float kernel[] = float[](
		gauss(8),
		gauss(7),
		gauss(6),
		gauss(5),
		gauss(4),
		gauss(3),
		gauss(2),
		gauss(1),
		gauss(0),
		gauss(1),
		gauss(2),
		gauss(3),
		gauss(4),
		gauss(5),
		gauss(6),
		gauss(7),
		gauss(8)
	);

	vec4 color = vec4(0);
	float denom = 0;
	for (int i = -KERNEL_RADIUS; i <= KERNEL_RADIUS; ++i) {
		float mask = kernel[i + KERNEL_RADIUS];
#ifdef BLUR_X
		ivec2 offset = ivec2(0, i);
#else
		ivec2 offset = ivec2(i, 0);
#endif
		color += textureOffset(main_texture, vertex_uv, offset) * mask;
		denom += mask;
	}
	return color / denom;
}

)"s;
		downsample_shader = t3d::create_shader(tconcatenate(header, u8R"(
#ifdef FRAGMENT_SHADER
out vec4 fragment_color;
void main() {
	fragment_color = get_sample(vertex_uv, 1);
}
#endif
)"s));
		downsample_filter_shader = t3d::create_shader(tconcatenate(header, u8R"(
#ifdef FRAGMENT_SHADER
out vec4 fragment_color;
void main() {
	fragment_color = apply_filter(get_sample(vertex_uv, 1));
}
#endif
)"s));
		blur_x_shader = t3d::create_shader(tconcatenate(u8"#define BLUR_X\n"s, header, u8R"(
#ifdef FRAGMENT_SHADER
out vec4 fragment_color;
void main() {
	fragment_color = blurred_sample(vertex_uv);
}
#endif
)"s));
		blur_y_shader = t3d::create_shader(tconcatenate(header, u8R"(
#ifdef FRAGMENT_SHADER
out vec4 fragment_color;
void main() {
	fragment_color = blurred_sample(vertex_uv);
}
#endif
)"s));
	}

	void render(t3d::RenderTarget *source, t3d::RenderTarget *destination) {
		timed_block("Bloom::render"s);

		{
			timed_block("Downsample"s);
			t3d::set_rasterizer(
				t3d::get_rasterizer()
					.set_depth_test(false)
					.set_depth_write(false)
			);

			t3d::set_shader(downsample_filter_shader);
			t3d::set_shader_constants(constants, 0);

			auto sample_from = source;
			for (auto &target : temp_targets) {
				t3d::set_render_target(target.destination);
				t3d::set_viewport(target.destination->color->size);
				t3d::set_texture(sample_from->color, 0);
				t3d::update_shader_constants(constants, {.texel_size = 1.0f / (v2f)sample_from->color->size, .threshold = threshold});
				t3d::draw(3);

				swap(target.source, target.destination);

				sample_from = target.source;

				if (&target == &temp_targets.front())
					t3d::set_shader(downsample_shader);
			}

			t3d::set_shader(blur_x_shader);
			for (auto &target : temp_targets) {
				t3d::set_render_target(target.destination);
				t3d::set_viewport(target.destination->color->size);
				t3d::set_texture(target.source->color, 0);
				t3d::draw(3);
				swap(target.source, target.destination);
			}

			t3d::set_shader(blur_y_shader);
			for (auto &target : temp_targets) {
				t3d::set_render_target(target.destination);
				t3d::set_viewport(target.destination->color->size);
				t3d::set_texture(target.source->color, 0);
				t3d::draw(3);
				swap(target.source, target.destination);
			}
		}

		t3d::set_shader(blit_texture_shader);
		t3d::set_render_target(destination);
		t3d::set_viewport(destination->color->size);
		t3d::set_blend(t3d::BlendFunction_disable, {}, {});
		t3d::set_texture(source->color, 0);
		t3d::draw(3);

		t3d::set_shader(blit_texture_color_shader);
		t3d::update_shader_constants(blit_texture_color_constants, {.color = V4f(intensity)});
		t3d::set_shader_constants(blit_texture_color_constants, 0);
		t3d::set_blend(t3d::BlendFunction_add, t3d::Blend_one, t3d::Blend_one);
		for (auto &target : temp_targets) {
			t3d::set_texture(target.source->color, 0);
			t3d::draw(3);
		}
	}

	void resize(v2u size) {
		v2u next_size = size;
		u32 target_index = 0;

		for (u32 target_index = 0; target_index < 8; ++target_index) {
			if (target_index < temp_targets.size) {
				t3d::resize_texture(temp_targets[target_index].source     ->color, next_size);
				t3d::resize_texture(temp_targets[target_index].destination->color, next_size);
			} else {
				auto create_temp_target = [&]() {
					return t3d::create_render_target(
						t3d::create_texture(t3d::CreateTexture_default, next_size.x, next_size.y, 0, t3d::TextureFormat_rgb_f16, t3d::TextureFiltering_linear, t3d::Comparison_none),
						0
					);
				};
				temp_targets.add({
					create_temp_target(),	
					create_temp_target(),	
				});
			}

			next_size.x /= 2;
			next_size.y /= 2;
		}
	}
	void free() {}
};
