#pragma once
#include <t3d.h>

t3d::Shader *blit_shader;

struct BlitColorConstants {
	v4f color;
};
t3d::TypedShaderConstants<BlitColorConstants> blit_color_constants;
t3d::Shader *blit_color_shader;
