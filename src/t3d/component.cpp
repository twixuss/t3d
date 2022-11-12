#include <t3d/component.h>
#include <t3d/app.h>
#include <t3d/components/camera.h>
#include <t3d/components/light.h>
#include <t3d/components/mesh_renderer.h>
#include <tl/thread.h>

ComponentStorage::ComponentStorage() {
	blocks.allocator = allocator;
}

ComponentStorage::Added ComponentStorage::add() {
	Added result;

	for (u32 block_index = 0; block_index < blocks.count; block_index += 1) {
		auto block = blocks[block_index];
		if (block->unfull_mask_count == 0)
			continue;

		// Search for free space in current block
		for (u32 mask_index = 0; mask_index < masks_per_block; mask_index += 1) {
			auto &mask = block->masks[mask_index];

			if (mask == ~0)
				continue;

			auto bit_index = find_lowest_zero_bit(mask);
			auto value_index = (mask_index * bits_in_mask) + bit_index;

			mask |= (Mask)1 << bit_index;

			if (mask == ~0)
				block->unfull_mask_count -= 1;

			result.pointer = (u8 *)block->values + value_index * bytes_per_entry;
			result.index = block_index * values_per_block + value_index;
			return result;
		}
	}

	auto block = allocator.allocate<Block>();
	block->values = allocator.allocate_uninitialized(bytes_per_entry * values_per_block, entry_alignment);

	block->masks[0] = 1;

	result.index = blocks.count * values_per_block;
	result.pointer = block->values;

	blocks.add(block);

	return result;
}

void ComponentStorage::remove_at(umm index) {
	auto block_index = index / values_per_block;
	auto value_index = index % values_per_block;

	auto mask_index = value_index / bits_in_mask;
	auto bit_index  = value_index % bits_in_mask;

	auto &block = blocks[block_index];

	auto mask = block->masks[mask_index];
	bounds_check(mask & ((Mask)1 << bit_index), "attempt to remove non-existant component");
	if (mask == ~0) {
		block->unfull_mask_count += 1;
	}
	mask &= ~((Mask)1 << bit_index);
	block->masks[mask_index] = mask;
}

void *ComponentStorage::get(umm index) {
	auto block_index = index / values_per_block;
	auto value_index = index % values_per_block;

	auto mask_index = value_index / bits_in_mask;
	auto bit_index  = value_index % bits_in_mask;

	auto &block = blocks[block_index];

	auto mask = block->masks[mask_index];
	bounds_check(mask & ((Mask)1 << bit_index), "attempt to get non-existant component");

	return (u8 *)block->values + value_index * bytes_per_entry;
}

void ComponentStorage::reallocate(u32 new_size, u32 new_alignment) {
	bytes_per_entry = new_size;
	entry_alignment = new_alignment;
	for (auto block : blocks) {
		allocator.free(block->values);
		block->values = allocator.allocate_uninitialized(new_size * values_per_block, new_alignment);
	}
}

void free(ComponentStorage &storage) {
	for (auto &block : storage.blocks) {
		storage.allocator.free(block->values);
		storage.allocator.free(block);
	}
	free(storage.blocks);
}

ComponentInfo &get_component_info(Uid uid) {
	return app->component_infos.find(uid).get();
}

ComponentInfo &component_infos_get_or_insert(Uid uid) {
	return app->component_infos.get_or_insert(uid);
}

Uid component_name_to_uid(Span<utf8> name) {
	return app->component_name_to_uid.find(name).get();
}
