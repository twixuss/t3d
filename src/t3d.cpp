#define T3D_IMPL
#define STB_IMAGE_IMPLEMENTATION
#include "../include/t3d.h"
#include <tl/console.h>
#include <stb_image.h>

namespace t3d {

#define A(ret, name, args, values) ret (*_##name) args;
APIS(A)
#undef A

RenderTarget *back_buffer;
v2u min_texture_size;

namespace d3d11 { bool init(InitInfo init_info); void free(); }
namespace gl    { bool init(InitInfo init_info); void free(); }

static bool init_api(GraphicsApi api, InitInfo init_info) {
	switch (api) {
		case t3d::GraphicsApi_d3d11: return d3d11::init(init_info);
		case t3d::GraphicsApi_opengl: return gl::init(init_info);
	}
	return false;
}

static bool check_api() {
	bool result = true;
#define A(ret, name, args, values) if(!_##name){print("t3d::" #name " was not initialized.\n");result=false;}
APIS(A)
#undef A
	return result;
}

GraphicsApi current_api;

bool init(GraphicsApi api, InitInfo init_info) {
	return current_api = api, init_api(api, init_info) && check_api();
}

void free() {
	switch (current_api) {
		case t3d::GraphicsApi_d3d11: return d3d11::free();
		case t3d::GraphicsApi_opengl: return gl::free();
	}
	current_api = {};
}

Pixels load_pixels(Span<pathchar> path) {
	print(Print_info, "Loading texture %.\n", path);

	Pixels result;

	auto file = read_entire_file(path);
	if (!file.data) {
		print(Print_error, "Failed to read file %.\n", path);
		return {};
	}

	//if (starts_with(file, "#?RADIANCE"s)) {
	//	auto c = file.data;
	//	while (1) {
	//		if (*c == '#' || *c == '\n' || starts_with(Span{c, 8}, "EXPOSURE"s) || starts_with(Span{c, 6}, "FORMAT"s)) {
	//			while (*c != '\n') {
	//				++c;
	//			}
	//			++c;
	//			continue;
	//		} else if (starts_with(Span{c, 3}, "+Y "s) || starts_with(Span{c, 3}, "-Y "s)) {
	//			c += 3;
	//			auto number_start = c;
	//			while (*c != ' ' && *c != '\n') {
	//				++c;
	//			}
	//			auto number_end = c;
	//			++c;
	//			if (auto number = parse_u64((Span<char>)Span(number_start, number_end))) {
	//				result.size.y = number.value;
	//			} else {
	//				print(Print_error, "Failed to parse hdr height.");
	//				return {};
	//			}
	//			continue;
	//		} else if (starts_with(Span{c, 3}, "+X "s) || starts_with(Span{c, 3}, "-X "s)) {
	//			c += 3;
	//			auto number_start = c;
	//			while (*c != ' ' && *c != '\n') {
	//				++c;
	//			}
	//			auto number_end = c;
	//			++c;
	//			if (auto number = parse_u64((Span<char>)Span(number_start, number_end))) {
	//				result.size.x = number.value;
	//			} else {
	//				print(Print_error, "Failed to parse hdr height.");
	//				return {};
	//			}
	//			continue;
	//		}
	//		break;
	//	}
	//	if (*c++ != 0x02) { print(Print_error, "Failed to parse %.\n", path); }
	//	if (*c++ != 0x02) { print(Print_error, "Failed to parse %.\n", path); }
	//
	//	u16 check_width = (u16)*c++ << 8;
	//	check_width |= *c++;
	//
	//	if (result.size.x != check_width) {
	//		print(Print_error, "Failed to parse %: check width failed.\n", path);
	//	}
	//
	//	result.format = TextureFormat_rgb_f32;
	//
	//	List<u8> pixel_bytes;
	//	pixel_bytes.reserve((umm)result.size.x * result.size.y * 12);
	//	while (c != file.end()) {
	//		if (*c & 0x80) {
	//			// run
	//			u32 run_count = *c & 0x80;
	//			while (run_count--) {
	//				pixel_bytes.add(c[1]);
	//				pixel_bytes.add(c[2]);
	//				pixel_bytes.add(c[3]);
	//			}
	//			c += 5;
	//		} else {
	//			// non-run
	//			pixel_bytes.add(c[0]);
	//			pixel_bytes.add(c[1]);
	//			pixel_bytes.add(c[2]);
	//			c += 5;
	//		}
	//	}
	//
	//	int x = 1;
	//
	int width, height;
	if (stbi_is_hdr_from_memory(file.data, file.size)) {
		result.data = stbi_loadf_from_memory(file.data, file.size, &width, &height, 0, 4);
		result.format = TextureFormat_rgba_f32;
	} else {
		result.data = stbi_load_from_memory(file.data, file.size, &width, &height, 0, 4);
		result.format = TextureFormat_rgba_u8n;
	}
	if (!result.data) {
		print(Print_error, "Failed to parse texture from %. Reason: %.\n", path, stbi_failure_reason());
		return {};
	}
	result.size = {(u32)width, (u32)height};
	result.free = stbi_image_free;
	return result;
}

}

