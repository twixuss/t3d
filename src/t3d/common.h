#pragma once
#define TL_TEMP_STORAGE_LIMIT 1024*1024*1024
#define TL_OPENGL_LOG_LEVEL 3
namespace tgraphics { struct Texture2D; }
namespace tg = tgraphics;
#define TL_FONT_TEXTURE_HANDLE tg::Texture2D *
#include <tl/common.h>

using namespace tl;

#include <tl/window.h>
#include <tl/console.h>
#include <tl/time.h>
#include <tl/mesh.h>
#include <tl/opengl.h>
#include <tl/tracking_allocator.h>
#include <tl/file_printer.h>
#include <tl/profiler.h>
#include <tl/process.h>

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

inline bool operator==(std::source_location a, std::source_location b) {
	return a.line() == b.line() && a.column() == b.column() && tl::as_span(a.file_name()) == tl::as_span(b.file_name());
}


using Texture2D = tg::Texture2D;

inline Optional<f32> parse_f32(Span<utf8> string) {
	if (!string.size)
		return {};

	u64 whole_part = 0;
	auto c = string.data;
	auto end = string.end();

	bool negative = false;
	if (*c == '-') {
		negative = true;
		++c;
	}

	bool do_fract_part = false;
	while (1) {
		if (c == end)
			break;

		if (*c == '.' || *c == ',') {
			do_fract_part = true;
			break;
		}

		u32 digit = *c - '0';
		if (digit >= 10)
			return {};

		whole_part *= 10;
		whole_part += digit;

		++c;
	}

	u64 fract_part  = 0;
	u64 fract_denom = 1;

	if (do_fract_part) {
		++c;
		while (1) {
			if (c == end) {
				break;
			}

			u32 digit = *c - '0';
			if (digit >= 10)
				return {};

			fract_denom *= 10;
			fract_part *= 10;
			fract_part += digit;

			++c;
		}
	}

	f64 result = (f64)whole_part + (f64)fract_part / (f64)fract_denom;
	if (negative) {
		result = -result;
	}
	return result;
}

extern "C" TL_DLL_EXPORT struct SharedData *shared;

void allocate_shared();

extern "C" TL_DLL_EXPORT void initialize_module();

void set_module_shared(HMODULE module);

void update_component_info(struct ComponentDesc const &desc);
