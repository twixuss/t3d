#pragma once
#ifndef ENUMERATE_COMPONENTS
#error You must define ENUMERATE_COMPONENTS before including "components_base.h".
/*
The macro should enumerate every component in this form:

#define ENUMERATE_COMPONENTS \
c(YourComponentType0) sep \
c(YourComponentType1) sep \
c(YourComponentType2)

Here `c` and `sep` are macros that will be defined by users of `ENUMERATE_COMPONENTS`.
where `c` is a macro that takes the name of the component in the argument,
and `sep` is a macro that expands into a separator. Note that `sep` should only be between `c`s.
Also note that order of executing a function in a `for_all_components` macro will be as the `ENUMERATE_COMPONENTS` macro defines it.
In this example it will execute a function on every component of type `YourComponentType0`,
then go through every `YourComponentType1`, then all of `YourComponentType2`.
*/
#endif

#include <tl/common.h>

using ComponentIndexBase = tl::u64;

union ComponentIndex {
	ComponentIndexBase value;
	struct {
		ComponentIndexBase type         : 10;
		ComponentIndexBase index        : 22;
		ComponentIndexBase entity_index : 32;
	};
};

struct Component {
	tl::u32 entity_index;
};


#define c(name) struct name;
#define sep

ENUMERATE_COMPONENTS

#undef sep
#undef c


#define c(name) name
#define sep ,

// Make sure every component is listed before including this file
template <class Component>
inline static constexpr tl::u32 component_type_index = tl::type_index<Component,
	ENUMERATE_COMPONENTS
>(0);

inline static constexpr tl::u32 component_type_count = tl::type_count<
	ENUMERATE_COMPONENTS
>();

#undef sep
#undef c


template <class Component>
inline constexpr tl::u32 get_component_type_index() {
	constexpr tl::u32 index = component_type_index<Component>;
	static_assert(index != component_type_count, "attempt to get a unregistered component type");
	return index;
}

template <class Component, class ...Args>
void on_create(Component &component, Args const &...args) {
}


struct ComponentStorage {
	using Mask = tl::umm;
	static constexpr tl::u32 bits_in_mask = sizeof(Mask) * 8;
	static constexpr tl::u32 values_per_block = 256;
	static constexpr tl::u32 masks_per_block = values_per_block / bits_in_mask;

	struct Block {
		tl::umm unfull_mask_count;
		void *data() { return this + 1; }
		tl::Span<Mask> masks() { return {(Mask *)data(), masks_per_block}; }
		void *values() { return (Mask *)data() + masks_per_block; }
	};

	tl::Allocator allocator = tl::current_allocator;
	tl::umm bytes_per_entry = 0;
	tl::List<Block *> blocks;

	struct Added {
		void *pointer;
		tl::u32 index;
	};

	Added add() {
		Added result;

		for (tl::u32 block_index = 0; block_index < blocks.size; block_index += 1) {
			auto block = blocks[block_index];
			if (block->unfull_mask_count == 0)
				continue;

			// Search for free space in current block
			for (tl::u32 mask_index = 0; mask_index < masks_per_block; mask_index += 1) {
				auto &mask = block->masks()[mask_index];

				if (mask == ~0)
					continue;

				auto bit_index = tl::find_lowest_zero_bit(mask);
				auto value_index = (mask_index * bits_in_mask) + bit_index;

				mask |= (Mask)1 << bit_index;

				if (mask == ~0)
					block->unfull_mask_count -= 1;

				result.pointer = (tl::u8 *)block->values() + value_index * bytes_per_entry;
				result.index = block_index * values_per_block;
				return result;
			}
		}

		auto block_index = blocks.size;
		auto block = blocks.add((Block *)allocator.allocate(tl::Allocate_uninitialized, sizeof(Block) + sizeof(Mask) * masks_per_block + bytes_per_entry * values_per_block));
		block->unfull_mask_count = masks_per_block;
		memset(block->masks().data, 0, sizeof(Mask) * masks_per_block);
		block->masks()[0] = 1;

		result.pointer = block->values();
		result.index = block_index * values_per_block;

		return result;
	}
};

ComponentStorage component_storages[component_type_count];

template <class Component>
void init_component_storage(tl::u32 index) {
	auto &storage = component_storages[index];
	tl::construct(storage);
	storage.bytes_per_entry = sizeof(Component);
}


template <class First = void, class ...Rest>
void init_component_storages(tl::umm index = 0) {
	init_component_storage<First>(index);
	init_component_storages<Rest...>(index + 1);
}

template <>
void init_component_storages<void>(tl::umm index) {
}

template <class Component>
struct ComponentStorageIterator {
	template <class Fn>
	ComponentStorageIterator(Fn &&fn) {
		static constexpr tl::u32 component_type = get_component_type_index<Component>();
		auto &storage = component_storages[component_type];
		for (auto block : storage.blocks) {
			if (block->unfull_mask_count == 0)
				continue;

			tl::u32 mask_index = 0;
			for (auto mask : block->masks()) {
				if (mask == 0)
					continue;
				for (tl::u32 bit_index = 0; bit_index != storage.bits_in_mask; bit_index += 1) {
					if (mask & ((ComponentStorage::Mask)1 << bit_index)) {
						fn(((Component *)block->values())[mask_index * storage.bits_in_mask + bit_index]);
					}
				}
				++mask_index;
			}
		}
	}
};

#define for_each_component_of_type(type, name) ComponentStorageIterator<type> CONCAT(_component_iterator_, __LINE__) = [&](type &name)
#define for_all_components() ComponentStorageIterator<type> CONCAT(_component_iterator_, __LINE__) = [&](type &name)
