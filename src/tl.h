#pragma once
#define TL_ENABLE_VEC4_SIMD
#define TL_OPENGL_LOG_LEVEL 3
namespace t3d { struct Texture; }
#define TL_FONT_TEXTURE_HANDLE t3d::Texture *
#include <tl/common.h>
#include <tl/window.h>
#include <tl/console.h>
#include <tl/time.h>
#include <tl/mesh.h>
#include <tl/opengl.h>
#include <tl/tracking_allocator.h>
#include <tl/file_printer.h>
#include <tl/profiler.h>
#include <tl/hash_map.h>
#include <tl/font.h>
#include <tl/cpu.h>
#include <source_location>
#include "../include/t3d.h"
#include "component_list.h"

inline bool operator==(std::source_location a, std::source_location b) {
	return a.line() == b.line() && a.column() == b.column() && tl::as_span(a.file_name()) == tl::as_span(b.file_name());
}
