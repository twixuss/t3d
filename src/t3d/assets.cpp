#include "assets.h"
#include <t3d/app.h>

Span<u8> Assets::get_asset_data(Span<utf8> local_path) {
	if (app->is_editor) {
		auto full_path = tconcatenate(directory, local_path);
		auto buffer = with(temporary_allocator, read_entire_file(to_pathchars(full_path, true)));
		if (!buffer.data) {
			print(Print_error, "Asset not found '%'.\n", full_path);
			return {};
		}
		return buffer;
	} else {
		auto found = asset_path_to_data.find(local_path);
		assert_always(found, "Asset '%' was not found", local_path);
		return *found;
	}
}

Texture2D *Assets::get_texture_2d(Span<utf8> path) {
	auto found = textures_2d_by_path.find(path);
	if (found) {
		return *found;
	}

	print(Print_info, "Loading texture %.\n", path);
	auto result = app->tg->load_texture_2d(get_asset_data(path), {.generate_mipmaps = true});

	if (!result) {
		return 0;
	}
	result->name.set(path);

	textures_2d_by_path.get_or_insert(result->name) = result;
	return result;
}
TextureCube *Assets::get_texture_cube(Span<utf8> path) {
	auto found = textures_cubes_by_path.find(path);
	if (found) {
		return *found;
	}

	print(Print_info, "Loading cubemap %.\n", path);

	auto cubemap_desc = as_utf8(get_asset_data(path));
	if (!cubemap_desc.data) {
		return 0;
	}

	auto got_tokens = parse_tokens(cubemap_desc);

	if (!got_tokens) {
		return 0;
	}
	auto &tokens = got_tokens.value();
	auto t = tokens.data;

	tg::TextureCubePaths paths = {};

	while (t < tokens.end()) {
		if (t->kind != Token_identifier) {
			print(Print_error, "Parsing failed. Expected identifier instead of '%'.\n", t->string);
			return 0;
		}
		auto side = t->string;
		++t;
		if (t >= tokens.end()) {
			print(Print_error, "Parsing failed. Unexpected end of file.\n");
			return 0;
		}
		if (t->kind != '"') {
			print(Print_error, "Parsing failed. Expected string instead of '%'.\n", t->string);
			return 0;
		}
		auto path = t->string;
		++t;

		     if (side == u8"left"s  ) paths.left   = path;
		else if (side == u8"right"s ) paths.right  = path;
		else if (side == u8"top"s   ) paths.top    = path;
		else if (side == u8"bottom"s) paths.bottom = path;
		else if (side == u8"front"s ) paths.front  = path;
		else if (side == u8"back"s  ) paths.back   = path;
		else {
			print(Print_error, "Parsing failed. Expected left/right/top/bottom/front/back instead of '%'.\n", t->string);
			return 0;
		}
	}

	tg::Pixels pixels[6];
	void *datas[6];
	u32 size = 0;
	tg::Format format = {};

	for (int i = 0; i < 6; ++i) {
		pixels[i] = tg::load_pixels(get_asset_data(paths.paths[i]));
		datas[i] = pixels[i].data;
		if (pixels[i].size.x != pixels[i].size.y) {
			print(Print_error, "Loading failed. Face sizes do not match.\n");
			return 0;
		}
		if (size == 0) {
			size = pixels[i].size.x;
			format = pixels[i].format;
		} else if (size != pixels[i].size.x) {
			print(Print_error, "Loading failed. Face sizes do not match.\n");
			return 0;
		} else if (format != pixels[i].format) {
			print(Print_error, "Loading failed. Face formats do not match.\n");
			return 0;
		}
	}
	defer {
		for (int i = 0; i < 6; ++i) {
			pixels[i].free(pixels[i].data);
		}
	};

	auto result = app->tg->create_texture_cube(size, datas, format);

	if (!result) {
		return 0;
	}
	app->tg->generate_mipmaps_cube(result, {});

	result->name.set(path);
	textures_cubes_by_path.get_or_insert(result->name) = result;
	return result;
}

Mesh *Assets::create_mesh(tl::CommonMesh &mesh) {
	Mesh result = {};
	result.vertex_buffer = app->tg->create_vertex_buffer(
		as_bytes(mesh.vertices),
		{
			tg::Element_f32x3, // position
			tg::Element_f32x3, // normal
			tg::Element_f32x4, // color
			tg::Element_f32x2, // uv
		}
	);

	result.index_buffer = app->tg->create_index_buffer(as_bytes(mesh.indices), sizeof(u32));

	result.index_count = mesh.indices.size;

	result.positions.reserve(mesh.vertices.size);
	for (auto &vertex : mesh.vertices) {
		result.positions.add(vertex.position);
	}

	result.indices = copy(mesh.indices);

	auto added = meshes.add();
	*added.pointer = result;
	return added.pointer;
}
