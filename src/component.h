#pragma once
#ifndef ENUMERATE_COMPONENTS
#error You must define ENUMERATE_COMPONENTS before including "component.h".
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
#include <tl/quaternion.h>

using namespace tl;

using ComponentIndexBase = u64;

union ComponentIndex {
	ComponentIndexBase value;
	struct {
		ComponentIndexBase type         : 10;
		ComponentIndexBase index        : 22;
		ComponentIndexBase entity_index : 32;
	};
};

struct Entity;

struct Component {
	u32 entity_index = -1;
	Entity &entity() const;

	//
	// These functions are not called and only needed to check if derived component overrides them
	//
	void init() { invalid_code_path(); }
	void start() { invalid_code_path(); }
	void update() { invalid_code_path(); }
	void free() { invalid_code_path(); }
};

#define is_statically_overridden(function, derived, base) (&base::function != &derived::function)

#define c(name) struct name;
#define sep

ENUMERATE_COMPONENTS

#undef sep
#undef c


#define c(name) name
#define sep ,

// Make sure every component is listed before including this file
template <class Component>
inline static constexpr u32 component_type_index = type_index<Component,
	ENUMERATE_COMPONENTS
>(0);

inline static constexpr u32 component_type_count = type_count<
	ENUMERATE_COMPONENTS
>();

#undef sep
#undef c


using TokenKind = u16;
enum : TokenKind {
	Token_identifier = 0x100,
	Token_number,
	Token_null,
	Token_entity,
};

struct Token {
	TokenKind kind = {};
	Span<utf8> string;
};


using ComponentSerialize = void(*)(StringBuilder &builder, void *component, bool binary);
#if T3D_EDITOR
using ComponentDeserialize = bool(*)(Token *&from, Token *end, void *component);
#else
using ComponentDeserialize = bool(*)(u8 *&from, u8 *end, void *component);
#endif
using ComponentDrawProperties = void(*)(void *component);
using ComponentConstruct = void(*)(void *component);
using ComponentInit = void(*)(void *component);
using ComponentFree = void(*)(void *component);

struct ComponentInfo {
	ComponentSerialize serialize;
	ComponentDeserialize deserialize;
	ComponentDrawProperties draw_properties;
	ComponentConstruct construct;
	ComponentInit init;
	ComponentFree free;
	Span<utf8> name;
};

template <class Component>
void adapt_component_serializer(StringBuilder &builder, void *component, bool binary) {
	return ((Component *)component)->serialize(builder, binary);
}
#if T3D_EDITOR
template <class Component>
bool adapt_component_deserializer(Token *&from, Token *end, void *component) {
	return ((Component *)component)->deserialize(from, end);
}
#else
template <class Component>
bool adapt_component_deserializer(u8 *&from, u8 *end, void *component) {
	return ((Component *)component)->deserialize(from, end);
}
#endif
template <class Component>
void adapt_component_property_drawer(void *component) {
	return ((Component *)component)->draw_properties();
}

template <class Component>
void adapt_component_construct(void *component) {
	assert(((umm)component % alignof(Component)) == 0);
	new (component) Component();
}
template <class Component>
void adapt_component_init(void *component) {
	if constexpr (is_statically_overridden(init, Component, ::Component)) {
		return ((Component *)component)->init();
	}
}
template <class Component>
void adapt_component_free(void *component) {
	if constexpr (is_statically_overridden(free, Component, ::Component)) {
		return ((Component *)component)->free();
	}
}

#if T3D_EDITOR
#define PROPERTY_DRAWER_ADAPTOR(component) adapt_component_property_drawer<component>
#else
#define PROPERTY_DRAWER_ADAPTOR(component) 0
#endif

#define c(component) { \
	.serialize       = adapt_component_serializer<component>, \
	.deserialize     = adapt_component_deserializer<component>, \
	.draw_properties = PROPERTY_DRAWER_ADAPTOR(component), \
	.construct       = adapt_component_construct<component>, \
	.init            = adapt_component_init<component>, \
	.free            = adapt_component_free<component>, \
	.name            = u8#component##s, \
}
#define sep ,
ComponentInfo component_info[] = {
	ENUMERATE_COMPONENTS
};
#undef sep
#undef c

template <class Component>
inline constexpr u32 get_component_type_index() {
	constexpr u32 index = component_type_index<Component>;
	static_assert(index != component_type_count, "attempt to get a unregistered component type");
	return index;
}

struct ComponentStorage {
	using Mask = umm;
	static constexpr u32 bits_in_mask = sizeof(Mask) * 8;
	static constexpr u32 values_per_block = 256;
	static constexpr u32 masks_per_block = values_per_block / bits_in_mask;

	struct Block {
		umm unfull_mask_count;
		Mask *masks;
		void *values;
	};

	Allocator allocator = current_allocator;
	u32 bytes_per_entry = 0;
	u32 entry_alignment = 0;
	List<Block *> blocks;

	struct Added {
		void *pointer;
		u32 index;
	};

	Added add() {
		Added result;

		for (u32 block_index = 0; block_index < blocks.size; block_index += 1) {
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

		umm memory_aligment = max(alignof(Block), entry_alignment);

		umm memory_size = ceil(sizeof(Block) + sizeof(Mask) * masks_per_block + bytes_per_entry * values_per_block, memory_aligment);

		auto memory = (u8 *)allocator.allocate_uninitialized(memory_size, memory_aligment);

		auto block_index = blocks.size;
		auto block = blocks.add((Block *)memory);

		block->masks = (Mask *)((u8 *)block + sizeof(Block));

		block->values = ceil((u8 *)block->masks + sizeof(Mask) * masks_per_block, entry_alignment);

		assert((u8 *)block->values + bytes_per_entry * values_per_block <= memory + memory_size);

		block->unfull_mask_count = masks_per_block;
		memset(block->masks, 0, sizeof(Mask) * masks_per_block);
		block->masks[0] = 1;

		result.pointer = block->values;
		result.index = block_index * values_per_block;

		return result;
	}

	void remove_at(ComponentIndexBase index) {
		auto block_index = index / values_per_block;
		auto value_index = index % values_per_block;

		auto mask_index = value_index / bits_in_mask;
		auto bit_index  = value_index % bits_in_mask;

		auto &block = blocks[block_index];

		auto mask = block->masks[mask_index];
		bounds_check(mask & ((Mask)1 << bit_index), "attempt to remove non-existant component");
		mask &= ~((Mask)1 << bit_index);
		block->masks[mask_index] = mask;
	}

	void *get(umm index) {
		auto block_index = index / values_per_block;
		auto value_index = index % values_per_block;

		auto mask_index = value_index / bits_in_mask;
		auto bit_index  = value_index % bits_in_mask;

		auto &block = blocks[block_index];

		auto mask = block->masks[mask_index];
		bounds_check(mask & ((Mask)1 << bit_index), "attempt to get non-existant component");

		return (u8 *)block->values + value_index * bytes_per_entry;
	}
};

ComponentStorage component_storages[component_type_count];

template <class Component>
void init_component_storage(u32 index) {
	auto &storage = component_storages[index];
	construct(storage);
	storage.bytes_per_entry = sizeof(Component);
	storage.entry_alignment = alignof(Component);
}


template <class First = void, class ...Rest>
void init_component_storages(umm index = 0) {
	init_component_storage<First>(index);
	init_component_storages<Rest...>(index + 1);
}

template <>
void init_component_storages<void>(umm index) {
}

template <class Component>
struct ComponentStorageIterator {
	template <class Fn>
	ComponentStorageIterator(Fn &&fn) {
		using FnRet = decltype(fn(*(Component*)0));

		static constexpr u32 component_type = get_component_type_index<Component>();
		auto &storage = component_storages[component_type];
		for (auto block : storage.blocks) {
			if (block->unfull_mask_count == 0)
				continue;

			u32 mask_index = 0;
			for (u32 mask_index = 0; mask_index < storage.masks_per_block; mask_index += 1) {
				auto &mask = block->masks[mask_index];
				if (mask == 0)
					continue;
				for (u32 bit_index = 0; bit_index != storage.bits_in_mask; bit_index += 1) {
					if (mask & ((ComponentStorage::Mask)1 << bit_index)) {
						auto &value = ((Component *)block->values)[mask_index * storage.bits_in_mask + bit_index];

						if constexpr (is_same<FnRet, void>) {
							fn(value);
						} else if constexpr (is_same<FnRet, ForEachDirective>) {
							if (fn(value) == ForEach_break) {
								return;
							}
						} else {
							static_assert(false, "iteration function must return either void or ForEachDirective (use for_each_continue/for_each_break macros for that)");
						}
					}
				}
				++mask_index;
			}
		}
	}
};

void free_component_storages() {
	for (auto &storage : component_storages) {
		for (auto &block : storage.blocks) {
			storage.allocator.free(block);
		}
		free(storage.blocks);
	}
}

#define for_each_component_of_type(type, name) ComponentStorageIterator<type> CONCAT(_component_iterator_, __COUNTER__) = [&](type &name)
#define for_all_components() ComponentStorageIterator<type> CONCAT(_component_iterator_, __COUNTER__) = [&](type &name)

struct Mesh;
struct Material;

// TODO: this should not be here... fucking c++
void draw_property(Span<utf8> name, f32 &value, umm id = 0, std::source_location location = std::source_location::current());
void draw_property(Span<utf8> name, v3f &value, umm id = 0, std::source_location location = std::source_location::current());
void draw_property(Span<utf8> name, quaternion &value, umm id = 0, std::source_location location = std::source_location::current());
void draw_property(Span<utf8> name, List<utf8> &value, umm id = 0, std::source_location location = std::source_location::current());
void draw_property(Span<utf8> name, tg::Texture2D *&value, umm id = 0, std::source_location location = std::source_location::current());
void draw_property(Span<utf8> name, Mesh *&value, umm id = 0, std::source_location location = std::source_location::current());

void serialize(StringBuilder &builder, f32 value, bool binary);
void serialize(StringBuilder &builder, v3f value, bool binary);
void serialize(StringBuilder &builder, Mesh *value, bool binary);
void serialize(StringBuilder &builder, Material *value, bool binary);
void serialize(StringBuilder &builder, tg::Texture2D *value, bool binary);

#if T3D_EDITOR
bool deserialize(f32 &value, Token *&from, Token *end);
bool deserialize(v3f &value, Token *&from, Token *end);
bool deserialize(Mesh *&value, Token *&from, Token *end);
bool deserialize(Material *&value, Token *&from, Token *end);
bool deserialize(tg::Texture2D *&value, Token *&from, Token *end);
#else
bool deserialize(f32 &value, u8 *&from, u8 *end);
bool deserialize(v3f &value, u8 *&from, u8 *end);
bool deserialize(Mesh *&value, u8 *&from, u8 *end);
bool deserialize(Material *&value, u8 *&from, u8 *end);
bool deserialize(tg::Texture2D *&value, u8 *&from, u8 *end);
#endif

template <class Derived>
struct ComponentBase : Component {};

#define DECLARE_FIELD(type, name, default) type name = default;

#define SERIALIZE_FIELD(type, name, default) \
if (binary) { \
	::serialize(builder, name, true); \
} else { \
	append(builder, "\t\t" #name " "); \
	::serialize(builder, name, false); \
	append(builder, "\n"); \
}


#define DRAW_FIELD(type, name, default) \
draw_property(u8#name##s, name, __COUNTER__); \


#if T3D_EDITOR

#define DRAW_PROPERTIES \
	void draw_properties() { \
		FIELDS(DRAW_FIELD) \
	}

#define DESERIALIZE_FIELD(type, name, default) \
else if (from->string == u8#name##s) { \
	from += 1; \
	if (from == end) { \
		print(Print_error, "Unexpected end of file while parsing property '" #name "'\n"); \
		return false; \
	} \
	if (!::deserialize(name, from, end)) return false; \
}

#define DESERIALIZE(name) \
	bool deserialize(Token *&from, Token *end) { \
		while (from->kind != '}') { \
			if (from->kind != Token_identifier) { \
				print(Print_error, "Expected an identifier while parsing " #name "'s properties, but got '%'\n", from->string); \
				return false; \
			} \
			if (false) {} \
			FIELDS(DESERIALIZE_FIELD) \
		} \
		from += 1; \
		return true; \
	}

#else

#define DRAW_PROPERTIES

#define DESERIALIZE_FIELD(type, name, default) \
	if (!::deserialize(name, from, end)) return false;

#define DESERIALIZE(name) \
	bool deserialize(u8 *&from, u8 *end) { \
		FIELDS(DESERIALIZE_FIELD) \
		return true; \
	}

#endif


#define DECLARE_COMPONENT(name) \
template <> \
struct ComponentBase<name> : Component { \
	static u32 type_index; \
	FIELDS(DECLARE_FIELD) \
	void serialize(StringBuilder &builder, bool binary) { \
		FIELDS(SERIALIZE_FIELD) \
	} \
	DESERIALIZE(name) \
	DRAW_PROPERTIES \
};  \
struct name : ComponentBase<name>
