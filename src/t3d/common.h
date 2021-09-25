#pragma once
#define TL_TEMP_STORAGE_LIMIT 1024*1024*1024
#define TL_OPENGL_LOG_LEVEL 3
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

extern "C" TL_DLL_EXPORT struct AppData *app;
extern "C" TL_DLL_EXPORT struct EditorData *editor;

void allocate_app();

extern "C" TL_DLL_EXPORT void initialize_module();

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

inline Optional<List<Token>> parse_tokens(Span<utf8> source) {
	List<Token> tokens;

	HashMap<Span<utf8>, TokenKind> string_to_token_kind;
	string_to_token_kind.allocator = temporary_allocator;
	string_to_token_kind.get_or_insert(u8"null"s)   = Token_null;

	auto current_char_p = source.data;
	auto next_char_p = current_char_p;
	auto end = source.end();

	utf32 c = 0;
	auto next_char = [&] {
		current_char_p = next_char_p;
		if (current_char_p >= end) {
			return false;
		}
		auto got = get_char_and_advance_utf8(&next_char_p);
		if (got.valid()) {
			c = got.get();
			return true;
		}
		return false;
	};

	next_char();

	while (current_char_p < end) {
		while (current_char_p != end && is_whitespace(c)) {
			next_char();
		}
		if (current_char_p == end) {
			break;
		}

		if (is_alpha(c) || c == '_') {
			Token token;
			token.string.data = current_char_p;

			while (next_char() && (is_alpha(c) || c == '_' || is_digit(c))) {
			}

			token.string.size = current_char_p - token.string.data;

			auto found = string_to_token_kind.find(token.string);
			if (found) {
				token.kind = *found;
			} else {
				token.kind = Token_identifier;
			}

			tokens.add(token);
		} else if (is_digit(c) || c == '-') {
			Token token;
			token.kind = Token_number;
			token.string.data = current_char_p;

			while (next_char() && is_digit(c)) {
			}

			if (current_char_p != end) {
				if (c == '.') {
					while (next_char() && is_digit(c)) {
					}
				}
			}

			token.string.size = current_char_p - token.string.data;
			tokens.add(token);
		} else {
			switch (c) {
				case '"': {
					Token token;
					token.kind = '"';
					token.string.data = current_char_p + 1;

				continue_search:
					while (next_char() && (c != '"')) {
					}

					if (current_char_p == end) {
						print(Print_error, "Unclosed string literal\n");
						return {};
					}

					if (current_char_p[-1] == '\\') {
						goto continue_search;
					}

					token.string.size = current_char_p - token.string.data;

					next_char();

					tokens.add(token);
					break;
				}
				case '{':
				case '}': {
					Token token;
					token.kind = c;
					token.string.data = current_char_p;
					token.string.size = 1;
					tokens.add(token);
					next_char();
					break;
				}
				default: {
					print(Print_error, "Parsing failed: invalid character '%'\n", c);
					return {};
				}
			}
		}
	}

	return tokens;
}
