#define TGRAPHICS_IMPL
#define TL_IMPL
#define TL_GL_VALIDATE_EACH_CALL
#pragma comment(lib, "freetype.lib")
#include <freetype/freetype.h>
#include <t3d/entity.h>
#include <t3d/common.h>
#include <t3d/component.h>
#include <t3d/gui.h>
#include <t3d/app.h>
#include <tl/masked_block_list.h>
#include <tl/thread.h>
#include <tl/profiler.h>
#include <tl/process.h>
#include <tl/cpu.h>
#include <tl/ram.h>
#include <tl/time.h>

AppData *app;
EditorData *editor;

Uid create_uid() {
	return { next(app->uid_generator) };
}

void allocate_app() {
	allocate(app);
	app->allocator = default_allocator;
	app->uid_generator.v = 0;
	while (app->uid_generator.v == 0) {
		_rdrand64_step(&app->uid_generator.v);
	}
}

void set_module_shared(void *module) {
	*(AppData **)GetProcAddress((HMODULE)module, "app") = app;
	*(EditorData **)GetProcAddress((HMODULE)module, "editor") = editor;
}

void initialize_thread() {
	init_allocator();
	current_printer = console_printer;
}

Optional<List<Token>> parse_tokens(Span<utf8> source) {
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
		if (got) {
			c = got.value();
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
				case ';':
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

void t3d_assert(char const *cause, char const *expression, char const *file, int line) {
	print("%: '%' in %:%", cause, expression, file, line);
	debug_break();
}
