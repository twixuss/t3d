#pragma once
#include <t3d/shared.h>

#define FIELDS(f) \
f(v3f, axis,               {}) \
f(f32, degrees_per_second, {}) \

DECLARE_COMPONENT(Rotator) {
	void update() {
		auto &entity = this->entity();
		entity.rotation *= quaternion_from_axis_angle(normalize(axis, {1, 0, 0}), degrees_per_second * (pi * shared->frame_time / 180));
	}
};

#undef FIELDS
