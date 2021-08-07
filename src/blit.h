#pragma once
#include "tl.h"

tg::Shader *blit_texture_shader;

struct BlitColorConstants {
	v4f color;
};
tg::TypedShaderConstants<BlitColorConstants> blit_color_constants;
tg::Shader *blit_color_shader;

struct BlitTextureColorConstants {
	v4f color;
};
tg::TypedShaderConstants<BlitTextureColorConstants> blit_texture_color_constants;
tg::Shader *blit_texture_color_shader;
