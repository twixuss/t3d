#pragma once
#include "texture.h"

#if T3D_EDITOR

Span<u8> get_asset_data(Span<utf8> path) {
	return with(temporary_allocator, read_entire_file(to_pathchars(path, true)));
}

#else

HashMap<Span<utf8>, Span<u8>> asset_path_to_data;

Span<u8> get_asset_data(Span<utf8> path) {
	auto found = asset_path_to_data.find(path);
	assert_always(found, "Asset '%' was not found", path);
	return *found;
}

#endif

struct {
	struct {
		HashMap<Span<utf8>, Texture2D *> by_path;
		Texture2D *get(Span<utf8> path) {
			auto found = by_path.find(path);
			if (found) {
				return *found;
			}
			
			print(Print_info, "Loading texture %.\n", path);
			auto result = tg::load_texture_2d(get_asset_data(path));

			if (!result) {
				return 0;
			}
			result->serializable = true;
			result->name.set(path);

			by_path.get_or_insert(result->name) = result;
			return result;
		}
	} textures_2d;
	struct {
		MaskedBlockList<Mesh, 256> meshes;
		HashMap<Span<utf8>, Mesh *> meshes_by_name;

		MaskedBlockList<Scene3D, 256> scenes3d;
		HashMap<Span<utf8>, Scene3D *> scenes3d_by_name;
		HashMap<Scene3D::Node *, Mesh *> meshes_by_node;

		Mesh *create_mesh(tl::CommonMesh &mesh) {
			Mesh result = {};
			result.vertex_buffer = tg::create_vertex_buffer(
				as_bytes(mesh.vertices),
				{
					tg::Element_f32x3, // position
					tg::Element_f32x3, // normal
					tg::Element_f32x4, // color
					tg::Element_f32x2, // uv
				}
			);

			result.index_buffer = tg::create_index_buffer(as_bytes(mesh.indices), sizeof(u32));

			result.index_count = mesh.indices.size;

			result.positions.reserve(mesh.vertices.size);
			for (auto &vertex : mesh.vertices) {
				result.positions.add(vertex.position);
			}

			result.indices = copy(mesh.indices);

			auto pointer = &meshes.add();
			*pointer = result;
			return pointer;
		}

		Mesh *get(Span<utf8> path) {
			auto found = meshes_by_name.find(path);
			if (found) {
				return *found;
			} else {
				auto submesh_separator = find(path, u8':');
				if (submesh_separator) {
					auto scene_path   = Span<utf8>{path.data, submesh_separator};
					auto submesh_name = Span<utf8>{submesh_separator + 1, path.end()};

					auto scene_data = get_asset_data(scene_path);

					auto &scene = scenes3d_by_name.get_or_insert(scene_path);
					if (!scene) {
						scene = &scenes3d.add();
						auto parsed = parse_glb_from_memory(scene_data);
						//if (!parsed) {
						//	print(Print_error, "Failed to parse scene file '%'\n", scene_path);
						//	return 0;
						//}
						//*scene = parsed.value;
						*scene = parsed;
						scenes3d_by_name.get_or_insert(scene_path) = scene;
					}

					assert(scene);

					auto node = scene->get_node(submesh_name);
					if (!node) {
						print(Print_error, "Failed to get node '%' from scene '%'\n", submesh_name, scene_path);
						return 0;
					}

					auto &mesh = meshes_by_node.get_or_insert(node);
					if (mesh) {
						return mesh;
					}

					mesh = create_mesh(*node->mesh);


					mesh->name.reserve(scene_path.size + 1 + submesh_name.size);
					mesh->name.add(scene_path);
					mesh->name.add(':');
					mesh->name.add(submesh_name);
					meshes_by_name.get_or_insert(mesh->name) = mesh;
					return mesh;
				} else {
					auto scene_data = get_asset_data(path);
					auto parse_result = parse_glb_from_memory(scene_data);
					defer { free(parse_result); };

					if (parse_result.meshes.size == 0) {
						print(Print_error, "Failed to load mesh '%' because there is no submeshes in the file\n", path);
						return 0;
					}

					auto result = create_mesh(parse_result.meshes[0]);
					meshes_by_name.get_or_insert(path) = result;
					return result;
				}
			}

		}
	} meshes;
} assets;
