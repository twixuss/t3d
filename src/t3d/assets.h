#pragma once
#include "mesh.h"
#include <tl/masked_block_list.h>

struct Assets {
	Span<utf8> directory;
	HashMap<Span<utf8>, Span<u8>> asset_path_to_data;

	HashMap<Span<utf8>, Texture2D *> textures_2d_by_path;
	HashMap<Span<utf8>, TextureCube *> textures_cubes_by_path;

	MaskedBlockList<Mesh, 256> meshes;
	HashMap<Span<utf8>, Mesh *> meshes_by_name;

	MaskedBlockList<Scene3D, 256> scenes3d;
	HashMap<Span<utf8>, Scene3D *> scenes3d_by_name;
	HashMap<Scene3D::Node *, Mesh *> meshes_by_node;

	Span<u8> get_asset_data(Span<utf8> path);
	Texture2D *get_texture_2d(Span<utf8> path);
	TextureCube *get_texture_cube(Span<utf8> path);
	Mesh *create_mesh(tl::CommonMesh &mesh);

	Mesh *get_mesh(Span<utf8> path) {
		auto found = meshes_by_name.find(path);
		if (found) {
			return *found;
		} else {
			auto submesh_separator = find(path, u8':');
			if (submesh_separator) {
				auto scene_path   = Span<utf8>{path.data, submesh_separator};
				auto submesh_name = Span<utf8>{submesh_separator + 1, path.end()};

				auto scene_data = get_asset_data(scene_path);
				assert(scene_data.data);

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
};
