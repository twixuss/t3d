#pragma once
#include "../include/t3d.h"
#include "component.h"
#include "post_effect.h"
#include "post_effects/exposure.h"
#include "post_effects/bloom.h"
#include "post_effects/dither.h"

struct Camera : Component {
	f32 fov = pi * 0.5f;

	m4 world_to_camera_matrix;

	t3d::RenderTarget *source_target;
	t3d::RenderTarget *destination_target;
	List<PostEffect> post_effects;

	template <class Effect>
	Effect &add_post_effect() {
		PostEffect effect;
		effect.allocator = current_allocator;
		effect.data = effect.allocator.allocate<Effect>();
		effect._init   = post_effect_init<Effect>;
		effect._free   = post_effect_free<Effect>;
		effect._render = post_effect_render<Effect>;
		effect._resize = post_effect_resize<Effect>;
		effect.init();
		post_effects.add(effect);
		return *(Effect *)effect.data;
	}
	v3f world_to_camera(v4f point) {
		auto p = world_to_camera_matrix * point;
		return {p.xyz / p.w};
	}
	v3f world_to_camera(v3f point) {
		return world_to_camera(V4f(point, 1));
	}

	void free() {
		for (auto &effect : post_effects) {
			effect.free();
		}
		tl::free(post_effects);
	}
};

template <>
void on_create(Camera &camera) {
	auto create_hdr_target = [&]() {
		auto hdr_color = t3d::create_texture(t3d::CreateTexture_default, 1, 1, 0, t3d::TextureFormat_rgb_f16, t3d::TextureFiltering_linear, t3d::Comparison_none);
		auto hdr_depth = t3d::create_texture(t3d::CreateTexture_default, 1, 1, 0, t3d::TextureFormat_depth,   t3d::TextureFiltering_none,   t3d::Comparison_none);
		return t3d::create_render_target(hdr_color, hdr_depth);
	};
	camera.source_target      = create_hdr_target();
	camera.destination_target = create_hdr_target();

	auto &exposure = camera.add_post_effect<Exposure>();
	exposure.scale = 0.5f;
	exposure.limit_min = 1.0f / 16;
	exposure.limit_max = 1024;
	exposure.approach_kind = Exposure::Approach_log_lerp;
	exposure.mask_kind = Exposure::Mask_one;
	exposure.mask_radius = 1;

	auto &bloom = camera.add_post_effect<Bloom>();
	bloom.threshold = 1;

	auto &dither = camera.add_post_effect<Dither>();
}
