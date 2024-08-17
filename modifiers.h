#ifndef MODIFIERS_H_
#define MODIFIERS_H_

#include "defs.h"
#include <string.h>

typedef struct {
	size_t byte_length;
	uint8_t *bits;
} ModifierSet;

typedef int32_t Modifier;

#define EMPTY_MODIFIER_SET ((ModifierSet) {.byte_length = 0, .bits = NULL})

__attribute__((unused)) inline static ModifierSet
modifier_set_copy(const ModifierSet old)
{
	ModifierSet result = old;
	result.bits = malloc(result.byte_length);
	if (!result.bits) {
		result.byte_length = 0;
		return result;
	}
	memcpy(result.bits, old.bits, result.byte_length);
	return result;
}

__attribute__((unused)) inline static void
modifier_set_destruct(ModifierSet * old)
{
	if (old->bits) {
		free(old->bits);
	}
	old->bits = NULL;
	old->byte_length = 0;
}

__attribute__((unused)) inline static bool
modifier_set_extend(ModifierSet * old, size_t new_byte_length)
{
	if (new_byte_length > old->byte_length) {
		uint8_t *bits = realloc(old->bits, new_byte_length);
		if (!bits) {
			return false;
		}
		memset(bits + old->byte_length, 0, new_byte_length - old->byte_length);
		old->bits = bits;
		old->byte_length = new_byte_length;
	}
	return true;
}

__attribute__((unused)) inline static void
modifier_set_set_from(ModifierSet * target, const ModifierSet source)
{
	modifier_set_extend(target, source.byte_length);
	for (size_t i = 0; i < target->byte_length; ++i) {
		if (i >= source.byte_length) {
			return;
		}
		target->bits[i] |= source.bits[i];
	}
}

__attribute__((unused)) inline static void
modifier_set_unset_from(ModifierSet * target, const ModifierSet source)
{
	for (size_t i = 0; i < target->byte_length; ++i) {
		if (i >= source.byte_length) {
			return;
		}
		target->bits[i] &= ~source.bits[i];
	}
}

__attribute__((unused)) inline static void
modifier_set_toggle_from(ModifierSet * target, const ModifierSet source)
{
	modifier_set_extend(target, source.byte_length);
	for (size_t i = 0; i < target->byte_length; ++i) {
		if (i >= source.byte_length) {
			return;
		}
		target->bits[i] ^= source.bits[i];
	}
}

__attribute__((unused)) inline static void
modifier_index_and_mask(Modifier modifier, size_t * byte_index, uint8_t * mask)
{
	if (byte_index) {
		*byte_index = modifier >> 3;
	}
	if (mask) {
		*mask = 1 << (modifier & 7);
	}
}

__attribute__((unused)) inline static bool
modifier_set_has(ModifierSet collection, Modifier element)
{
	size_t byte_index;
	uint8_t mask;
	modifier_index_and_mask(element, &byte_index, &mask);
	if (byte_index >= collection.byte_length) {
		return false;
	}
	return (collection.bits[byte_index] & mask) != 0;
}

__attribute__((unused)) inline static void
modifier_set_set(ModifierSet * target, Modifier element)
{
	size_t byte_index;
	uint8_t mask;
	modifier_index_and_mask(element, &byte_index, &mask);
	modifier_set_extend(target, byte_index + 1);
	target->bits[byte_index] |= mask;
}

__attribute__((unused)) inline static void
modifier_set_unset(ModifierSet * target, Modifier element)
{
	size_t byte_index;
	uint8_t mask;
	modifier_index_and_mask(element, &byte_index, &mask);
	if (byte_index >= target->byte_length) {
		return;
	}
	target->bits[byte_index] &= ~mask;
}

__attribute__((unused)) inline static void
modifier_set_toggle(ModifierSet * target, Modifier element)
{
	size_t byte_index;
	uint8_t mask;
	modifier_index_and_mask(element, &byte_index, &mask);
	modifier_set_extend(target, byte_index + 1);
	target->bits[byte_index] ^= mask;
}

#endif /* end of include guard: MODIFIERS_H_ */
