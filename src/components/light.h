#pragma once
#include "../include/t3d.h"
#include "component.h"
#include "texture.h"

u32 const shadow_map_resolution = 256;

#define FIELDS(F) \
F(f32, intensity, 100) \
F(Texture *, mask, 0)

DECLARE_COMPONENT(Light) {
	t3d::RenderTarget *shadow_map;
	
	m4 world_to_light_matrix;
};

template <>
void component_init(Light &light) {
	auto depth = t3d::create_texture(t3d::CreateTexture_default, shadow_map_resolution, shadow_map_resolution, 0, t3d::TextureFormat_depth, t3d::TextureFiltering_linear, t3d::Comparison_less);
	light.shadow_map = t3d::create_render_target(0, depth);
}


template <>
void component_free(Light &light) {
	//t3d::free(light.shadow_map);
}