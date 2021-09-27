#define TL_IMPL
#define TL_MAIN
#include <tl/common.h>
#include <tl/file.h>

using namespace tl;

s32 tl_main(Span<Span<utf8>> arguments) {
	current_printer = Printer {
		.func = [](PrintKind kind, Span<utf8> span, void *state) {
			write({state}, as_bytes(span));
			console_printer(kind, span);
		},
		.state = open_file("after_build_log.txt", {.write = true}).handle,
	};

	Span<utf8> exe_dir = arguments[0];
	while (exe_dir.size && !(exe_dir.back() == '\\' || exe_dir.back() == '/')) {
		exe_dir.size --;
	}

	ListList<utf8> editor_cpp_files;
	for_each_file_recursive(tformat(u8"%../../src/t3d/", exe_dir), [&] (Span<utf8> item) {
		if (ends_with(item, u8".cpp"s)/* && parse_path(parent_directory(item)).name != u8"components"s*/) {
			editor_cpp_files.add(item);
		}
	});
	editor_cpp_files.make_absolute();

	create_directory(concatenate(exe_dir, u8"../../data/obj"s));

	for (auto path : editor_cpp_files) {
		if (!path.size) continue;
		if (path.back()== '\r')
			path.size--;

		auto source = format(u8"%../t3d/%.obj"s, exe_dir, parse_path(path).name);
		auto destination = format(u8"%../../data/obj/%.obj"s, exe_dir, parse_path(path).name);
		print("Copying '%' -> '%'. success = %\n", source, destination, copy_file(source, destination));
	}

	return 0;
}
