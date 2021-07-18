#pragma once
#include "../include/t3d.h"

struct Material {
	t3d::Shader *shader;
	t3d::ShaderConstants *constants;
};
