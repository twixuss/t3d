#pragma once
#include <t3d/component.h>
#include <t3d/serialize.h>

#pragma comment(lib, "freetype.lib")

extern "C" void register_component_Rotator();
extern "C" __declspec(dllexport) void t3d_register_components() {
register_component_Rotator();
}