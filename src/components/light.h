#pragma once
#include "../include/t3d.h"
#include "component.h"

u32 const shadow_map_resolution = 256;

struct Light : Component {
	t3d::RenderTarget *shadow_map;
	t3d::Texture *texture;
	m4 world_to_light_matrix;

	void free() {

	}
};

template <>
void on_create(Light &light) {
	auto depth = t3d::create_texture(t3d::CreateTexture_default, shadow_map_resolution, shadow_map_resolution, 0, t3d::TextureFormat_depth, t3d::TextureFiltering_linear, t3d::Comparison_less);
	light.shadow_map = t3d::create_render_target(0, depth);
}
