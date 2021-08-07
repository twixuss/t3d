#pragma once
#define TL_ENABLE_VEC4_SIMD
#define TL_OPENGL_LOG_LEVEL 3
namespace tgraphics { struct Texture2D; }
namespace tg = tgraphics;
#define TL_FONT_TEXTURE_HANDLE tg::Texture2D *
#include <tl/common.h>
#include <tl/window.h>
#include <tl/console.h>
#include <tl/time.h>
#include <tl/mesh.h>
#include <tl/opengl.h>
#include <tl/tracking_allocator.h>
#include <tl/file_printer.h>
#include <tl/profiler.h>

using namespace tl;
inline umm get_hash(Span<utf8> value) {
	umm result = value.size * 13381;
	for (auto c : value) {
		result = result * 41681 + (umm)c * 7247;
	}
	return result;
}
inline umm get_hash(Span<utf16> value) {
	umm result = value.size * 13381;
	for (auto c : value) {
		result = result * 41681 + (umm)c * 7247;
	}
	return result;
}


#include <tl/hash_map.h>
#include <tl/font.h>
#include <tl/cpu.h>
#include <tl/ram.h>
#include <source_location>

struct Texture2DExtension {
	List<utf8> name;
	bool serializable;
};

#define TGRAPHICS_TEXTURE_2D_EXTENSION ::Texture2DExtension
#include <tgraphics/tgraphics.h>
#include "component_list.h"

inline bool operator==(std::source_location a, std::source_location b) {
	return a.line() == b.line() && a.column() == b.column() && tl::as_span(a.file_name()) == tl::as_span(b.file_name());
}


using Texture2D = tg::Texture2D;
