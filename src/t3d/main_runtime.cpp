#define TL_MAIN

#include "common.h"
#include "runtime.h"
#include "assets.h"

Camera *main_camera;

File data_file;
Span<u8> data_buffer;
DataHeader *data_header;

void load_assets() {
	shared->assets.asset_path_to_data = {};

	auto cursor = data_buffer.data + data_header->asset_offset;
	auto end    = data_buffer.data + data_header->asset_offset + data_header->asset_size;
	while (1) {
		if (cursor == end) {
			break;
		}

		auto asset_path_size = *(u32 *)cursor;
		cursor += sizeof(asset_path_size);
		assert(cursor < end);

		List<utf8> asset_path;
		asset_path.resize(asset_path_size);

		memcpy(asset_path.data, cursor, asset_path_size);
		cursor += asset_path_size;
		assert(cursor < end);


		auto asset_size = *(u32 *)cursor;
		cursor += sizeof(asset_size);
		assert(cursor < end);

		Span<u8> asset;
		asset.data = cursor;
		asset.size = asset_size;

		cursor += asset_size;

		shared->assets.asset_path_to_data.get_or_insert(asset_path) = asset;

		print("Got asset '%'\n", asset_path);
	}
}

extern "C" void t3d_get_component_descs(List<ComponentDesc> &descs);

s32 tl_main(Span<Span<utf8>> arguments) {
	auto log_file = open_file(tl_file_string("runtime_log.txt"s), {.write = true});
	defer { close(log_file); };
	auto log_printer = Printer {
		[](PrintKind kind, Span<utf8> string, void *data) {
			console_printer(kind, string);
			write({data}, as_bytes(string));
		},
		log_file.handle
	};

	current_printer = log_printer;

	Profiler::init();
	defer { Profiler::deinit(); };

	print("Opening 'data.bin' ...\n");
	data_file = open_file(tl_file_string("data.bin"), {.read = true});
	defer { close(data_file); };

	print("Mapping 'data.bin' ...\n");
	auto mapped_assets = map_file(data_file);
	defer { unmap_file(mapped_assets); };

	data_buffer = mapped_assets.data;
	data_header = (DataHeader *)data_buffer.data;

	CreateWindowInfo info;
	info.on_create = [](Window &window) {
		print("Initializing runtime ...\n");
		runtime_init(window);

		List<ComponentDesc> descs;
		t3d_get_component_descs(descs);
		for (auto desc : descs) {
			update_component_info(desc);
		}

		print("Loading assets ...\n");
		load_assets();

		print("Loading scene ...\n");
		assert_always(deserialize_scene_binary(Span(data_buffer.data + data_header->scene_offset, data_header->scene_size)));

		print("Starting runtime ...\n");
		runtime_start();

		for_each_component<Camera>([&](Camera &camera) {
			main_camera = &camera;
			for_each_break;
		});
	};

	info.on_draw = [](Window &window) {
		static v2u old_window_size;
		if (any_true(old_window_size != window.client_size)) {
			old_window_size = window.client_size;
			shared->tg->resize_render_targets(window.client_size);
			main_camera->resize_targets(window.client_size);
		}

		runtime_update();
		runtime_render();

		shared->tg->clear(shared->tg->back_buffer, tg::ClearFlags_color | tg::ClearFlags_depth, {}, 1);
		shared->current_viewport = aabb_min_max({}, (v2s)window.client_size);
		shared->tg->set_viewport(window.client_size);
		render_camera(*main_camera, main_camera->entity());
		shared->tg->present();

		update_time();
	};


	auto window = create_window(info);
	defer { free(window); };

	assert_always(window);

	shared->frame_timer = create_precise_timer();


	while (update(window)) {
	}

	return 0;
}
