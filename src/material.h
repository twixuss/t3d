#pragma once
#include "tl.h"

struct Material {
	tg::Shader *shader;
	tg::ShaderConstants *constants;
};

MaskedBlockList<Material, 256> materials;
