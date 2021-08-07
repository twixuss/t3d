#pragma once
#include "../component.h"
#include "../entity.h"

#define FIELDS(f) \
f(v3f, axis,               {}) \
f(f32, degrees_per_second, {}) \

DECLARE_COMPONENT(Rotator) {
	void update() {
		auto &entity = this->entity();
		entity.rotation *= quaternion_from_axis_angle(normalize(axis, {1, 0, 0}), degrees_per_second * (pi * frame_time / 180));
	}
};
