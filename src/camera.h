#pragma once
#include "../include/t3d.h"
#include "component.h"

struct Camera : Component {
	f32 fov = pi * 0.5f;
};

Camera *main_camera;

template <>
void on_create(Camera &camera) {
	main_camera = &camera;
}
