#pragma once
#include "../include/t3d.h"

struct Texture {
	t3d::Texture *texture;
	List<utf8> name;
	bool serializable;
};

Texture *white_texture;
Texture *black_texture;
