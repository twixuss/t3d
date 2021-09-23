#pragma once

#include <t3d/common.h>
#include <t3d/draw_property.h>
#include <tl/common.h>
#include <tl/quaternion.h>
#include <tl/hash_map.h>

using namespace tl;

using ComponentUID = u32;

struct ComponentIndex {
	ComponentUID type;
	u32 index;
	u32 entity_index;

	bool operator==(ComponentIndex const &that) const {
		return memcmp(this, &that, sizeof(*this)) == 0;
	}
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

	void remove_at(umm index) {
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
	template <class Fn>
	void for_each(Fn &&fn) {
		using FnRet = decltype(fn((void*)0));

		for (auto block : blocks) {
			if (block->unfull_mask_count == 0)
				continue;

			u32 mask_index = 0;
			for (u32 mask_index = 0; mask_index < masks_per_block; mask_index += 1) {
				auto &mask = block->masks[mask_index];
				if (mask == 0)
					continue;
				for (u32 bit_index = 0; bit_index != bits_in_mask; bit_index += 1) {
					if (mask & ((Mask)1 << bit_index)) {
						auto value = (u8 *)block->values + bytes_per_entry * (mask_index * bits_in_mask + bit_index);

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


using ComponentSerialize         = void(*)(StringBuilder &builder, void *component, bool binary);
using ComponentDeserializeText   = bool(*)(Token *&from, Token *end, void *component);
using ComponentDeserializeBinary = bool(*)(u8 *&from, u8 *end, void *component);
using ComponentDrawProperties    = void(*)(void *component);
using ComponentConstruct         = void(*)(void *component);
using ComponentInit              = void(*)(void *component);
using ComponentStart             = void(*)(void *component);
using ComponentUpdate            = void(*)(void *component);
using ComponentFree              = void(*)(void *component);

struct ComponentDesc {
	ComponentSerialize serialize;
	ComponentDeserializeText deserialize_text;
	ComponentDeserializeBinary deserialize_binary;
	ComponentDrawProperties draw_properties;
	ComponentConstruct construct;
	ComponentInit init;
	ComponentStart start;
	ComponentUpdate update;
	ComponentFree free;
	Span<utf8> name;
	u32 size;
	u32 alignment;
	ComponentUID *uid;
};

struct ComponentInfo {
	ComponentSerialize serialize;
	ComponentDeserializeText deserialize_text;
	ComponentDeserializeBinary deserialize_binary;
	ComponentDrawProperties draw_properties;
	ComponentConstruct construct;
	ComponentInit init;
	ComponentStart start;
	ComponentUpdate update;
	ComponentFree free;
	Span<utf8> name;
	ComponentStorage storage;
};

ComponentInfo &get_component_info(ComponentUID uid);
void free_component_storages();
ComponentInfo &component_infos_get_or_insert(ComponentUID uid);
ComponentUID get_new_component_uid();

template <class Component, class Fn>
void for_each_component(Fn &&fn) {
	ComponentInfo &info = get_component_info(Component::uid);
	info.storage.for_each([&](void *component) {
		return fn(*(Component *)component);
	});
}

template <class Component>
void adapt_component_serializer(StringBuilder &builder, void *component, bool binary) {
	return ((Component *)component)->serialize(builder, binary);
}
template <class Component>
bool adapt_component_deserializer_text(Token *&from, Token *end, void *component) {
	return ((Component *)component)->deserialize_text(from, end);
}
template <class Component>
bool adapt_component_deserializer_binary(u8 *&from, u8 *end, void *component) {
	return ((Component *)component)->deserialize_binary(from, end);
}
template <class Component>
void adapt_component_property_drawer(void *component) {
	return ((Component *)component)->draw_properties();
}

template <class Component>
void adapt_component_construct(void *component) {
	assert(((umm)component % alignof(Component)) == 0);
	new (component) Component();
}

struct Mesh;
struct Material;

void serialize_binary(StringBuilder &builder, f32 value);
void serialize_binary(StringBuilder &builder, v3f value);
void serialize_binary(StringBuilder &builder, Mesh *value);
void serialize_binary(StringBuilder &builder, tg::Texture2D *value);

void serialize_text(StringBuilder &builder, f32 value);
void serialize_text(StringBuilder &builder, v3f value);
void serialize_text(StringBuilder &builder, Mesh *value);
void serialize_text(StringBuilder &builder, tg::Texture2D *value);

bool deserialize_text(f32 &value, Token *&from, Token *end);
bool deserialize_text(v3f &value, Token *&from, Token *end);
bool deserialize_text(Mesh *&value, Token *&from, Token *end);
bool deserialize_text(Material *&value, Token *&from, Token *end);
bool deserialize_text(tg::Texture2D *&value, Token *&from, Token *end);

bool deserialize_binary(f32 &value, u8 *&from, u8 *end);
bool deserialize_binary(v3f &value, u8 *&from, u8 *end);
bool deserialize_binary(Mesh *&value, u8 *&from, u8 *end);
bool deserialize_binary(Material *&value, u8 *&from, u8 *end);
bool deserialize_binary(tg::Texture2D *&value, u8 *&from, u8 *end);

template <class Derived>
struct ComponentBase : Component {};

#define DECLARE_FIELD(type, name, default) type name = default;

#define SERIALIZE_FIELD(type, name, default) \
if (binary) { \
	serialize_binary(builder, name); \
} else { \
	append(builder, "\t\t" #name " "); \
	serialize_text(builder, name); \
	append(builder, "\n"); \
}


#define DRAW_FIELD(type, name, default) \
draw_property(u8#name##s, name, __COUNTER__); \


#define DRAW_PROPERTIES \
	void draw_properties() { \
		FIELDS(DRAW_FIELD) \
	}

template <class T>
bool deserialize_text(Token *&from, Token *end, T &value, Span<utf8> name) {
	from += 1;
	if (from == end) {
		print(Print_error, "Unexpected end of file while parsing property '%'\n", name);
		return false;
	}
	if (!::deserialize_text(value, from, end))
		return false;
	return true;
}


#define DESERIALIZE_FIELD_TEXT(type, name, default) \
else if (from->string == u8#name##s) { \
	if (!::deserialize_text(from, end, name, from->string)) return false; \
}


#define DESERIALIZE_FIELD_BINARY(type, name, default) \
	if (!::deserialize_binary(name, from, end)) return false;


#define DECLARE_COMPONENT(ComponentT) \
struct ComponentT; \
template <> \
struct ComponentBase<ComponentT> : Component { \
	inline static ComponentUID uid; \
	FIELDS(DECLARE_FIELD) \
	void serialize(StringBuilder &builder, bool binary) { \
		FIELDS(SERIALIZE_FIELD) \
	} \
	bool deserialize_text(Token *&from, Token *end) { \
		while (from->kind != '}') { \
			if (from->kind != Token_identifier) { \
				print(Print_error, "Expected an identifier while parsing " #ComponentT "'s properties, but got '%'\n", from->string); \
				return false; \
			} \
			if (false) {} \
			FIELDS(DESERIALIZE_FIELD_TEXT) \
		} \
		from += 1; \
		return true; \
	} \
	bool deserialize_binary(u8 *&from, u8 *end) { \
		FIELDS(DESERIALIZE_FIELD_BINARY) \
		return true; \
	} \
	DRAW_PROPERTIES \
};  \
ComponentDesc get_component_desc_##ComponentT(); \
struct ComponentT : ComponentBase<ComponentT>

#define REGISTER_COMPONENT(ComponentT) \
ComponentDesc get_component_desc_##ComponentT() { \
	ComponentDesc desc = {}; \
 \
	desc.name = u8#ComponentT##s; \
 \
	desc.size      = sizeof(ComponentT); \
	desc.alignment = alignof(ComponentT); \
 \
	desc.uid = &ComponentT::uid;\
 \
	desc.serialize          = adapt_component_serializer<ComponentT>; \
	desc.deserialize_text   = adapt_component_deserializer_text<ComponentT>; \
	desc.deserialize_binary = adapt_component_deserializer_binary<ComponentT>; \
	desc.draw_properties    = adapt_component_property_drawer<ComponentT>; \
	desc.construct          = adapt_component_construct<ComponentT>; \
 \
	if constexpr (is_statically_overridden(init, ComponentT, ::Component)) { \
		desc.init = [](void *component) { ((ComponentT *)component)->init(); }; \
	} \
	if constexpr (is_statically_overridden(start, ComponentT, ::Component)) { \
		desc.start = [](void *component) { ((ComponentT *)component)->start(); }; \
	} \
	if constexpr (is_statically_overridden(update, ComponentT, ::Component)) { \
		desc.update = [](void *component) { ((ComponentT *)component)->update(); }; \
	} \
	if constexpr (is_statically_overridden(free, ComponentT, ::Component)) { \
		desc.free = [](void *component) { ((ComponentT *)component)->free(); }; \
	} \
	return desc; \
}
