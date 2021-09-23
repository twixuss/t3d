#define TGRAPHICS_IMPL
#define TL_IMPL
#define TL_GL_VALIDATE_EACH_CALL
#pragma comment(lib, "freetype.lib")
#include <freetype/freetype.h>
#include <t3d/entity.h>
#include <t3d/common.h>
#include <t3d/component.h>
#include <t3d/gui.h>
#include <t3d/shared.h>
#include <tl/masked_block_list.h>

SharedData *shared;

void allocate_shared() {
	allocate(shared);
	shared->allocator = default_allocator;
}

void set_module_shared(HMODULE module) {
	*(SharedData **)GetProcAddress(module, "shared") = shared;
}

void initialize_module() {
	init_allocator();
	current_printer = console_printer;
}

void update_component_info(ComponentDesc const &desc) {
	auto found_uid = shared->component_name_to_uid.find(desc.name);

	ComponentUID uid = found_uid ? *found_uid : atomic_increment(&shared->component_uid_counter);

	if (found_uid) {
		print("Re-registered component '%' with uid '%'\n", desc.name, uid);\
	} else {
		print("Registered new component '%' with uid '%'\n", desc.name, uid);\
	}

	*desc.uid = uid;

	ComponentInfo info;

	info.name = desc.name;

	info.storage.bytes_per_entry = desc.size;
	info.storage.entry_alignment = desc.alignment;

	info.serialize          = desc.serialize         ;
	info.construct          = desc.construct         ;
	info.deserialize_binary = desc.deserialize_binary;
	info.deserialize_text   = desc.deserialize_text  ;
	info.draw_properties    = desc.draw_properties   ;
	info.free               = desc.free              ;
	info.init               = desc.init              ;
	info.start              = desc.start             ;
	info.update             = desc.update            ;

	shared->component_infos.get_or_insert(uid) = info;
}
