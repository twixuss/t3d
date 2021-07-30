#pragma once
#include "../component.h"
#include "../entity.h"

#define FIELDS(f) \
f(v3f, axis, {}) \
f(f32, degrees_per_second, {}) \

DECLARE_COMPONENT(Rotator) {

};

template <>
void component_update(Rotator &rotator) {
	auto &entity = rotator.entity();
	entity.rotation *= quaternion_from_axis_angle(normalize(rotator.axis, {1, 0, 0}), rotator.degrees_per_second * (pi * frame_time / 180));
}
