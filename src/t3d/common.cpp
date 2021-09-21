#define TGRAPHICS_IMPL
#define TL_IMPL
#define TL_GL_VALIDATE_EACH_CALL
#pragma comment(lib, "freetype.lib")
#include <freetype/freetype.h>
#include <t3d/entity.h>
#include <t3d/common.h>
#include <t3d/component.h>
#include <t3d/gui.h>
#include <t3d/shared_data.h>
#include <tl/masked_block_list.h>

SharedData *shared_data;

void allocate_shared_data() {
	allocate(shared_data);
	shared_data->allocator = default_allocator;
}

void set_module_shared_data(HMODULE module) {
	*(SharedData **)GetProcAddress(module, "shared_data") = shared_data;
}

void initialize_module() {
	current_allocator = default_allocator;
	current_printer = console_printer;
}
