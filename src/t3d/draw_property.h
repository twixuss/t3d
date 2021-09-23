#pragma once
#include <t3d/common.h>
#include <t3d/mesh.h>
#include <tl/quaternion.h>

void draw_property(Span<utf8> name, f32 &value, u64 id = 0, std::source_location location = std::source_location::current());
void draw_property(Span<utf8> name, v3f &value, u64 id = 0, std::source_location location = std::source_location::current());
void draw_property(Span<utf8> name, quaternion &value, u64 id = 0, std::source_location location = std::source_location::current());
void draw_property(Span<utf8> name, List<utf8> &value, u64 id = 0, std::source_location location = std::source_location::current());
void draw_property(Span<utf8> name, tg::Texture2D *&value, u64 id = 0, std::source_location location = std::source_location::current());
void draw_property(Span<utf8> name, Mesh *&value, u64 id = 0, std::source_location location = std::source_location::current());
