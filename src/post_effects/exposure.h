#pragma once
#include <t3d.h>
#include "../src/time.h" // TODO without  ../src/  crt's time.h will be included
#include "blit.h"

struct Exposure {
	inline static constexpr u32 min_texture_size = 16;
	struct Constants {
		f32 exposure_offset;
		f32 exposure_scale;
	};

	enum ApproachKind {
		Approach_lerp,
		Approach_log_lerp,
	};

	enum MaskKind {
		Mask_one,
		Mask_proximity,
	};

	ApproachKind approach_kind;

	MaskKind mask_kind;
	f32 mask_radius;

	t3d::Shader *shader;
	t3d::TypedShaderConstants<Constants> constants;
	f32 exposure = 1;
	f32 scale;
	f32 limit_min = 0;
	f32 limit_max = 1 << 24;
	List<t3d::RenderTarget *> downsampled_targets;

	void init() {
		constants = t3d::create_shader_constants<Exposure::Constants>();
		shader = t3d::create_shader(u8R"(
#ifdef VERTEX_SHADER
#define V2F out
#else
#define V2F in
#endif

layout (std140, binding=0) uniform _ {
	float exposure_offset;
	float exposure_scale;
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
	vec3 color = texture(main_texture, vertex_uv).rgb;
	color = 1 - exp(-color * exposure_offset);
	color = -log(max(1 - color, 0.000000000001));
	//color = min(color, vec3(256));
	//color = log(color + 1);
	//color = pow(color, vec3(1 / 2.2));
	fragment_color = vec4(color, 1);
}
#endif
)"s);
	}


	void render(t3d::RenderTarget *source, t3d::RenderTarget *destination) {
		timed_block("Exposure::render"s);

		{
			timed_block("Downsample"s);
			t3d::set_rasterizer(
				t3d::get_rasterizer()
					.set_depth_test(false)
					.set_depth_write(false)
			);

			t3d::set_blend(t3d::BlendFunction_disable, {}, {});

			t3d::set_shader(blit_shader);

			auto sample_from = source;
			for (auto &target : downsampled_targets) {
				timed_block("blit"s);
				t3d::set_render_target(target);
				t3d::set_viewport(target->color->size);
				t3d::set_texture(sample_from->color, 0);
				t3d::draw(3);
				sample_from = target;
			}
		}

		v3f texels[Exposure::min_texture_size * Exposure::min_texture_size];

		{
			timed_block("t3d::read_texture"s);
			t3d::read_texture(downsampled_targets.back()->color, as_bytes(array_as_span(texels)));
		}
		{
			timed_block("average"s);
			f32 target_exposure = 0;
			switch (mask_kind) {
				case Exposure::Mask_one: {
					f32 sum_luminance = 0;
					for (auto texel : texels) {
						sum_luminance += max(texel.x, texel.y, texel.z);
					}
					if (sum_luminance == 0) {
						target_exposure = limit_max;
					} else {
						target_exposure = clamp(1 / sum_luminance * count_of(texels), limit_min, limit_max);
					}
					break;
				}
				case Exposure::Mask_proximity: {
					f32 sum_luminance = 0;
					f32 sum_mask = 0;
					for (u32 y = 0; y < Exposure::min_texture_size; ++y) {
						for (u32 x = 0; x < Exposure::min_texture_size; ++x) {
							auto texel = texels[y*Exposure::min_texture_size+x];
							f32 dist = distance(V2f(x,y), V2f(63)*0.5);

							constexpr f32 inv_diagonal = 1 / max(1, CE::sqrt(pow2(Exposure::min_texture_size * 0.5f - 0.5f) * 2));

							f32 mask = map_clamped(dist * inv_diagonal, mask_radius, 0.0f, 0.0f, 1.0f);
							sum_mask += mask;
							sum_luminance += mask * max(texel.x, texel.y, texel.z);
						}
					}
					if (sum_luminance == 0) {
						target_exposure = limit_max;
					} else {
						target_exposure = clamp(1 / sum_luminance * sum_mask, limit_min, limit_max);
					}
					break;
				}
				default:
					invalid_code_path("mask_kind is invalid");
					break;
			}
			exposure = pow(2, lerp(log2(exposure), log2(target_exposure), frame_time));
		}

		t3d::update_shader_constants(constants, {
			.exposure_offset = +exposure * scale,
			.exposure_scale = scale
		});

		t3d::set_shader(shader);
		t3d::set_shader_constants(constants, 0);
		t3d::set_render_target(destination);
		t3d::set_viewport(destination->color->size);
		t3d::set_texture(source->color, 0);
		t3d::draw(3);
	}

	void resize(v2u size) {
		if (size.x == 0) size.x = 1;
		if (size.y == 0) size.y = 1;
		v2u next_size = max(floor_to_power_of_2(size - 1), V2u(min_texture_size));
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

			if (next_size.x == Exposure::min_texture_size && next_size.y == Exposure::min_texture_size) break;

			if (next_size.x != Exposure::min_texture_size) next_size.x /= 2;
			if (next_size.y != Exposure::min_texture_size) next_size.y /= 2;

			++target_index;
		}
	}
	void free() {}
};