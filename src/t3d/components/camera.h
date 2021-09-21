#pragma once
#include <t3d/shared_data.h>
#include <t3d/post_effect.h>
#include <t3d/post_effects/exposure.h>
#include <t3d/post_effects/bloom.h>
#include <t3d/post_effects/dither.h>

#define FIELDS(F) \
F(f32, fov,        pi * 0.5f) \
F(f32, near_plane, 0.01f) \
F(f32, far_plane,  100.0f) \

DECLARE_COMPONENT(Camera) {
	m4 world_to_camera_matrix;

	tg::RenderTarget *source_target;
	tg::RenderTarget *destination_target;
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

	void init() {
		auto create_hdr_target = [&]() {
			auto hdr_color = tg::create_texture_2d(1, 1, 0, tg::Format_rgb_f16);
			auto hdr_depth = tg::create_texture_2d(1, 1, 0, tg::Format_depth);
			return tg::create_render_target(hdr_color, hdr_depth);
		};
		source_target      = create_hdr_target();
		destination_target = create_hdr_target();
	}

	void free() {
		for (auto &effect : post_effects) {
			effect.free();
		}
		tl::free(post_effects);
	}
	void resize_targets(v2u size) {
		tg::resize_texture(source_target->color, size);
		tg::resize_texture(source_target->depth, size);
		tg::resize_texture(destination_target->color, size);
		tg::resize_texture(destination_target->depth, size);
	}
};

#undef FIELDS

REGISTER_COMPONENT(Camera)
