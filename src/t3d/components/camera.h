#pragma once
#include <t3d/component.h>
#include <t3d/post_effect.h>

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
	v3f world_to_ndc(v4f point);
	v3f world_to_ndc(v3f point);
	v3f world_to_window(v4f point);
	v3f world_to_window(v3f point);

	void init();
	void free();
	void resize_targets(v2u size);
};

#undef FIELDS
