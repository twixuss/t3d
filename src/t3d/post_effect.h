#pragma once
#include "common.h"

struct PostEffect {
	Allocator allocator = current_allocator;
	void *data;
	void (*_init)(void *data);
	void (*_free)(void *data);
	void (*_render)(void *data, tg::RenderTarget *source, tg::RenderTarget *destination);
	void (*_resize)(void *data, v2u size);

	void init() {
		_init(data);
	}
	void free() {
		_free(data);
		allocator.free(data);
	}
	void render(tg::RenderTarget *source, tg::RenderTarget *destination) {
		_render(data, source, destination);
	}
	void resize(v2u size) {
		_resize(data, size);
	}
};

template <class Effect> void post_effect_init(void *data) { ((Effect *)data)->init(); }
template <class Effect> void post_effect_free(void *data) { ((Effect *)data)->free(); }
template <class Effect> void post_effect_render(void *data, tg::RenderTarget *source, tg::RenderTarget *destination) { ((Effect *)data)->render(source, destination); }
template <class Effect> void post_effect_resize(void *data, v2u size) { ((Effect *)data)->resize(size); }
