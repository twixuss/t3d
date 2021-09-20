#include <t3d/common.h>
#include <source_location>
#include <tl/common.h>
#include <tl/bin2cpp.h>
tl::umm get_hash(struct ManipulatorStateKey const &);

#include "../dep/tl/include/tl/masked_block_list.h"
#include <t3d/component.h>
#include <t3d/components/light.h>
#include <t3d/components/mesh_renderer.h>
#include <t3d/components/camera.h>
#include "editor/window.h"
#include "editor/scene_view.h"
#include "editor/hierarchy_view.h"
#include "editor/split_view.h"
#include "editor/property_view.h"
#include "editor/file_view.h"
#include "editor/tab_view.h"
#include "editor/input.h"
#include <t3d/serialize.h>
#include <t3d/assets.h>
#include "runtime.h"

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


m4 local_to_world_position(v3f position, quaternion rotation, v3f scale) {
	return m4::translation(position) * (m4)rotation * m4::scale(scale);
}

m4 local_to_world_normal(quaternion rotation, v3f scale) {
	return (m4)rotation * m4::scale(1.0f / scale);
}

void render_scene(SceneView *view) {
	timed_function();

	//print("%\n", to_euler_angles(quaternion_from_euler(0, time, time)));
	//selected_entity->qrotation = quaternion_from_euler(to_euler_angles(quaternion_from_euler(0, time, time)));

	auto &camera = *view->camera;
	auto &camera_entity = *view->camera_entity;

	render_camera(camera, camera_entity);

	tg::disable_blend();

	//tg::clear(tg::back_buffer, tg::ClearFlags_depth, {}, 1);

	tg::set_rasterizer({
		.depth_test = true,
		.depth_write = true,
		.depth_func = tg::Comparison_less,
	});
	tg::set_blend(tg::BlendFunction_add, tg::Blend_source_alpha, tg::Blend_one_minus_source_alpha);

	if (selection.kind == Selection_entity) {
		auto new_transform = manipulate_transform(selection.entity->position, selection.entity->rotation, selection.entity->scale, view->manipulator_kind);
		selection.entity->position = new_transform.position;
		selection.entity->rotation = new_transform.rotation;
		selection.entity->scale    = new_transform.scale;

		for (auto &request : manipulator_draw_requests) {
			v3f camera_to_handle_direction = normalize(request.position - camera_entity.position);
			tg::update_shader_constants(entity_constants, {
				.local_to_camera_matrix =
					camera.world_to_camera_matrix
					* m4::translation(camera_entity.position + camera_to_handle_direction)
					* (m4)request.rotation
					* m4::scale(request.size * dot(camera_to_handle_direction, camera_entity.rotation * v3f{0,0,-1})),
				.local_to_world_normal_matrix = local_to_world_normal(request.rotation, V3f(request.size * dot(camera_to_handle_direction, camera_entity.rotation * v3f{0,0,-1}))),
				.object_rotation_matrix = (m4)request.rotation,
			});
			tg::set_shader(handle_shader);
			tg::set_shader_constants(handle_constants, 0);

			u32 selected_element = request.highlighted_part_index;

			v3f to_camera = normalize(camera_entity.position - selection.entity->position);
			switch (request.kind) {
				case Manipulate_position: {
					tg::update_shader_constants(handle_constants, {.color = V3f(1), .selected = (f32)(selected_element != null_manipulator_part), .to_camera = to_camera});
					draw_mesh(handle_sphere_mesh);

					tg::update_shader_constants(handle_constants, {.color = V3f(1,0,0), .selected = (f32)(selected_element == 0), .to_camera = to_camera});
					draw_mesh(handle_axis_x_mesh);

					tg::update_shader_constants(handle_constants, {.color = V3f(0,1,0), .selected = (f32)(selected_element == 1), .to_camera = to_camera});
					draw_mesh(handle_axis_y_mesh);

					tg::update_shader_constants(handle_constants, {.color = V3f(0,0,1), .selected = (f32)(selected_element == 2), .to_camera = to_camera});
					draw_mesh(handle_axis_z_mesh);

					tg::update_shader_constants(handle_constants, {.color = V3f(1,0,0), .selected = (f32)(selected_element == 0), .to_camera = to_camera});
					draw_mesh(handle_arrow_x_mesh);

					tg::update_shader_constants(handle_constants, {.color = V3f(0,1,0), .selected = (f32)(selected_element == 1), .to_camera = to_camera});
					draw_mesh(handle_arrow_y_mesh);

					tg::update_shader_constants(handle_constants, {.color = V3f(0,0,1), .selected = (f32)(selected_element == 2), .to_camera = to_camera});
					draw_mesh(handle_arrow_z_mesh);

					tg::update_shader_constants(handle_constants, {.color = V3f(1,0,0), .selected = (f32)(selected_element == 3), .to_camera = to_camera});
					draw_mesh(handle_plane_x_mesh);

					tg::update_shader_constants(handle_constants, {.color = V3f(0,1,0), .selected = (f32)(selected_element == 4), .to_camera = to_camera});
					draw_mesh(handle_plane_y_mesh);

					tg::update_shader_constants(handle_constants, {.color = V3f(0,0,1), .selected = (f32)(selected_element == 5), .to_camera = to_camera});
					draw_mesh(handle_plane_z_mesh);
					break;
				}
				case Manipulate_rotation: {
					tg::update_shader_constants(handle_constants, {.matrix = m4::rotation_r_zxy(0,0,pi/2), .color = V3f(1,0,0), .selected = (f32)(selected_element == 0), .to_camera = to_camera, .is_rotation = 1});
					draw_mesh(handle_circle_mesh);

					tg::update_shader_constants(handle_constants, {.color = V3f(0,1,0), .selected = (f32)(selected_element == 1), .to_camera = to_camera, .is_rotation = 1});
					draw_mesh(handle_circle_mesh);

					tg::update_shader_constants(handle_constants, {.matrix = m4::rotation_r_zxy(pi/2,0,0), .color = V3f(0,0,1), .selected = (f32)(selected_element == 2), .to_camera = to_camera, .is_rotation = 1});
					draw_mesh(handle_circle_mesh);

					if (request.dragging) {
						quaternion rotation = quaternion_look(request.tangent.direction);
						v3f position = request.tangent.origin;
						tg::update_shader_constants(entity_constants, {
							.local_to_camera_matrix =
								camera.world_to_camera_matrix
								* m4::translation(camera_entity.position + camera_to_handle_direction)
								* m4::scale(request.size * dot(camera_to_handle_direction, camera_entity.rotation * v3f{0,0,-1})) * m4::translation(position) * (m4)rotation,
							.local_to_world_normal_matrix = local_to_world_normal(rotation, V3f(request.size * dot(camera_to_handle_direction, camera_entity.rotation * v3f{0,0,-1}))),
							.object_rotation_matrix = (m4)rotation,
						});
						tg::update_shader_constants(handle_constants, {.color = V3f(1,1,1), .selected = 1, .to_camera = to_camera});
						draw_mesh(handle_tangent_mesh);
					}

					break;
				}
				case Manipulate_scale: {
					tg::update_shader_constants(handle_constants, {.color = V3f(1), .selected = (f32)(selected_element != null_manipulator_part), .to_camera = to_camera});
					draw_mesh(handle_sphere_mesh);

					tg::update_shader_constants(handle_constants, {.matrix = m4::scale(request.scale.x, 1, 1), .color = V3f(1,0,0), .selected = (f32)(selected_element == 0), .to_camera = to_camera});
					draw_mesh(handle_axis_x_mesh);

					tg::update_shader_constants(handle_constants, {.matrix = m4::scale(1, request.scale.y, 1), .color = V3f(0,1,0), .selected = (f32)(selected_element == 1), .to_camera = to_camera});
					draw_mesh(handle_axis_y_mesh);

					tg::update_shader_constants(handle_constants, {.matrix = m4::scale(1, 1, request.scale.z), .color = V3f(0,0,1), .selected = (f32)(selected_element == 2), .to_camera = to_camera});
					draw_mesh(handle_axis_z_mesh);

					tg::update_shader_constants(handle_constants, {.matrix = m4::translation(0.8f*request.scale.x,0,0) * m4::scale(1.5f), .color = V3f(1,0,0), .selected = (f32)(selected_element == 0), .to_camera = to_camera});
					draw_mesh(handle_sphere_mesh);

					tg::update_shader_constants(handle_constants, {.matrix = m4::translation(0,0.8f*request.scale.y,0) * m4::scale(1.5f), .color = V3f(0,1,0), .selected = (f32)(selected_element == 1), .to_camera = to_camera});
					draw_mesh(handle_sphere_mesh);

					tg::update_shader_constants(handle_constants, {.matrix = m4::translation(0,0,0.8f*request.scale.z) * m4::scale(1.5f), .color = V3f(0,0,1), .selected = (f32)(selected_element == 2), .to_camera = to_camera});
					draw_mesh(handle_sphere_mesh);

					tg::update_shader_constants(handle_constants, {.color = V3f(1,0,0), .selected = (f32)(selected_element == 3), .to_camera = to_camera});
					draw_mesh(handle_plane_x_mesh);

					tg::update_shader_constants(handle_constants, {.color = V3f(0,1,0), .selected = (f32)(selected_element == 4), .to_camera = to_camera});
					draw_mesh(handle_plane_y_mesh);

					tg::update_shader_constants(handle_constants, {.color = V3f(0,0,1), .selected = (f32)(selected_element == 5), .to_camera = to_camera});
					draw_mesh(handle_plane_z_mesh);
					break;
				}
			}
		}
		manipulator_draw_requests.clear();
	}

	for_each_component<Camera>([&](Camera &camera) {
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

	debug_draw_lines();
}

void add_assets(StringBuilder &asset_builder, Span<pathchar> directory) {
	auto items = get_items_in_directory(directory);
	for (auto &item : items) {
		auto path16 = concatenate(directory, '/', item.name, '\0');
		if (item.kind == FileItem_directory) {
			add_assets(asset_builder, path16);
		} else {
			auto path8 = concatenate(to_utf8(directory), '/', item.name);
			append_bytes(asset_builder, (u32)path8.size);
			append_bytes(asset_builder, path8);

			auto data = read_entire_file(path16);
			append_bytes(asset_builder, (u32)data.size);
			append_bytes(asset_builder, as_span(data));
		}
	}
}

void invoke_msvc(Span<utf8> arguments) {
	constexpr auto cl_path = "C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\Community\\VC\\Tools\\MSVC\\14.29.30037\\bin\\Hostx64\\x64\\cl.exe"s;

	StringBuilder bat_builder;
	append(bat_builder, u8R"(
@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
cl )");

	append(bat_builder, arguments);
	append(bat_builder, " > \"temp\\cppbuild_log.txt\"");

	auto bat_path = tl_file_string("temp\\cppbuild.bat"s);
	write_entire_file(bat_path, as_bytes(to_string(bat_builder)));

	auto process = execute(bat_path);
	if (!process) {
		print(Print_error, "Cannot execute file '%'\n", bat_path);
		return;
	}

	defer { free(process); };

	print("cl %\n", arguments);

	wait(process);
	auto exit_code = get_exit_code(process);
	if (exit_code != 0) {
		print(Print_error, "Build command failed\n");
		print(as_utf8(read_entire_file(tl_file_string("temp\\cppbuild_log.txt"))));
		return;
	}

	print("Build succeeded\n");
}

void build_executable() {
	scoped_allocator(temporary_allocator);

	StringBuilder asset_builder;
	add_assets(asset_builder, to_pathchars(assets.directory));

	auto data_file = open_file(tl_file_string("data.bin"), {.write = true});
	defer { close(data_file); };

	DataHeader header;

	set_cursor(data_file, sizeof(header), File_begin);

	auto asset_data = as_bytes(to_string(asset_builder));
	header.asset_offset = get_cursor(data_file);
	header.asset_size = asset_data.size;
	write(data_file, asset_data);

	auto scene_data = serialize_scene_binary();
	header.scene_offset = get_cursor(data_file);
	header.scene_size = scene_data.size;
	write(data_file, scene_data);

	set_cursor(data_file, 0, File_begin);
	write(data_file, value_as_bytes(header));


	invoke_msvc(u8R"(..\src\main_runtime.cpp ..\src\common.cpp /ZI /I"." /I"..\include" /I"..\dep\tl\include" /I"..\dep\freetype\include" /I"..\dep\stb" /I"..\dep\tgraphics\include" /std:c++latest /D"BUILD_DEBUG=0" /link /LIBPATH:"../dep/freetype/win64")"s);
}

void insert_scripts_paths(ListList<utf8> &scripts, Span<utf8> directory) {
	FileItemList items = get_items_in_directory(with(temporary_allocator, to_pathchars(assets.directory)));
	for (auto &item : items) {
		auto item_path = with(temporary_allocator, concatenate(directory, '/', item.name));
		if (item.kind == FileItem_directory) {
			insert_scripts_paths(scripts, item_path);
		} else {
			if (ends_with(item.name, u8".cpp"s)) {
				scripts.add(item_path);
			}
		}
	}
}

ComponentUID component_uid_counter = 0;
ComponentUID get_new_uid() {
	return component_uid_counter++;
}

HashMap<ComponentUID, ComponentInfo> built_in_components;

using T3dRegisterComponents = void (*)(HashMap<Span<utf8>, ComponentUID> &component_name_to_uid, HashMap<ComponentUID, ComponentInfo> &component_infos, ComponentUID (*get_new_uid)());

void recompile_all_scripts() {
	ListList<utf8> script_paths;
	insert_scripts_paths(script_paths, assets.directory);
	script_paths.make_absolute();

	ListList<utf8> component_names;
	for (auto &path : script_paths) {
		auto file = open_file(to_pathchars(path, true).data, {.read = true});
		if (!is_valid(file)) {
			print(Print_error, "Couldn't open script file %\n", path);
			continue;
		}
		defer { close(file); };

		auto mapped = map_file(file);
		defer { unmap_file(mapped); };

		auto decls = find_all(mapped.data, "DECLARE_COMPONENT"b);
		for (auto &decl : decls) {
			if (decl.end() >= mapped.data.end()) {
				print(Print_error, "DECLARE_COMPONENT was at the end of the file %\n", path);
				continue;
			}
			if (*decl.end() != '(') {
				print(Print_error, "'(' was expected after DECLARE_COMPONENT in file %\n", path);
				continue;
			}
			u8 *closing_paren = decl.end() + 1;
			while (1) {
				if (closing_paren == mapped.data.end()) {
					print(Print_error, "DECLARE_COMPONENT was unclosed in file %\n", path);
					goto skip_decl;
				}
				if (*closing_paren == ')') {
					break;
				}
				++closing_paren;
			}

			component_names.add((Span<utf8>)Span(decl.end() + 1, closing_paren));

		skip_decl:;
		}
	}
	component_names.make_absolute();

	auto scripts_init_path = u8"scripts_init.cpp"s;

	// Generate source
	{
		StringBuilder builder;
		append(builder, u8R"(#pragma once
#define TL_IMPL
#define TGRAPHICS_IMPL
#include <freetype/freetype.h> // TODO: Shold this be in user code?
#include <t3d/component.h>
#include <t3d/serialize.h>

#pragma comment(lib, "freetype.lib")

)"s);
		for (auto component_name : component_names) {
			append_format(builder, "extern \"C\" void register_component_%(HashMap<Span<utf8>, ComponentUID> &component_name_to_uid, HashMap<ComponentUID, ComponentInfo> &component_infos, ComponentUID (*get_new_uid)());\n", component_name);
		}
		append(builder, u8R"(extern "C" __declspec(dllexport) void t3d_register_components(HashMap<Span<utf8>, ComponentUID> &component_name_to_uid, HashMap<ComponentUID, ComponentInfo> &component_infos, ComponentUID (*get_new_uid)()) {
)");
		for (auto component_name : component_names) {
			append_format(builder, u8R"(register_component_%(component_name_to_uid, component_infos, get_new_uid);
)", component_name);
		}
		append(builder, u8"}"s);


		write_entire_file(to_pathchars(scripts_init_path), as_bytes(to_string(builder)));
	}



	// setup environment
	Span<utf8> scripts_dll_path = u8"scripts.dll"s;

	// compile dll
	{
		StringBuilder builder;
		append_format(builder, "% ", scripts_init_path);
		for (auto script_path : script_paths) {
			append_format(builder, "% ", script_path);
		}
		append(builder, R"(../data/component.cpp /LD /out:scripts.dll /ZI /I"." /I"..\include" /I"..\dep\tl\include" /I"..\dep\freetype\include" /I"..\dep\stb" /I"..\dep\tgraphics\include" /std:c++latest /D"BUILD_DEBUG=0" /link /LIBPATH:"../dep/freetype/win64")"s);
		invoke_msvc(as_utf8(to_string(builder)));
	}

	// load dll
	HMODULE scripts_dll = LoadLibraryW(with(temporary_allocator, (wchar *)to_pathchars(scripts_dll_path, true).data));

	// register all components in dll
	auto scripts_register_components = (T3dRegisterComponents)GetProcAddress(scripts_dll, "t3d_register_components");

	StringBuilder builder;

	for_each(entities, [&](Entity &entity) {
		for (auto &component : entity.components) {
			auto found_info = component_infos.find(component.type);
			assert(found_info);
			auto &info = *found_info;
			info.serialize(builder, info.storage.get(component.index), false);
		}
	});

	// Remove old components
	set(component_infos, built_in_components);

	// Add new components
	scripts_register_components(component_name_to_uid, component_infos, get_new_uid);
}

void run() {
	construct(manipulator_draw_requests);
	construct(manipulator_states);
	construct(debug_lines);

	construct(button_states);
	construct(float_field_states);
	construct(text_field_states );
	construct(scroll_bar_states );

	construct(input_string);

	construct(drag_and_drop_data);
	construct(tab_moves);
	construct(editor_windows);
	construct(gui_draws);

	construct(editor_assets);
	editor_assets.directory = u8"../data"s;

	CreateWindowInfo info;
	info.on_create = [](Window &window) {
		runtime_init(window);
		assets.directory = u8"../example"s;

		built_in_components = copy(component_infos);

		recompile_all_scripts();

		tg::set_scissor(aabb_min_max({}, (v2s)window.client_size));

		init_font();

		debug_lines_vertex_buffer = tg::create_vertex_buffer(
			{},
			{
				tg::Element_f32x3, // position
				tg::Element_f32x3, // color
			}
		);
		//tg::set_vsync(false);



		handle_sphere_mesh  = editor_assets.get_mesh(u8"handle.glb:Sphere"s);
		handle_circle_mesh  = editor_assets.get_mesh(u8"handle.glb:Circle"s);
		handle_tangent_mesh = editor_assets.get_mesh(u8"handle.glb:Tangent"s);
		handle_axis_x_mesh  = editor_assets.get_mesh(u8"handle.glb:AxisX"s );
		handle_axis_y_mesh  = editor_assets.get_mesh(u8"handle.glb:AxisY"s );
		handle_axis_z_mesh  = editor_assets.get_mesh(u8"handle.glb:AxisZ"s );
		handle_arrow_x_mesh = editor_assets.get_mesh(u8"handle.glb:ArrowX"s );
		handle_arrow_y_mesh = editor_assets.get_mesh(u8"handle.glb:ArrowY"s );
		handle_arrow_z_mesh = editor_assets.get_mesh(u8"handle.glb:ArrowZ"s );
		handle_plane_x_mesh = editor_assets.get_mesh(u8"handle.glb:PlaneX"s);
		handle_plane_y_mesh = editor_assets.get_mesh(u8"handle.glb:PlaneY"s);
		handle_plane_z_mesh = editor_assets.get_mesh(u8"handle.glb:PlaneZ"s);

		auto create_default_scene = [&]() {
			auto &suzanne = create_entity("suzan\"ne");
			suzanne.rotation = quaternion_from_euler(radians(v3f{-54.7, 45, 0}));
			{
				auto &mr = add_component<MeshRenderer>(suzanne);
				mr.mesh = assets.get_mesh(u8"scene.glb:Suzanne"s);
				mr.material = &surface_material;
				mr.lightmap = assets.get_texture_2d(u8"suzanne_lightmap.png"s);
			}
			selection.set(&suzanne);

			auto &floor = create_entity("floor");
			{
				auto &mr = add_component<MeshRenderer>(floor);
				mr.mesh = assets.get_mesh(u8"scene.glb:Room"s);
				mr.material = &surface_material;
				mr.lightmap = assets.get_texture_2d(u8"floor_lightmap.png"s);
			}

			auto light_texture = assets.get_texture_2d(u8"spotlight_mask.png"s);

			{
				auto &light = create_entity("light1");
				light.position = {0,2,6};
				//light.rotation = quaternion_from_euler(-pi/10,0,pi/6);
				light.rotation = quaternion_from_euler(0,0,0);
				add_component<Light>(light).mask = light_texture;
			}

			{
				auto &light = create_entity("light2");
				light.position = {6,2,-6};
				light.rotation = quaternion_from_euler(-pi/10,pi*0.75,0);
				add_component<Light>(light).mask = light_texture;
			}

			auto &camera_entity = create_entity("main camera");
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
		};

		//if (!deserialize_window_layout())
			main_window = create_split_view(
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

		if (!deserialize_scene_text(u8"test.scene"s))
			create_default_scene();

		window.min_window_size = client_size_to_window_size(window, main_window->get_min_size());
	};
	info.on_draw = [](Window &window) {
		current_cursor = Cursor_default;

		static v2u old_window_size;
		if (any_true(old_window_size != window.client_size)) {
			old_window_size = window.client_size;

			main_window->resize({.min = {}, .max = (v2s)window.client_size});

			tg::resize_render_targets(window.client_size);
		}

		current_viewport = current_scissor = {
			.min = {},
			.max = (v2s)window.client_size,
		};
		tg::set_viewport(current_viewport);
		tg::set_scissor(current_scissor);

		current_mouse_position = {window.mouse_position.x, (s32)window.client_size.y - window.mouse_position.y};

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

		if (key_down(Key_f2, {.anywhere = true})) {
			for_each(entities, [](Entity &e) {
				print("name: %, index: %, flags: %, position: %, rotation: %\n", e.name, index_of(entities, &e).get(), e.flags, e.position, degrees(to_euler_angles(e.rotation)));
				for (auto &c : e.components) {
					print("\tparent: %, type: % (%), index: %\n", c.entity_index, c.type, get_component_info(c.type).name, c.index);
				}
			});
		}

		if (key_down(Key_f6, {.anywhere = true})) {
			build_executable();
		}
		window.min_window_size = client_size_to_window_size(window, main_window->get_min_size());

		input_user_index = 0;
		focusable_input_user_index = 0;

		timed_block("frame"s);

		runtime_render();

		tg::clear(tg::back_buffer, tg::ClearFlags_color | tg::ClearFlags_depth, foreground_color, 1);

		{
			timed_block("main_window->render()"s);
			main_window->render();
		}

		switch (drag_and_drop_kind) {
			case DragAndDrop_file: {
				auto texture = assets.get_texture_2d(as_utf8(drag_and_drop_data));
				if (texture) {
					aabb<v2s> thumbnail_viewport;
					thumbnail_viewport.min = thumbnail_viewport.max = current_mouse_position;
					thumbnail_viewport.max.x += 128;
					thumbnail_viewport.min.y -= 128;
					push_current_viewport(thumbnail_viewport) {
						gui_image(texture);
					}
				}
				break;
			}
			case DragAndDrop_tab: {
				auto tab_info = *(DragDropTabInfo *)drag_and_drop_data.data;
				auto tab = tab_info.tab_view->tabs[tab_info.tab_index];

				auto font = get_font_at_size(font_collection, font_size);
				ensure_all_chars_present(tab.window->name, font);
				auto placed_chars = with(temporary_allocator, place_text(tab.window->name, font));

				tg::Viewport tab_viewport;
				tab_viewport.min = tab_viewport.max = current_mouse_position;

				tab_viewport.min.y -= TabView::tab_height;
				tab_viewport.max.x = tab_viewport.min.x + placed_chars.back().position.max.x + 4;

				push_current_viewport(tab_viewport) {
					gui_panel({.1,.1,.1,1});

					label(placed_chars, font, {.position = {2, 0}});
				}
				break;
			}
		}

		if (drag_and_dropping()) {
			if (key_state[256].state & KeyState_up) {
				drag_and_drop_kind = DragAndDrop_none;
				unlock_input_nocheck();
			}
		}

		debug_frame();

		bool debug_print_editor_window_hierarchy = frame_index == 0;
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
					auto main_window_viewport = main_window->viewport;
					main_window = what_is_left;
					main_window->resize(main_window_viewport);
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
					assert(to == main_window);
					main_window = create_split_view(left, right, {.split_t = 0.5f, .horizontal = horizontal});
				}

				tg::Viewport window_viewport = {};
				window_viewport.max = (v2s)max(main_window->get_min_size(), window.client_size);
				main_window->resize(window_viewport);
				resize(window, (v2u)window_viewport.max);

				//to_parent->resize(to_parent->viewport);
			}

			//split_view->free();

			debug_print_editor_window_hierarchy = true;
		}
		tab_moves.clear();


		if (should_unlock_input) {
			should_unlock_input = false;
			input_is_locked = false;
			input_locker = 0;
		}

		if (debug_print_editor_window_hierarchy || (key_state[Key_f4].state & KeyState_down)) {
			debug_print_editor_window_hierarchy = false;
			debug_print_editor_window_hierarchy_tab = 0;

			main_window->debug_print();
		}

		for (auto &state : key_state) {
			if (state.state & KeyState_down) {
				state.state &= ~KeyState_down;
			} else if (state.state & KeyState_up) {
				state.state = KeyState_none;
			}
			if (state.state & KeyState_repeated) {
				state.state &= ~KeyState_repeated;
			}
			state.state &= ~KeyState_begin_drag;
			if ((state.state & KeyState_held) && !(state.state & KeyState_drag) && (distance_squared(state.start_position, current_mouse_position) >= pow2(8))) {
				state.state |= KeyState_drag | KeyState_begin_drag;
			}
		}

		input_string.clear();

		for (auto &draw : gui_draws) {
			tg::set_viewport(draw.viewport);
			tg::set_scissor(draw.scissor);
			switch (draw.kind) {
				case GuiDraw_rect_colored: {
					auto &rect_colored = draw.rect_colored;
					blit(rect_colored.color);
					break;
				}
				case GuiDraw_rect_textured: {
					auto &rect_textured = draw.rect_textured;
					blit(rect_textured.texture);
					break;
				}
				case GuiDraw_label: {
					auto &label = draw.label;

					auto font = label.font;
					auto placed_text = label.placed_chars;

					assert(placed_text.size);

					struct Vertex {
						v2f position;
						v2f uv;
					};

					List<Vertex> vertices;
					vertices.allocator = temporary_allocator;

					for (auto &c : placed_text) {
						Span<Vertex> quad = {
							{{c.position.min.x, c.position.min.y}, {c.uv.min.x, c.uv.min.y}},
							{{c.position.max.x, c.position.min.y}, {c.uv.max.x, c.uv.min.y}},
							{{c.position.max.x, c.position.max.y}, {c.uv.max.x, c.uv.max.y}},
							{{c.position.min.x, c.position.max.y}, {c.uv.min.x, c.uv.max.y}},
						};
						vertices += {
							quad[1], quad[0], quad[2],
							quad[2], quad[0], quad[3],
						};
					}

					if (text_vertex_buffer) {
						tg::update_vertex_buffer(text_vertex_buffer, as_bytes(vertices));
					} else {
						text_vertex_buffer = tg::create_vertex_buffer(as_bytes(vertices), {
							tg::Element_f32x2, // position
							tg::Element_f32x2, // uv
						});
					}
					tg::set_rasterizer({.depth_test = false, .depth_write = false});
					tg::set_topology(tg::Topology_triangle_list);
					tg::set_blend(tg::BlendFunction_add, tg::Blend_secondary_color, tg::Blend_one_minus_secondary_color);
					tg::set_shader(text_shader);
					tg::set_shader_constants(text_shader_constants, 0);
					tg::update_shader_constants(text_shader_constants, {
						.inv_half_viewport_size = v2f{2,-2} / (v2f)draw.viewport.size(),
						.offset = (v2f)label.position,
					});
					tg::set_vertex_buffer(text_vertex_buffer);
					tg::set_sampler(tg::Filtering_nearest, 0);
					tg::set_texture(font->texture, 0);
					tg::draw(vertices.size);
					break;
				}
				default: invalid_code_path("not implemented"); break;
			}
		}
		gui_draws.clear();

		clear_temporary_storage();

		{
			timed_block("present"s);
			tg::present();
		}

		update_time();

		++fps_counter;
		set_title(&window, tformat(u8"frame_time: % ms, fps: %", frame_time * 1000, fps_counter_result));

		set_cursor(window, current_cursor);

		fps_timer += frame_time;
		if (fps_timer >= 1) {
			fps_counter_result = fps_counter;
			fps_timer -= 1;
			fps_counter = 0;
		}
	};
	info.on_key_down = [](u8 key) {
		key_state[key].state = KeyState_down | KeyState_repeated | KeyState_held;
		key_state[key].start_position = input_is_locked ? input_lock_mouse_position : current_mouse_position;
	};
	info.on_key_up = [](u8 key) {
		key_state[key].state = KeyState_up;
	};
	info.on_key_repeat = [](u8 key) {
		key_state[key].state |= KeyState_repeated;
	};
	info.on_mouse_down = [](u8 button){
		key_state[256 + button].state = KeyState_down | KeyState_held;
		key_state[256 + button].start_position = input_is_locked ? input_lock_mouse_position : current_mouse_position;
	};
	info.on_mouse_up = [](u8 button){
		auto &state = key_state[256 + button];
		state.state = KeyState_up | ((state.state & KeyState_drag) ? KeyState_end_drag : 0);
	};
	info.on_char = [](u32 ch) {
		input_string.add(encode_utf8(ch));
	};
	info.client_size = {1280, 720};
	window = create_window(info);
	defer { free(window); };

	assert_always(window);


	frame_timer = create_precise_timer();

	Profiler::enabled = false;
	Profiler::reset();

	while (update(window)) {
	}

	write_entire_file(tl_file_string("test.scene"s), as_bytes(with(temporary_allocator, serialize_scene_text())));

	serialize_window_layout();

	for_each(entities, [](Entity &e) {
		destroy_entity(e);
	});

	free_component_storages();
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

	auto log_file = open_file(tl_file_string("editor_log.txt"s), {.write = true});
	defer { close(log_file); };
	auto log_printer = Printer {
		[](PrintKind kind, Span<utf8> string, void *data) {
			console_printer(kind, string);
			write({data}, as_bytes(string));
		},
		log_file.handle
	};

	current_printer = log_printer;

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
