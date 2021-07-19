#pragma once
#include <t3d.h>
#include "viewport.h"

struct Bloom {
	inline static constexpr u32 min_texture_size = 1;
	struct Constants {
		v2f texel_size;
		f32 threshold;
	};

	t3d::Shader *downsample_shader;
	t3d::Shader *upsample_shader;
	t3d::Shader *downsample_filter_shader;
	t3d::TypedShaderConstants<Constants> constants;
	List<t3d::RenderTarget *> downsampled_targets;
	f32 threshold = 1;

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
	float intensity;
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
		upsample_shader = t3d::create_shader(tconcatenate(header, u8R"(
#ifdef FRAGMENT_SHADER
out vec4 fragment_color;
void main() {
	fragment_color = get_sample(vertex_uv, 0.5);
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
			for (auto &target : downsampled_targets) {
				t3d::set_render_target(target);
				t3d::set_viewport(target->color->size);
				t3d::set_texture(sample_from->color, 0);
				t3d::update_shader_constants(constants, {.texel_size = 1.0f / (v2f)sample_from->color->size, .threshold = threshold});
				t3d::draw(3);
				sample_from = target;

				if (&target == &downsampled_targets.front())
					t3d::set_shader(downsample_shader);
			}
		}

		{
			timed_block("Upsample"s);
			t3d::set_shader(upsample_shader);
			for (s32 i = 0; i < downsampled_targets.size - 1; ++i) {
				auto &target = downsampled_targets[i];
				auto &sample_from = downsampled_targets[i + 1];

				t3d::set_render_target(target);
				t3d::set_viewport(target->color->size);
				t3d::set_texture(sample_from->color, 0);
				t3d::update_shader_constants(constants, {.texel_size = 1.0f / (v2f)sample_from->color->size});
				t3d::draw(3);
			}
		}
		t3d::set_shader(blit_shader);
		t3d::set_render_target(destination);
		t3d::set_viewport(destination->color->size);
		t3d::set_blend(t3d::BlendFunction_disable, {}, {});
		t3d::set_texture(source->color, 0);
		t3d::draw(3);

		t3d::set_shader(blit_color_shader);
		t3d::update_shader_constants(blit_color_constants, {.color = V4f(0.1f)});
		t3d::set_shader_constants(blit_color_constants, 0);
		t3d::set_blend(t3d::BlendFunction_add, t3d::Blend_one, t3d::Blend_one);
		for (auto &target : downsampled_targets) {
			t3d::set_texture(target->color, 0);
			t3d::draw(3);
			//if (&target == &downsampled_targets[0])
			//	break;
		}

		//t3d::set_blend(t3d::BlendFunction_disable, {}, {});
		//t3d::set_texture(downsampled_targets[2]->color, 0);
		//t3d::draw(3);
		//t3d::set_blend(t3d::BlendFunction_add, t3d::Blend_one, t3d::Blend_one);
		//t3d::set_texture(source->color, 0);
		//t3d::set_texture(downsampled_targets[0]->color, 0);
		//t3d::draw(3);
		//t3d::set_texture(downsampled_targets[1]->color, 0);
		//t3d::draw(3);
		//t3d::set_texture(downsampled_targets[2]->color, 0);
		//t3d::draw(3);
	}

	void resize(v2u size) {
		v2u next_size = floor_to_power_of_2(size - 1);
		u32 target_index = 0;
		while (1) {
			if (target_index < downsampled_targets.size) {
				t3d::resize_texture(downsampled_targets[target_index]->color, next_size);
			} else {
				downsampled_targets.add(t3d::create_render_target(
					t3d::create_texture(t3d::CreateTexture_default, next_size.x, next_size.y, 0, t3d::TextureFormat_rgb_f16, t3d::TextureFiltering_linear, t3d::Comparison_none),
					0
				));
			}

			if (next_size.x == Bloom::min_texture_size && next_size.y == Bloom::min_texture_size) break;

			if (next_size.x != Bloom::min_texture_size) next_size.x /= 2;
			if (next_size.y != Bloom::min_texture_size) next_size.y /= 2;

			++target_index;
		}
	}
	void free() {}
};
