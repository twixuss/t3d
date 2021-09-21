#pragma once
#include "common.h"
#include <tl/masked_block_list.h>

struct Material {
	tg::Shader *shader;
	tg::ShaderConstants *constants;
};
