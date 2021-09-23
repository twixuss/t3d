#define TL_IMPL
#define TL_MAIN
#include <tl/common.h>
#include <tl/file.h>

using namespace tl;

s32 tl_main(Span<Span<utf8>> arguments) {
	current_printer = console_printer;

	Span<utf8> exe_dir = arguments[0];
	while (exe_dir.size && !(exe_dir.back() == '\\' || exe_dir.back() == '/')) {
		exe_dir.size --;
	}

	auto file_path = concatenate(exe_dir, u8"..\\..\\data\\cpp_files.txt"s);

	auto data = as_utf8(read_entire_file(to_pathchars(file_path)));
	if (!data.data) {
		print("Failed to read '%'\n", file_path);
		return false;
	}


	auto paths = split(data, u8"\n"s);

	for (auto path : paths) {
		if (!path.size) continue;

		auto source = format(u8"%..\\t3d\\%.obj"s, exe_dir, parse_path(path).name);
		auto destination = format(u8"%..\\..\\bin\\obj\\%.obj"s, exe_dir, parse_path(path).name);
		print("Copying '%' -> '%'\n", source, destination);
		copy_file(source, destination);
	}

	return 0;
}
