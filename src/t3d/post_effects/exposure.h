#pragma once
#include <t3d/post_effect.h>
#include <t3d/app.h>
#include <t3d/blit.h>

struct Exposure {
	inline static constexpr u32 min_texture_size = 16;
	struct Constants {
		f32 exposure_offset;
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

	tg::Shader *shader;
	tg::TypedShaderConstants<Constants> constants;
	f32 exposure = 1;
	f32 adapted_exposure = 1;
	f32 limit_min = 0;
	f32 limit_max = 1 << 24;
	List<tg::RenderTarget *> downsampled_targets;
	bool auto_adjustment;

	void init() {
		constants = app->tg->create_shader_constants<Exposure::Constants>();
		shader = app->tg->create_shader(u8R"(
#ifdef VERTEX_SHADER
#define V2F out
#else
#define V2F in
#endif

layout (std140, binding=0) uniform _ {
	float exposure_offset;
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


	void render(tg::RenderTarget *source, tg::RenderTarget *destination) {
		app->tg->set_rasterizer(
			app->tg->get_rasterizer()
				.set_depth_test(false)
				.set_depth_write(false)
		);

		if (auto_adjustment) {
			app->tg->disable_blend();

			app->tg->set_shader(app->blit_texture_shader);
			app->tg->set_sampler(tg::Filtering_linear, 0);

			auto sample_from = source;
			for (auto &target : downsampled_targets) {
				timed_block("blit"s);
				app->tg->set_render_target(target);
				app->tg->set_viewport(target->color->size);
				app->tg->set_texture(sample_from->color, 0);
				app->tg->draw(3);
				sample_from = target;
			}

			v3f texels[Exposure::min_texture_size * Exposure::min_texture_size];

			{
				timed_block("tg::read_texture"s);
				app->tg->read_texture(downsampled_targets.back()->color, as_bytes(array_as_span(texels)));
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

								constexpr f32 inv_diagonal = 1 / max(1, tl::sqrt(pow2(Exposure::min_texture_size * 0.5f - 0.5f) * 2));

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
				adapted_exposure = pow(2, lerp(log2(adapted_exposure), log2(target_exposure), app->frame_time));
			}
		}

		app->tg->update_shader_constants(constants, {
			.exposure_offset = adapted_exposure * exposure,
		});

		app->tg->set_shader(shader);
		app->tg->set_shader_constants(constants, 0);
		app->tg->set_render_target(destination);
		app->tg->set_viewport(destination->color->size);
		app->tg->set_sampler(tg::Filtering_nearest, 0);
		app->tg->set_texture(source->color, 0);
		app->tg->draw(3);
	}

	void resize(v2u size) {
		if (size.x == 0) size.x = 1;
		if (size.y == 0) size.y = 1;
		v2u next_size = max(floor_to_power_of_2(size - 1), V2u(min_texture_size));
		u32 target_index = 0;
		while (1) {
			if (target_index < downsampled_targets.size) {
				app->tg->resize_texture(downsampled_targets[target_index]->color, next_size);
			} else {
				downsampled_targets.add(app->tg->create_render_target(
					app->tg->create_texture_2d(next_size.x, next_size.y, 0, tg::Format_rgb_f16),
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
