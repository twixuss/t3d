#pragma once
#define TL_IMPL
#define TGRAPHICS_IMPL
#include <freetype/freetype.h> // TODO: Shold this be in user code?
#include <t3d/component.h>
#include <t3d/serialize.h>

#pragma comment(lib, "freetype.lib")

extern "C" void register_component_Rotator(HashMap<Span<utf8>, ComponentUID> &component_name_to_uid, HashMap<ComponentUID, ComponentInfo> &component_infos, ComponentUID (*get_new_uid)());
extern "C" __declspec(dllexport) void t3d_register_components(HashMap<Span<utf8>, ComponentUID> &component_name_to_uid, HashMap<ComponentUID, ComponentInfo> &component_infos, ComponentUID (*get_new_uid)()) {
register_component_Rotator(component_name_to_uid, component_infos, get_new_uid);
}