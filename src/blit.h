#pragma once
#include <t3d.h>

t3d::Shader *blit_texture_shader;

struct BlitColorConstants {
	v4f color;
};
t3d::TypedShaderConstants<BlitColorConstants> blit_color_constants;
t3d::Shader *blit_color_shader;

struct BlitTextureColorConstants {
	v4f color;
};
t3d::TypedShaderConstants<BlitTextureColorConstants> blit_texture_color_constants;
t3d::Shader *blit_texture_color_shader;
