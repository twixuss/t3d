#pragma once
void t3d_assert(char const *, char const *, char const *, int);
#define ASSERTION_FAILURE(cause, expression, ...) t3d_assert(cause, expression, __FILE__, __LINE__)
#define TL_OPENGL_LOG_LEVEL 3
#define TL_ENABLE_PROFILER 0 // TODO: fix profiler
namespace tgraphics { struct Texture2D; }
namespace tg = tgraphics;
#define TL_FONT_TEXTURE_HANDLE tg::Texture2D *
#include <tl/common.h>
#include <tl/list.h>
#include <tl/optional.h>

using namespace tl;

#include <source_location>

struct Texture2DExtension {
	List<utf8> name;
};

struct TextureCubeExtension {
	List<utf8> name;
};

#define TGRAPHICS_TEXTURE_2D_EXTENSION ::Texture2DExtension
#define TGRAPHICS_TEXTURE_CUBE_EXTENSION ::TextureCubeExtension
#include <tgraphics/tgraphics.h>

inline bool operator==(std::source_location a, std::source_location b) {
	return a.line() == b.line() && a.column() == b.column() && tl::as_span(a.file_name()) == tl::as_span(b.file_name());
}


using Texture2D   = tg::Texture2D;
using TextureCube = tg::TextureCube;

inline Optional<f32> parse_f32(Span<utf8> string) {
	if (!string.count)
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

extern "C" TL_DLL_EXPORT struct AppData *app;
extern "C" TL_DLL_EXPORT struct EditorData *editor;

void allocate_app();

extern "C" TL_DLL_EXPORT void initialize_thread();

void set_module_shared(void *module);

void update_component_info(struct ComponentDesc const &desc);

using TokenKind = u16;
enum : TokenKind {
	Token_identifier = 0x100,
	Token_number,
	Token_null,
};

struct Token {
	TokenKind kind = {};
	Span<utf8> string;
};

Optional<List<Token>> parse_tokens(Span<utf8> source);

struct Uid {
	u64 value;
};

inline umm append(StringBuilder &builder, Uid uid) {
	return append(builder, FormatInt{.value = uid.value, .radix = 16, .leading_zero_count = 16});
}

inline bool operator==(Uid const &a, Uid const &b) { return a.value == b.value; }
inline bool operator!=(Uid const &a, Uid const &b) { return a.value != b.value; }

template <>
inline umm get_hash(Uid a) {
	return a.value;
}

Uid create_uid();
