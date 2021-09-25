#pragma once
#include <t3d/app.h>

#define FIELDS(f) \
f(v3f, axis,               {}) \
f(f32, degrees_per_second, {}) \
f(f32, test_test, 1) \

DECLARE_COMPONENT(Rotator) {
	f32 start_pos;
	void start() {
		auto &entity = this->entity();
		start_pos = entity.position.y;
	}
	void update() {
		auto &entity = this->entity();
		entity.rotation *= quaternion_from_axis_angle(normalize(axis, {1, 0, 0}), degrees_per_second * (pi * app->frame_time / 180));
		entity.position.y = start_pos + tl::sin(app->time * 10) * test_test;
	}
};

#undef FIELDS
