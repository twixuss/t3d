#include <t3d/common.h>
#include <source_location>
#include <tl/common.h>
#include <tl/bin2cpp.h>
tl::umm get_hash(struct ManipulatorStateKey const &);

#include <tl/masked_block_list.h>
#include <t3d/component.h>
#include <t3d/components/light.h>
#include <t3d/components/mesh_renderer.h>
#include <t3d/components/camera.h>
#include <t3d/editor/window.h>
#include <t3d/editor/scene_view.h>
#include <t3d/editor/hierarchy_view.h>
#include <t3d/editor/split_view.h>
#include <t3d/editor/property_view.h>
#include <t3d/editor/file_view.h>
#include <t3d/editor/tab_view.h>
#include <t3d/editor/input.h>
#include <t3d/serialize.h>
#include <t3d/assets.h>
#include <t3d/runtime.h>
#include <t3d/post_effects/bloom.h>
#include <t3d/post_effects/dither.h>
#include <t3d/post_effects/exposure.h>

#include <tl/process.h>
#include <tl/thread.h>
#include <tl/profiler.h>
#include <tl/cpu.h>
#include <tl/ram.h>
#include <tl/opengl.h>

#define NOMINMAX
#include <Windows.h>

#define c(name) { \
.init         = adapt_editor_window_init<name>, \
.get_min_size = editor_window_get_min_size<name>, \
.resize       = editor_window_resize<name>, \
.render       = editor_window_render<name>, \
.free         = editor_window_free<name>, \
.debug_print  = editor_window_debug_print<name>, \
.serialize    = editor_window_serialize<name>, \
.deserialize  = editor_window_deserialize<name>, \
.size         = sizeof(name), \
.alignment    = alignof(name), \
}
#define sep ,

EditorWindowMetadata editor_window_metadata[editor_window_type_count] = {
	ENUMERATE_WINDOWS
};


#undef sep
#undef c

u32 fps_counter;
u32 fps_counter_result;
f32 fps_timer;

Mesh *suzanne_mesh;
Mesh *floor_mesh;
Mesh *handle_sphere_mesh;
Mesh *handle_circle_mesh;
Mesh *handle_tangent_mesh;
Mesh *handle_axis_x_mesh;
Mesh *handle_axis_y_mesh;
Mesh *handle_axis_z_mesh;
Mesh *handle_arrow_x_mesh;
Mesh *handle_arrow_y_mesh;
Mesh *handle_arrow_z_mesh;
Mesh *handle_plane_x_mesh;
Mesh *handle_plane_y_mesh;
Mesh *handle_plane_z_mesh;

Span<utf8> editor_exe_path;
Span<utf8> editor_directory;
Span<utf8> editor_bin_directory;
Span<utf8> project_name;
Span<utf8> component_descs_getter_path;
Span<utf8> project_directory;

ListList<utf8> all_component_names;

HMODULE scripts_dll;
void (*scripts_dll_initialize_thread)();

void update_component_info(ComponentDesc const &desc) {
	scoped_allocator(default_allocator);

	auto found_uid = app->component_name_to_uid.find(desc.name);

	ComponentInfo *info;

	Uid uid;

	if (found_uid) {
		uid = found_uid.get_unchecked();
		print("Re-registered component '%' with uid '%'\n", desc.name, uid);

		auto found_info = app->component_infos.find(uid);
		assert(found_info);
		info = found_info.raw();

		assert(info->name == desc.name);

		if (desc.size != info->size || desc.alignment != info->alignment) {
			for (auto scene : app->scenes) {
				scene->component_storages.find(uid).get().reallocate(desc.size, desc.alignment);
			}
		}
	} else {
		uid = create_uid();
		print("Registered new component '%' with uid '%'\n", desc.name, uid);

		info = &app->component_infos.get_or_insert(uid);

		info->name.set(desc.name);

		app->component_name_to_uid.get_or_insert(info->name) = uid;
	}

	info->size = desc.size;
	info->alignment = desc.alignment;
	info->serialize          = desc.serialize         ;
	info->construct          = desc.construct         ;
	info->deserialize_binary = desc.deserialize_binary;
	info->deserialize_text   = desc.deserialize_text  ;
	info->draw_properties    = desc.draw_properties   ;
	info->free               = desc.free              ;
	info->init               = desc.init              ;
	info->start              = desc.start             ;
	info->update             = desc.update            ;
}

m4 local_to_world_position(v3f position, quaternion rotation, v3f scale) {
	return m4::translation(position) * (m4)rotation * m4::scale(scale);
}

m4 local_to_world_normal(quaternion rotation, v3f scale) {
	return (m4)rotation * m4::scale(1.0f / scale);
}

void render_scene(SceneView *view) {
	timed_function();

	auto &camera = *view->camera;
	auto &camera_entity = *view->camera_entity;

	render_camera(camera, camera_entity);

	app->tg->set_render_target(camera.source_target);
	app->tg->clear(camera.source_target, tg::ClearFlags_depth, {}, 1);
	app->tg->set_rasterizer({
		.depth_test = true,
		.depth_write = true,
		.depth_func = tg::Comparison_less,
	});
	app->tg->set_blend(tg::BlendFunction_add, tg::Blend_source_alpha, tg::Blend_one_minus_source_alpha);

	if (selection.kind == Selection_entity) {
		auto new_transform = manipulate_transform(selection.entity->position, selection.entity->rotation, selection.entity->scale, view->manipulator_kind);
		selection.entity->position = new_transform.position;
		selection.entity->rotation = new_transform.rotation;
		selection.entity->scale    = new_transform.scale;

		for (auto &request : manipulator_draw_requests) {
			v3f camera_to_handle_direction = normalize(request.position - camera_entity.position);
			app->tg->update_shader_constants(app->entity_constants, {
				.local_to_camera_matrix =
					camera.world_to_camera_matrix
					* m4::translation(camera_entity.position + camera_to_handle_direction)
					* (m4)request.rotation
					* m4::scale(request.size * dot(camera_to_handle_direction, camera_entity.rotation * v3f{0,0,-1})),
				.local_to_world_normal_matrix = local_to_world_normal(request.rotation, V3f(request.size * dot(camera_to_handle_direction, camera_entity.rotation * v3f{0,0,-1}))),
				.object_rotation_matrix = (m4)request.rotation,
			});
			app->tg->set_shader(app->handle_shader);
			app->tg->set_shader_constants(app->handle_constants, 0);

			u32 selected_element = request.highlighted_part_index;

			v3f to_camera = normalize(camera_entity.position - selection.entity->position);
			switch (request.kind) {
				case Manipulate_position: {
					app->tg->update_shader_constants(app->handle_constants, {.color = V3f(1), .selected = (f32)(selected_element != null_manipulator_part), .to_camera = to_camera});
					draw_mesh(handle_sphere_mesh);

					app->tg->update_shader_constants(app->handle_constants, {.color = V3f(1,0,0), .selected = (f32)(selected_element == 0), .to_camera = to_camera});
					draw_mesh(handle_axis_x_mesh);

					app->tg->update_shader_constants(app->handle_constants, {.color = V3f(0,1,0), .selected = (f32)(selected_element == 1), .to_camera = to_camera});
					draw_mesh(handle_axis_y_mesh);

					app->tg->update_shader_constants(app->handle_constants, {.color = V3f(0,0,1), .selected = (f32)(selected_element == 2), .to_camera = to_camera});
					draw_mesh(handle_axis_z_mesh);

					app->tg->update_shader_constants(app->handle_constants, {.color = V3f(1,0,0), .selected = (f32)(selected_element == 0), .to_camera = to_camera});
					draw_mesh(handle_arrow_x_mesh);

					app->tg->update_shader_constants(app->handle_constants, {.color = V3f(0,1,0), .selected = (f32)(selected_element == 1), .to_camera = to_camera});
					draw_mesh(handle_arrow_y_mesh);

					app->tg->update_shader_constants(app->handle_constants, {.color = V3f(0,0,1), .selected = (f32)(selected_element == 2), .to_camera = to_camera});
					draw_mesh(handle_arrow_z_mesh);

					app->tg->update_shader_constants(app->handle_constants, {.color = V3f(1,0,0), .selected = (f32)(selected_element == 3), .to_camera = to_camera});
					draw_mesh(handle_plane_x_mesh);

					app->tg->update_shader_constants(app->handle_constants, {.color = V3f(0,1,0), .selected = (f32)(selected_element == 4), .to_camera = to_camera});
					draw_mesh(handle_plane_y_mesh);

					app->tg->update_shader_constants(app->handle_constants, {.color = V3f(0,0,1), .selected = (f32)(selected_element == 5), .to_camera = to_camera});
					draw_mesh(handle_plane_z_mesh);
					break;
				}
				case Manipulate_rotation: {
					app->tg->update_shader_constants(app->handle_constants, {.matrix = m4::rotation_r_zxy(0,0,pi/2), .color = V3f(1,0,0), .selected = (f32)(selected_element == 0), .to_camera = to_camera, .is_rotation = 1});
					draw_mesh(handle_circle_mesh);

					app->tg->update_shader_constants(app->handle_constants, {.color = V3f(0,1,0), .selected = (f32)(selected_element == 1), .to_camera = to_camera, .is_rotation = 1});
					draw_mesh(handle_circle_mesh);

					app->tg->update_shader_constants(app->handle_constants, {.matrix = m4::rotation_r_zxy(pi/2,0,0), .color = V3f(0,0,1), .selected = (f32)(selected_element == 2), .to_camera = to_camera, .is_rotation = 1});
					draw_mesh(handle_circle_mesh);

					if (request.dragging) {
						quaternion rotation = quaternion_look(request.tangent.direction);
						v3f position = request.tangent.origin;
						app->tg->update_shader_constants(app->entity_constants, {
							.local_to_camera_matrix =
								camera.world_to_camera_matrix
								* m4::translation(camera_entity.position + camera_to_handle_direction)
								* m4::scale(request.size * dot(camera_to_handle_direction, camera_entity.rotation * v3f{0,0,-1})) * m4::translation(position) * (m4)rotation,
							.local_to_world_normal_matrix = local_to_world_normal(rotation, V3f(request.size * dot(camera_to_handle_direction, camera_entity.rotation * v3f{0,0,-1}))),
							.object_rotation_matrix = (m4)rotation,
						});
						app->tg->update_shader_constants(app->handle_constants, {.color = V3f(1,1,1), .selected = 1, .to_camera = to_camera});
						draw_mesh(handle_tangent_mesh);
					}

					break;
				}
				case Manipulate_scale: {
					app->tg->update_shader_constants(app->handle_constants, {.color = V3f(1), .selected = (f32)(selected_element != null_manipulator_part), .to_camera = to_camera});
					draw_mesh(handle_sphere_mesh);

					app->tg->update_shader_constants(app->handle_constants, {.matrix = m4::scale(request.scale.x, 1, 1), .color = V3f(1,0,0), .selected = (f32)(selected_element == 0), .to_camera = to_camera});
					draw_mesh(handle_axis_x_mesh);

					app->tg->update_shader_constants(app->handle_constants, {.matrix = m4::scale(1, request.scale.y, 1), .color = V3f(0,1,0), .selected = (f32)(selected_element == 1), .to_camera = to_camera});
					draw_mesh(handle_axis_y_mesh);

					app->tg->update_shader_constants(app->handle_constants, {.matrix = m4::scale(1, 1, request.scale.z), .color = V3f(0,0,1), .selected = (f32)(selected_element == 2), .to_camera = to_camera});
					draw_mesh(handle_axis_z_mesh);

					app->tg->update_shader_constants(app->handle_constants, {.matrix = m4::translation(0.8f*request.scale.x,0,0) * m4::scale(1.5f), .color = V3f(1,0,0), .selected = (f32)(selected_element == 0), .to_camera = to_camera});
					draw_mesh(handle_sphere_mesh);

					app->tg->update_shader_constants(app->handle_constants, {.matrix = m4::translation(0,0.8f*request.scale.y,0) * m4::scale(1.5f), .color = V3f(0,1,0), .selected = (f32)(selected_element == 1), .to_camera = to_camera});
					draw_mesh(handle_sphere_mesh);

					app->tg->update_shader_constants(app->handle_constants, {.matrix = m4::translation(0,0,0.8f*request.scale.z) * m4::scale(1.5f), .color = V3f(0,0,1), .selected = (f32)(selected_element == 2), .to_camera = to_camera});
					draw_mesh(handle_sphere_mesh);

					app->tg->update_shader_constants(app->handle_constants, {.color = V3f(1,0,0), .selected = (f32)(selected_element == 3), .to_camera = to_camera});
					draw_mesh(handle_plane_x_mesh);

					app->tg->update_shader_constants(app->handle_constants, {.color = V3f(0,1,0), .selected = (f32)(selected_element == 4), .to_camera = to_camera});
					draw_mesh(handle_plane_y_mesh);

					app->tg->update_shader_constants(app->handle_constants, {.color = V3f(0,0,1), .selected = (f32)(selected_element == 5), .to_camera = to_camera});
					draw_mesh(handle_plane_z_mesh);
					break;
				}
			}
		}
		manipulator_draw_requests.clear();
	}

	auto scene = app->current_scene;

	scene->for_each_component<Camera>([&](Camera &camera) {
		auto &camera_entity = camera.entity();
		if (!is_editor_entity(camera_entity)) {
			m4 projection_matrix = m4::perspective_right_handed((f32)view->viewport.size().x / view->viewport.size().y, camera.fov, camera.near_plane, camera.far_plane);
			m4 translation_matrix = m4::translation(-camera_entity.position);
			m4 rotation_matrix = transpose((m4)camera_entity.rotation);
			auto world_to_camera_matrix = projection_matrix * rotation_matrix * translation_matrix;


			m4 camera_to_world_matrix = inverse(world_to_camera_matrix);

			v3f points[] {
				v3f{-1,-1,-1},
				v3f{-1,-1, 1},
				v3f{-1, 1,-1},
				v3f{-1, 1, 1},
				v3f{ 1,-1,-1},
				v3f{ 1,-1, 1},
				v3f{ 1, 1,-1},
				v3f{ 1, 1, 1},
			};

			for (auto &point : points) {
				v4f p = camera_to_world_matrix * V4f(point, 1);
				point = p.xyz / p.w;
			}

			debug_line(points[0], points[1]);
			debug_line(points[2], points[3]);
			debug_line(points[4], points[5]);
			debug_line(points[6], points[7]);

			debug_line(points[0], points[2]);
			debug_line(points[1], points[3]);
			debug_line(points[4], points[6]);
			debug_line(points[5], points[7]);

			debug_line(points[0], points[4]);
			debug_line(points[1], points[5]);
			debug_line(points[2], points[6]);
			debug_line(points[3], points[7]);
		}
	});

	gui_image(camera.source_target->color);

	debug_draw_lines();
}

void add_files_recursive(ListList<utf8> &result, Span<pathchar> directory) {
	auto items = get_items_in_directory(directory);
	for (auto &item : items) {
		auto path16 = concatenate(directory, '/', item.name);
		if (item.kind == FileItem_directory) {
			add_files_recursive(result, path16);
		} else {
			result.add(to_utf8(path16));
		}
	}
}

auto query_performance_counter() {
	LARGE_INTEGER r;
	QueryPerformanceCounter(&r);
	return r.QuadPart;
}

template <class Fn>
bool invoke_msvc(Span<utf8> arguments, Fn &&what_to_do_while_compiling) {
	auto prev_allocator = current_allocator;
	scoped_allocator(temporary_allocator);

	create_directory(format(u8"%build/"s, editor_directory));

	constexpr auto cl_path = "C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\Community\\VC\\Tools\\MSVC\\14.29.30037\\bin\\Hostx64\\x64\\cl.exe"s;

	StringBuilder bat_builder;
	append(bat_builder, u8R"(
@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
cl )");
	append_format(bat_builder, "/Fd\"%temp/%.pdb\" ", editor_directory, query_performance_counter());

	append(bat_builder, arguments);
	//append_format(bat_builder, " | \"%bin/stdin_duplicator.exe\" \"stdout\" \"%build/build_log.txt\"", editor_directory, editor_directory);

	auto bat_path = format(u8"%build/build.bat"s, editor_directory);

	write_entire_file(bat_path, as_bytes(to_string(bat_builder)));

	auto process = start_process(bat_path);

	if (!is_valid(process)) {
		print(Print_error, "Cannot execute file '%'\n", bat_path);
		return false;
	}

	defer { free(process); };

	print("cl %\n", arguments);

	{
		scoped_allocator(prev_allocator);
		what_to_do_while_compiling();
	}


	StringBuilder log_builder;
	while (1) {
		u8 buf[256];
		auto bytes_read = process.standard_out->read(array_as_span(buf));

		if (bytes_read == 0)
			break;

		auto string = Span((utf8 *)buf, bytes_read);
		append(log_builder, string);
		print(string);
	}
	write_entire_file(format("%build/build_log.txt", editor_directory), as_bytes(to_string(log_builder)));

	wait(process);
	auto exit_code = get_exit_code(process);
	if (exit_code != 0) {
		print(Print_error, "Build command failed\n");
		print(as_utf8(read_entire_file(tl_file_string("build_log.txt"))));
		return false;
	}

	print("Build succeeded\n");
	return true;
}

bool invoke_msvc(Span<utf8> arguments) {
	return invoke_msvc(arguments, []{});
}

ListList<utf8> project_h_files;
ListList<utf8> project_cpp_files;
ListList<utf8> editor_cpp_files;

void update_scripts_paths() {
	project_h_files.clear();
	project_cpp_files.clear();

	project_h_files.make_relative();
	project_cpp_files.make_relative();

	for_each_file_recursive(app->assets.directory, [] (Span<utf8> item) {
		if (ends_with(item, u8".h"s)) {
			project_h_files.add(item);
		}
		if (ends_with(item, u8".cpp"s)) {
			project_cpp_files.add(item);
		}
	});

	project_h_files.make_absolute();
	project_cpp_files.make_absolute();
}

#include <ImageHlp.h>
#pragma comment(lib, "imagehlp.lib")
#pragma comment(lib, "dbghelp.lib")

ListList<ascii> get_exported_functions_in_dll(Span<utf8> dll_path) {
    DWORD *name_rvas = 0;
    _IMAGE_EXPORT_DIRECTORY *ied;
    ULONG dir_size;
    _LOADED_IMAGE loaded_image;

	ListList<ascii> result;
    if (MapAndLoad((ascii *)temporary_null_terminate(dll_path).data, NULL, &loaded_image, TRUE, TRUE)) {

        ied = (_IMAGE_EXPORT_DIRECTORY *)ImageDirectoryEntryToData(loaded_image.MappedAddress, false, IMAGE_DIRECTORY_ENTRY_EXPORT, &dir_size);

		if (ied != NULL) {
            name_rvas = (DWORD *)ImageRvaToVa(loaded_image.FileHeader, loaded_image.MappedAddress, ied->AddressOfNames, NULL);

            for (DWORD i = 0; i < ied->NumberOfNames; i++) {
				auto span = as_span((ascii *)ImageRvaToVa(loaded_image.FileHeader, loaded_image.MappedAddress, name_rvas[i], NULL));
				span.size += 1; // include null terminator for future GetProcAddress calls
				result.add(span);
            }
        }
        UnMapAndLoad(&loaded_image);
    }
	result.make_absolute();
	return result;
}


Span<ascii> include_dirs[] = {
	"src"s,
	"dep/tl/include"s,
	"dep/freetype/include"s,
	"dep/stb"s,
	"dep/tgraphics/include"s,
};

Span<ascii> lib_dirs[] = {
	"dep/freetype/win64"s,
};

void clear_build_directory() {
	auto items = get_items_in_directory(to_pathchars(format(u8"%build"s, editor_directory), true));
	for (auto item : items) {
		delete_file(to_pathchars(format(u8"%build/%"s, editor_directory, item.name)));
	}
}

void append_editor_cpp_or_obj_files(StringBuilder &builder) {
	for (auto cpp_path : editor_cpp_files) {
		auto name = parse_path(cpp_path).name;
		if (name == u8"main_editor"s ||
			name == u8"main_runtime"s ||
			name == u8"main"s)
		{
			continue;
		}

		auto obj_path = tformat("%data/obj/%.obj", editor_directory, name);
		if (file_exists(obj_path)) {
			append(builder, obj_path);
			append(builder, ' ');
		} else {
			append(builder, cpp_path);
			append(builder, ' ');
		}
	}
}

void build_executable() {
	scoped_allocator(temporary_allocator);

	auto build_assets = [&] {
		StringBuilder asset_builder;

		ListList<utf8> asset_paths;
		add_files_recursive(asset_paths, to_pathchars(app->assets.directory));
		asset_paths.make_absolute();

		for (auto full_path : asset_paths) {
			auto path = full_path.subspan(app->assets.directory.size + 1, full_path.size - app->assets.directory.size - 1);
			print("asset %\n", path);
			append_bytes(asset_builder, (u32)path.size);
			append_bytes(asset_builder, path);

			auto data = read_entire_file(to_pathchars(full_path));
			append_bytes(asset_builder, (u32)data.size);
			append_bytes(asset_builder, as_span(data));
		}

		auto data_path = format(u8"%build/data.bin", project_directory);
		create_directory(parse_path(data_path).directory);
		auto data_file = open_file(data_path, {.write = true});
		defer { close(data_file); };

		DataHeader header;

		set_cursor(data_file, sizeof(header), File_begin);

		auto asset_data = as_bytes(to_string(asset_builder));
		header.asset_offset = get_cursor(data_file);
		header.asset_size = asset_data.size;
		write(data_file, asset_data);


		HashMap<Uid, Uid> uid_remap;
		u64 uid_counter = 0;
		for (auto name : all_component_names) {
			uid_remap.get_or_insert(component_name_to_uid(name)).value = uid_counter++;
		}

		auto scene_data = serialize_scene_binary(app->current_scene, uid_remap);
		header.scene_offset = get_cursor(data_file);
		header.scene_size = scene_data.size;
		write(data_file, scene_data);

		set_cursor(data_file, 0, File_begin);
		write(data_file, value_as_bytes(header));
	};


	// Generate source
	{
		StringBuilder builder;
		append(builder, u8R"(#pragma once
#include <t3d/component.h>
#include <t3d/serialize.h>
#include <t3d/app.h>

#pragma comment(lib, "freetype.lib")

static u64 uid_generator = 0;

void update_component_info(ComponentDesc const &desc) {
	scoped_allocator(default_allocator);

	assert(!app->component_name_to_uid.find(desc.name));

	Uid uid;
	uid.value = uid_generator++;

	assert(!app->component_infos.find(uid));

	print("Registered new component '%' with uid '%'\n", desc.name, uid);

	auto &info = app->component_infos.get_or_insert(uid);

	info.name.set(desc.name);

	app->component_name_to_uid.get_or_insert(info.name) = uid;

	info.size      = desc.size;
	info.alignment = desc.alignment;
	info.serialize          = desc.serialize;
	info.construct          = desc.construct;
	info.deserialize_binary = desc.deserialize_binary;
	info.deserialize_text   = desc.deserialize_text;
	info.draw_properties    = desc.draw_properties;
	info.free               = desc.free;
	info.init               = desc.init;
	info.start              = desc.start;
	info.update             = desc.update;
}

)"s);


		for (auto component_name : all_component_names) {
			append_format(builder, u8"extern \"C\" ComponentDesc t3dcd%();\n", component_name);
		}
		append(builder, u8"extern \"C\" void t3d_get_component_descs(List<ComponentDesc> &descs) {\n");
		for (auto component_name : all_component_names) {
			append_format(builder, u8"	descs.add(t3dcd%());\n", component_name);
		}
		append(builder, u8"}"s);


		create_directory(format("%build", editor_directory));
		write_entire_file(to_pathchars(component_descs_getter_path), as_bytes(to_string(builder)));
	}

	StringBuilder builder;

	append_editor_cpp_or_obj_files(builder);

	append_format(builder, "% ", component_descs_getter_path);

	append_format(builder, u8"%src/t3d/main_runtime.cpp "s, editor_directory);

	for (auto cpp_file : project_cpp_files) {
		append_format(builder, "% ", cpp_file);
	}

	for (auto inc : include_dirs) {
		append_format(builder, "/I\"%%\" ", editor_directory, inc);
	}

	create_directory(format(u8"%build", project_directory));
	append_format(builder, u8"/Zi /MTd /std:c++latest /D\"BUILD_DEBUG=0\" /link /out:%build/%.exe "s, project_directory, project_name);

	for (auto lib : lib_dirs) {
		append_format(builder, "/LIBPATH:\"%..\\%\" ", editor_bin_directory, lib);
	}

	if (!invoke_msvc((List<utf8>)to_string(builder), build_assets)) {
		return;
	}
}


Span<utf8> scripts_dll_path;

void recompile_all_scripts() {
	scripts_dll_path = format(u8"%build/scripts%.dll"s, editor_directory, query_performance_counter());

	update_scripts_paths();



	// compile dll
	{
		StringBuilder builder;

		for (auto cpp_file : project_cpp_files) {
			append_format(builder, "% ", cpp_file);
		}

		append_editor_cpp_or_obj_files(builder);

		for (auto inc : include_dirs) {
			append_format(builder, "/I\"%%\" ", editor_directory, inc);
		}

		append_format(builder, "/LD /Zi /MTd /std:c++latest /D\"BUILD_DEBUG=0\" /link /out:\"%\" ", scripts_dll_path);


		for (auto lib : lib_dirs) {
			append_format(builder, "/LIBPATH:\"%..\\%\" ", editor_bin_directory, lib);
		}
		assert(invoke_msvc(as_utf8(to_string(builder))));
	}

}

void reload_all_scripts(bool recompile) {
	scoped_allocator(temporary_allocator);


	//
	// Serialize components that are about to be rebuilt
	//
	// `app->current_scene` will be null on first call because scene is not loaded yet, so do nothing
	//
	List<ComponentIndex> components_to_update;
	StringBuilder builder;
	if (app->current_scene) {
		components_to_update.allocator = temporary_allocator;
		for_each(app->current_scene->entities, [&](Entity &entity) {
			for (auto &component : entity.components) {
				auto found_info = app->component_infos.find(component.type_uid);
				assert(found_info);
				auto &info = *found_info;
				info.serialize(builder, app->current_scene->get_component_data(component), false);

				append(builder, '}'); // deserialier need this to finish parsing

				components_to_update.add(component);
			}
		});
	}

	//
	// Reload dll
	//
	if (scripts_dll) {
		FreeLibrary(scripts_dll);
	}

	if (recompile) {
		recompile_all_scripts();
	}

	scripts_dll = LoadLibraryW(with(temporary_allocator, (wchar *)to_pathchars(scripts_dll_path, true).data));
	scripts_dll_initialize_thread = ((void (*)())GetProcAddress(scripts_dll, "initialize_thread"));
	scripts_dll_initialize_thread();

	set_module_shared(scripts_dll);

	//
	// Register all components in dll
	//
	auto exported_funcs = get_exported_functions_in_dll(scripts_dll_path);

	List<ComponentDesc> descs;
	descs.allocator = app->allocator;
	all_component_names.make_relative();
	all_component_names.clear();
	for (auto func_name : exported_funcs) {
		auto prefix = u8"t3dcd"s;
		if (starts_with(func_name, prefix)) {
			all_component_names.add(as_utf8(func_name.subspan(prefix.size, func_name.size - 1 - prefix.size)));
			using GetComponentDesc = ComponentDesc (*)();
			descs.add(((GetComponentDesc)GetProcAddress(scripts_dll, func_name.data))());
		}
	}
	all_component_names.make_absolute();

	for (auto &desc : descs) {
		update_component_info(desc);
	}

	if (app->current_scene) {
		//
		// Deserialize components that were rebuilt
		//
		auto string = as_utf8(to_string(builder));
		auto tokens = parse_tokens(string).value();
		auto t = tokens.data;
		for (auto &component : components_to_update) {
			auto found_info = app->component_infos.find(component.type_uid);
			assert(found_info);
			auto &info = *found_info;
			auto data = app->current_scene->get_component_data(component);
			info.construct(data);
			info.deserialize_text(t, tokens.end(), data);
		}
	}
}

struct Task {
	struct State {
		bool finished = false;
	};

	Allocator allocator = current_allocator;
	State *state = 0;

};

bool started(Task task) {
	return task.state != 0;
}
bool finished(Task task) {
	return task.state->finished;
}

void wait(Task task) {
	loop_until([&]{ return task.state->finished; });
}

Printer log_printer;

template <class Fn, class ...Args>
Task async(Fn &&fn, Args &&...args) {
	Task task;
	task.state = task.allocator.allocate<Task::State>();
	create_thread([task_state = task.state, fn = std::forward<Fn>(fn), ...args = std::forward<Args>(args)] {
		current_printer = log_printer;
		fn(args...);
		task_state->finished = true;
	});
	return task;
}

void set_project_directory(Span<utf8> directory) {
	project_directory = directory;
	project_name = parse_path(directory).name;
	app->assets.directory = format(u8"%assets/"s, project_directory);
}

void compile_project() {
	recompile_all_scripts();
	reload_all_scripts(false);
}
void load_project() {
	auto create_default_scene = [&]() {
		auto scene = default_allocator.allocate<Scene>();

		auto &suzanne = scene->create_entity("suzan\"ne");
		suzanne.rotation = quaternion_from_euler(radians(v3f{-54.7, 45, 0}));
		{
			auto &mr = add_component<MeshRenderer>(suzanne);
			mr.mesh = app->assets.get_mesh(u8"scene.glb:Suzanne"s);
			mr.material = &app->surface_material;
			mr.lightmap = app->assets.get_texture_2d(u8"suzanne_lightmap.png"s);
		}
		selection.set(&suzanne);

		auto &floor = scene->create_entity("floor");
		{
			auto &mr = add_component<MeshRenderer>(floor);
			mr.mesh = app->assets.get_mesh(u8"scene.glb:Room"s);
			mr.material = &app->surface_material;
			mr.lightmap = app->assets.get_texture_2d(u8"floor_lightmap.png"s);
		}

		auto light_texture = app->assets.get_texture_2d(u8"spotlight_mask.png"s);

		{
			auto &light = scene->create_entity("light1");
			light.position = {0,2,6};
			//light.rotation = quaternion_from_euler(-pi/10,0,pi/6);
			light.rotation = quaternion_from_euler(0,0,0);
			add_component<Light>(light).mask = light_texture;
		}

		{
			auto &light = scene->create_entity("light2");
			light.position = {6,2,-6};
			light.rotation = quaternion_from_euler(-pi/10,pi*0.75,0);
			add_component<Light>(light).mask = light_texture;
		}

		auto &camera_entity = scene->create_entity("main camera");
		camera_entity.position = {0, 0, 4};

		auto &camera = add_component<Camera>(camera_entity);

		auto &exposure = camera.add_post_effect<Exposure>();
		exposure.auto_adjustment = false;
		exposure.exposure = 1.5;
		exposure.limit_min = 1.0f / 16;
		exposure.limit_max = 1024;
		exposure.approach_kind = Exposure::Approach_log_lerp;
		exposure.mask_kind = Exposure::Mask_one;
		exposure.mask_radius = 1;

		auto &bloom = camera.add_post_effect<Bloom>();

		auto &dither = camera.add_post_effect<Dither>();

		return scene;
	};

	//if (!deserialize_window_layout())
		editor->main_window = create_split_view(
			create_split_view(
				create_tab_view(create_file_view()),
				create_tab_view(create_scene_view()),
				{ .split_t = 0 }
			),
			create_split_view(
				create_tab_view(create_hierarchy_view()),
				create_tab_view(create_property_view()),
				{ .horizontal = true }
			),
			{ .split_t = 1 }
		);

	editor->main_window->resize(aabb_min_max({}, (v2s)app->window->client_size));

	auto scene = deserialize_scene_text(u8"test.scene"s);
	app->current_scene = scene ? scene : create_default_scene();
	app->scenes.add(app->current_scene);

	app->window->min_window_size = client_size_to_window_size(*app->window, editor->main_window->get_min_size());
	app->sky_box_texture = app->assets.get_texture_cube(u8"sky.cubemap"s);
}

struct RecentProject {
	List<utf8> path;
	Date date;
};
List<RecentProject> recent_projects;
bool initted_recent_projects;

template <class T>
List<Span<T>> split_by_any(Span<T> what, Span<T> by) {
	List<Span<T>> result;

	T *start = what.data;

	for (auto w = what.data; w != what.end(); ++w) {
		for (auto &by_it : by) {
			if (*w == by_it) {
				result.add(Span(start, w));
				start = w + 1;
				break;
			}
		}
	}

	result.add(Span(start, what.end()));

	return result;
}

void split_test(Span<char> a, Span<char> b) {
	print("split_by_any(\"%\", \"%\") = %\n", a, b, split_by_any(a, b));
}

List<utf8> recent_list_path;

void init_recent_projects() {
	recent_projects.allocator = default_allocator;
	recent_list_path = format(u8"%user/recent_projects", editor_directory);

	Span<u8> recent_list;
	if (!file_exists(recent_list_path)) {
		create_directory(parent_directory(recent_list_path));
		StringBuilder builder;
		builder.allocator = temporary_allocator;

		auto default_path = with(temporary_allocator, replace(as_span(format(u8"%example/", editor_directory)), u8'\\', u8'/'));

		append_bytes(builder, (u32)default_path.size);
		append_bytes(builder, default_path);

		append_bytes(builder, get_date());

		recent_list = (List<u8>)to_string(builder);
		write_entire_file(recent_list_path, as_bytes(recent_list));
	} else {
		recent_list = with(temporary_allocator, read_entire_file(recent_list_path));
	}

	u8 *cursor = recent_list.data;
	u8 *end = recent_list.end();

	while (cursor != end) {
		RecentProject rp;

		u32 path_size;
		if (cursor + sizeof(u32) > end) {
			print(Print_error, "Recent projects list file is corrupted ('%' is past the end of buffer)", "path_size");
			return;
		}
		path_size = *(u32 *)cursor;
		cursor += sizeof(u32);

		if (cursor + path_size > end) {
			print(Print_error, "Recent projects list file is corrupted ('%' is past the end of buffer)", "path");
			return;
		}
		rp.path.set(Span((utf8 *)cursor, path_size));
		cursor += path_size;


		if (cursor + sizeof(Date) > end) {
			print(Print_error, "Recent projects list file is corrupted ('%' is past the end of buffer)", "date");
			return;
		}
		rp.date = *(Date *)cursor;
		cursor += sizeof(Date);

		recent_projects.add(rp);
	}
}

void serialize_recent_projects() {
	scoped_allocator(temporary_allocator);
	StringBuilder builder;

	for (auto &p : recent_projects) {
		append_bytes(builder, (u32)p.path.size);
		append_bytes(builder, p.path);
		append_bytes(builder, p.date);
	}

	write_entire_file(recent_list_path, as_bytes(to_string(builder)));
}

List<utf8> project_directory_text_field;
Task compile_project_task;
bool show_editor;
enum ProjectSelectionState {
	ProjectSelection_recent_list,
	ProjectSelection_new,
};
ProjectSelectionState project_selection_state;

void directory_field(List<utf8> &path, umm id = 0, std::source_location location = std::source_location::current()) {
	text_field(path, id, location);
}

void draw_project_selection() {
	if (!initted_recent_projects) {
		initted_recent_projects = true;
		init_recent_projects();
	}

	label(u8"Test label\nPrivet\nHello"s, 24);

	if (started(compile_project_task)) {
		push_label_theme {
			editor->label_theme.color.w = map<f32>(pow2(map<f32>(tl::sin(app->time*tau), -1, 1, 0, 1)), 1, 0, 0.25, 1);
			label(u8"Loading...", 64, {.align = Align_center});
		}

		if (finished(compile_project_task)) {
			show_editor = true;
		}
	} else {
		s32 const margin = 16;
		s32 const spacing = 40;
		s32 const font_size = 14;
		v2s const size = {640, 32};
		v2s const offset = {0, recent_projects.size * 32 / 2};
		switch (project_selection_state) {
			case ProjectSelection_new: {
				{
					tg::Viewport v;
					v.min = editor->current_viewport.center() - size / 2 + offset;
					v.max = editor->current_viewport.center() + size / 2 + offset;
					v.min.x -= margin;
					v.max.y += margin;

					v.min.y -= 3 * spacing + margin;
					v.max.x += margin;

					push_viewport(v) {
						gui_panel(middle_color);
					}
				}

				tg::Viewport v;
				v.min = editor->current_viewport.center() - size / 2 + offset;
				v.max = editor->current_viewport.center() + size / 2 + offset;
				push_viewport(v) {
					if (button(u8"Back"s)) {
						project_selection_state = ProjectSelection_recent_list;
					}
				}

				v.min.y -= spacing;
				v.max.y -= spacing;

				push_viewport(v) {
					directory_field(project_directory_text_field);
				}

				break;
			}
			case ProjectSelection_recent_list: {
				{
					tg::Viewport v;
					v.min = editor->current_viewport.center() - size / 2 + offset;
					v.max = editor->current_viewport.center() + size / 2 + offset;
					v.min.x -= margin;
					v.max.y += margin;

					v.min.y -= recent_projects.size * spacing + margin;
					v.max.x += margin;

					push_viewport(v) {
						gui_panel(middle_color);
					}
				}

				tg::Viewport v;
				v.min = editor->current_viewport.center() - size / 2 + offset;
				v.max = editor->current_viewport.center() + size / 2 + offset;
				push_viewport(v) {
					if (button(u8"Create new"s)) {
						project_selection_state = ProjectSelection_new;
					}
				}

				push_button_theme {
					editor->button_theme.font_size = font_size;

					u32 id = 0;
					RecentProject *selected_project = 0;
					for (auto &p : recent_projects) {
						v.min.y -= spacing;
						v.max.y -= spacing;

						push_viewport(v) {
							if (button(id)) {
								assert(p.path.back() == '\\' || p.path.back() == '/');
								set_project_directory(p.path);
								compile_project_task = async(compile_project);

								selected_project = &p;
							}

							label(parse_path(p.path).name, 20, {.align = Align_center});
							push_label_theme {
								editor->label_theme.color.w = 0.5f;
								label(p.path, font_size, {.align = Align_left});
								label(to_string(p.date), font_size, {.align = Align_right});
							}
						}

						++id;
					}

					if (selected_project) {
						selected_project->date = get_date();
						recent_projects.move_at(selected_project, 0);
						serialize_recent_projects();
					}
				}
				break;
			}
		}
	}

	if (show_editor) {
		scripts_dll_initialize_thread();
		load_project();
	}
}

void draw_editor() {
	if (app->did_resize) {
		editor->main_window->resize({.min = {}, .max = (v2s)app->window->client_size});
	}

	if (key_down(Key_f2, {.anywhere = true})) {
		for_each(app->current_scene->entities, [](Entity &e) {
			print("name: %, index: %, flags: %, position: %, rotation: %\n", e.name, get_entity_index(e), e.flags, e.position, degrees(to_euler_angles(e.rotation)));
			for (auto &c : e.components) {
				print("\tparent: %, type: % (%), index: %\n", c.entity_index, c.type_uid, get_component_info(c.type_uid).name, c.storage_index);
			}
		});
	}


	if (key_down(Key_f6, {.anywhere = true})) {
		build_executable();
	}
	app->window->min_window_size = client_size_to_window_size(*app->window, editor->main_window->get_min_size());

	timed_block("frame"s);

	runtime_render();

	{
		timed_block("editor->main_window->render()"s);
		editor->main_window->render();
	}

	switch (editor->drag_and_drop_kind) {
		case DragAndDrop_file: {
			auto texture = app->assets.get_texture_2d(as_utf8(editor->drag_and_drop_data));
			if (texture) {
				aabb<v2s> thumbnail_viewport;
				thumbnail_viewport.min = thumbnail_viewport.max = app->current_mouse_position;
				thumbnail_viewport.max.x += 128;
				thumbnail_viewport.min.y -= 128;
				push_viewport(thumbnail_viewport) {
					gui_image(texture);
				}
			}
			break;
		}
		case DragAndDrop_tab: {
			auto tab_info = *(DragDropTabInfo *)editor->drag_and_drop_data.data;
			auto tab = tab_info.tab_view->tabs[tab_info.tab_index];

			auto font = get_font_at_size(app->font_collection, font_size);
			ensure_all_chars_present(tab.window->name, font);
			auto placed_chars = with(temporary_allocator, place_text(tab.window->name, font));

			tg::Viewport tab_viewport;
			tab_viewport.min = tab_viewport.max = app->current_mouse_position;

			tab_viewport.min.y -= TabView::tab_height;
			tab_viewport.max.x = tab_viewport.min.x + placed_chars.back().position.max.x + 4;

			push_viewport(tab_viewport) {
				gui_panel({.1,.1,.1,1});

				label(tab.window->name, font_size, {.position = {2, 0}});
				//label(placed_chars, font, {.position = {2, 0}}, 0, std::source_location::current(), V4f(1));
			}
			break;
		}
	}

	if (drag_and_dropping()) {
		if (editor->key_state[256].state & KeyState_up) {
			editor->drag_and_drop_kind = DragAndDrop_none;
			unlock_input_nocheck();
		}
	}

	debug_frame();

	bool debug_print_editor_window_hierarchy = app->frame_index == 0;
	for (auto &move : tab_moves) {
		auto from = move.from;
		auto tab_index = move.tab_index;
		auto to = move.to;
		auto direction = move.direction;

		auto tab = from->tabs[tab_index];

		auto remove_tab_view = [&]() {
			auto split_view = (SplitView *)from->parent;
			assert(split_view);
			assert(split_view->kind == EditorWindow_split_view);

			EditorWindow *what_is_left = split_view->get_other_part(from);

			what_is_left->parent = split_view->parent;

			if (split_view->parent) {
				switch (split_view->parent->kind) {
					case EditorWindow_split_view: {
						auto parent_view = (SplitView *)split_view->parent;
						parent_view->get_part(split_view) = what_is_left;
						parent_view->resize(parent_view->viewport);
						break;
					}
					default: {
						invalid_code_path();
						break;
					}
				}
			} else {
				auto main_window_viewport = editor->main_window->viewport;
				editor->main_window = what_is_left;
				editor->main_window->resize(main_window_viewport);
			}
		};

		if (direction == (u32)-1) {
			if (!tab.window->parent->parent)
				return;

			to->tabs.add(tab);
			from->tabs.erase_at(tab_index);
			if (tab_index < from->selected_tab) {
				--from->selected_tab;
			}
			from->selected_tab = min(from->selected_tab, from->tabs.size - 1);

			if (from->tabs.size == 0) {
				remove_tab_view();
			}
		} else {
			bool horizontal = (direction & 1);
			bool swap_parts = direction >= 2;

			TabView *new_tab_view;
			if (from->tabs.size == 1) {
				new_tab_view = from;

				auto from_parent = (SplitView *)from->parent;
				assert(from_parent);
				assert(from_parent->kind == EditorWindow_split_view);
				auto what_is_left = from_parent->get_other_part(from);
				auto from_parent_parent = (SplitView *)from_parent->parent;
				assert(from_parent_parent);
				assert(from_parent_parent->kind == EditorWindow_split_view);
				from_parent_parent->replace_child(from_parent, what_is_left);
			} else {
				if (tab_index < from->selected_tab) {
					--from->selected_tab;
				}
				from->tabs.erase_at(tab_index);
				from->selected_tab = min(from->selected_tab, from->tabs.size - 1);
				if (from->tabs.size == 0) {
					remove_tab_view();
				}
				new_tab_view = create_tab_view(tab.window);
			}

			auto left = to;
			auto right = new_tab_view;
			if (swap_parts) {
				swap(left, right);
			}

			if (to->parent) {
				auto to_parent = (SplitView *)to->parent;
				assert(to_parent->kind == EditorWindow_split_view);

				to_parent->replace_child(to, create_split_view(left, right, {.split_t = 0.5f, .horizontal = horizontal}));
			} else {
				assert(to == editor->main_window);
				editor->main_window = create_split_view(left, right, {.split_t = 0.5f, .horizontal = horizontal});
			}

			tg::Viewport window_viewport = {};
			window_viewport.max = (v2s)max(editor->main_window->get_min_size(), app->window->client_size);
			editor->main_window->resize(window_viewport);
			resize(*app->window, (v2u)window_viewport.max);

			//to_parent->resize(to_parent->viewport);
		}

		//split_view->free();

		debug_print_editor_window_hierarchy = true;
	}
	tab_moves.clear();


	if (debug_print_editor_window_hierarchy || (editor->key_state[Key_f4].state & KeyState_down)) {
		debug_print_editor_window_hierarchy = false;

		editor->main_window->debug_print();
	}

}

void run() {
	allocate_app();
	allocate(editor);
	editor->scene = default_allocator.allocate<Scene>();

	app->is_editor = true;
	editor->assets.directory = format(u8"%data/"s, editor_directory);
	construct(manipulator_draw_requests);
	construct(manipulator_states);
	construct(debug_lines);
	construct(tab_moves);

	CreateWindowInfo info;
	info.on_create = [](Window &window) {
		app->window = &window;

		runtime_init();

		app->tg->set_scissor(window.client_size);

		init_font();

		debug_lines_vertex_buffer = app->tg->create_vertex_buffer(
			{},
			{
				tg::Element_f32x3, // position
				tg::Element_f32x3, // color
			}
		);

		app->tg->set_vsync(true);

		handle_sphere_mesh  = editor->assets.get_mesh(u8"handle.glb:Sphere"s);
		handle_circle_mesh  = editor->assets.get_mesh(u8"handle.glb:Circle"s);
		handle_tangent_mesh = editor->assets.get_mesh(u8"handle.glb:Tangent"s);
		handle_axis_x_mesh  = editor->assets.get_mesh(u8"handle.glb:AxisX"s );
		handle_axis_y_mesh  = editor->assets.get_mesh(u8"handle.glb:AxisY"s );
		handle_axis_z_mesh  = editor->assets.get_mesh(u8"handle.glb:AxisZ"s );
		handle_arrow_x_mesh = editor->assets.get_mesh(u8"handle.glb:ArrowX"s );
		handle_arrow_y_mesh = editor->assets.get_mesh(u8"handle.glb:ArrowY"s );
		handle_arrow_z_mesh = editor->assets.get_mesh(u8"handle.glb:ArrowZ"s );
		handle_plane_x_mesh = editor->assets.get_mesh(u8"handle.glb:PlaneX"s);
		handle_plane_y_mesh = editor->assets.get_mesh(u8"handle.glb:PlaneY"s);
		handle_plane_z_mesh = editor->assets.get_mesh(u8"handle.glb:PlaneZ"s);

	};
	info.on_draw = [](Window &window) {
		app->tg->draw_call_count = 0;
		app->tg->clear(app->tg->back_buffer, tg::ClearFlags_color, background_color, {});

		static v2u old_window_size;
		app->did_resize = false;
		if (any_true(old_window_size != app->window->client_size)) {
			old_window_size = app->window->client_size;
			app->tg->on_window_resize(app->window->client_size);
			app->did_resize = true;
		}

		editor->current_viewport = editor->current_scissor = {
			.min = {},
			.max = (v2s)app->window->client_size,
		};
		app->tg->set_viewport(editor->current_viewport);
		app->tg->set_scissor(editor->current_scissor);

		app->current_mouse_position = {app->window->mouse_position.x, (s32)app->window->client_size.y - app->window->mouse_position.y};

		if (key_down(Key_f1, {.anywhere = true})) {
			Profiler::enabled = true;
			Profiler::reset();
		}
		defer {
			if (Profiler::enabled) {
				Profiler::enabled = false;
				write_entire_file(tl_file_string("update.tmd"s), Profiler::output_for_timed());
			}
		};

		app->current_cursor = Cursor_default;

		gui_begin_frame();

		if (show_editor) {
			draw_editor();
		} else {
			draw_project_selection();
		}

		if (editor->should_unlock_input) {
			editor->should_unlock_input = false;
			editor->input_is_locked = false;
			editor->input_locker = 0;
		}

		for (auto &state : editor->key_state) {
			if (state.state & KeyState_down) {
				state.state &= ~KeyState_down;
			} else if (state.state & KeyState_up) {
				state.state = KeyState_none;
			}
			if (state.state & KeyState_repeated) {
				state.state &= ~KeyState_repeated;
			}
			state.state &= ~KeyState_begin_drag;
			if ((state.state & KeyState_held) && !(state.state & KeyState_drag) && (distance_squared(state.start_position, app->current_mouse_position) >= pow2(8))) {
				state.state |= KeyState_drag | KeyState_begin_drag;
			}
		}

		editor->input_string.clear();

		app->tg->set_render_target(app->tg->back_buffer);
		gui_draw();


		{
			timed_block("present"s);
			app->tg->present();
		}

		update_time();

		++fps_counter;
		set_title(app->window, tformat(u8"frame_time: % ms, fps: %, draw calls: %", FormatFloat{.value = app->frame_time * 1000, .precision = 1}, fps_counter_result, app->tg->draw_call_count));

		set_cursor(*app->window, app->current_cursor);

		fps_timer += app->frame_time;
		if (fps_timer >= 1) {
			fps_counter_result = fps_counter;
			fps_timer -= 1;
			fps_counter = 0;
		}

		clear_temporary_storage();
	};
	info.on_key_down = [](u8 key) {
		editor->key_state[key].state = KeyState_down | KeyState_repeated | KeyState_held;
		editor->key_state[key].start_position = editor->input_is_locked ? editor->input_lock_mouse_position : app->current_mouse_position;
	};
	info.on_key_up = [](u8 key) {
		editor->key_state[key].state = KeyState_up;
	};
	info.on_key_repeat = [](u8 key) {
		editor->key_state[key].state |= KeyState_repeated;
	};
	info.on_mouse_down = [](u8 button){
		editor->key_state[256 + button].state = KeyState_down | KeyState_held;
		editor->key_state[256 + button].start_position = editor->input_is_locked ? editor->input_lock_mouse_position : app->current_mouse_position;
	};
	info.on_mouse_up = [](u8 button){
		auto &state = editor->key_state[256 + button];
		state.state = KeyState_up | ((state.state & KeyState_drag) ? KeyState_end_drag : 0);
	};
	info.on_char = [](u32 ch) {
		editor->input_string.add(encode_utf8(ch));
	};
	info.client_size = {1280, 720};
	if (!create_window(info)) {
		return;
	}
	defer { free(app->window); };

	assert_always(app->window);


	app->frame_timer = create_precise_timer();

	Profiler::enabled = false;
	Profiler::reset();

	while (update(app->window)) {
	}

	if (app->current_scene) {
		write_entire_file(tl_file_string("test.scene"s), as_bytes(with(temporary_allocator, serialize_scene_text(app->current_scene))));

		//serialize_window_layout();

		for_each(app->current_scene->entities, [](Entity &e) {
			destroy_entity(e);
		});
	}

	for (auto scene : app->scenes) {
		scene->free();
	}
}

s32 tl_main(Span<Span<utf8>> arguments) {
	Profiler::init();
	defer { Profiler::deinit(); };

#define TRACK_ALLOCATIONS 0
#if TRACK_ALLOCATIONS
	debug_init();
	defer { debug_deinit(); };
	current_allocator = tracking_allocator;
#endif

	construct(project_cpp_files);
	construct(project_h_files  );
	construct(editor_cpp_files );
	construct(all_component_names);
	construct(project_directory_text_field);

	editor_exe_path = arguments[0];
	if (!is_absolute_path(editor_exe_path)) {
		editor_exe_path = make_absolute_path(editor_exe_path);
	}

	editor_bin_directory = parse_path(editor_exe_path).directory;
	editor_bin_directory.size ++; // include slash

	editor_directory = parent_directory(editor_bin_directory);


	auto log_file = open_file(tformat("%bin/editor_log.txt"s, editor_directory), {.write = true});
	defer { close(log_file); };
	log_printer = Printer {
		[](PrintKind kind, Span<utf8> string, void *data) {
			console_printer(kind, string);
			write({data}, as_bytes(string));
		},
		log_file.handle
	};

	current_printer = log_printer;


	component_descs_getter_path = format(u8"%build/get_component_descs.cpp", editor_directory);

	//project_name      = u8"example"s;
	//project_directory = format(u8"%example/"s, editor_directory);

	for_each_file_recursive(tformat(u8"%src/t3d/", editor_directory), [] (Span<utf8> item) {
		if (ends_with(item, u8".cpp"s)) {
			editor_cpp_files.add(item);
		}
	});
	editor_cpp_files.make_absolute();

	clear_build_directory();
	defer { clear_build_directory(); };

	auto cpu_info = get_cpu_info();
	print(R"(CPU:
 - Brand: %
 - Vendor: %
 - Thread count: %
 - Cache line size: %
)", as_span(cpu_info.brand), to_string(cpu_info.vendor), cpu_info.logical_processor_count, cpu_info.cache_line_size);

	print("Cache:\n");

	for (u32 level = 0; level != CpuCacheLevel_count; ++level) {
		for (u32 type_index = 0; type_index != CpuCacheType_count; ++type_index) {
			auto &cache = cpu_info.caches_by_level_and_type[level][type_index];
			if (cache.count == 0 || cache.size == 0)
				continue;
			print("L% %: % x %\n", level + 1, to_string((CpuCacheType)type_index), cache.count, format_bytes(cache.size));
		}
	}

#define f(x) print(" - " #x ": %\n", cpu_info.has_feature(CpuFeature_##x));
	tl_all_cpu_features(f)
#undef f

	print("RAM: %\n", format_bytes(get_ram_size()));

	run();

#if TRACK_ALLOCATIONS
	current_allocator = temporary_allocator;

	print("Unfreed allocations:\n");
	for (auto &[pointer, info] : get_tracked_allocations()) {
		print("size: %, location: %, call stack:\n", info.size, info.location);
		auto call_stack = to_string(info.call_stack);
		for (auto &call : call_stack.call_stack) {
			print("\t%(%):%\n", call.file, call.line, call.name);
		}
	}
#endif
	return 0;
}
