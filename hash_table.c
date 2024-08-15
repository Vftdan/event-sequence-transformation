#include <stdlib.h>
#include "hash_table.h"

#define FNV_OFFSET_BASIS 0xCBF29CE484222325
#define FNV_PRIME 0x00000100000001B3

inline static uint64_t
fnv_1a(uint8_t *bytes, size_t length)
{
	uint64_t hash = FNV_OFFSET_BASIS;
	for (size_t i = 0; i < length; ++i) {
		uint8_t b = bytes[i];
		hash ^= b;
		hash *= FNV_PRIME;
	}
	return hash;
}

#define NULL_KEY ((HashTableKey){.length = 0, .bytes = NULL, .pre_hash = 0})

HashTableKey
hash_table_key_from_bytes(const char *bytes, size_t size)
{
	if (!bytes) {
		return NULL_KEY;
	}
	HashTableKey key = {
		.length = size,
		.bytes = bytes,
		.pre_hash = fnv_1a((uint8_t*) bytes, size),
	};
	return key;
}

HashTableKey
hash_table_key_copy(const HashTableKey old)
{
	if (!old.bytes) {
		return NULL_KEY;
	}
	char *new_bytes = malloc(old.length + 1);
	if (!new_bytes) {
		return NULL_KEY;
	}
	memcpy(new_bytes, old.bytes, old.length);
	new_bytes[old.length] = 0;
	return (HashTableKey) {
		.length = old.length,
		.bytes = new_bytes,
		.pre_hash = old.pre_hash,
	};
}

void
hash_table_key_deinit_copied(HashTableKey *key)
{
	if (key->bytes) {
		free((char*) key->bytes);
		key->bytes = NULL;
	}
	key->length = 0;
	key->pre_hash = 0;
}

#define FAMILY_PRIME 0x1FFFFFFFFFFFFFFF

inline static HashFamilyMember
family_random_member()
{
	uint64_t n[2];
	for (int i = 0; i < 2; ++i) {
		n[i] = (uint64_t)((rand() / (double) RAND_MAX) * (FAMILY_PRIME - RAND_MAX) + rand()) % FAMILY_PRIME;
	}
	if (!n[0]) {
		n[0] = 1;
	}
	return (HashFamilyMember) {
		.a = n[0],
		.b = n[1],
	};
}

inline static size_t
family_map(HashFamilyMember member, uint64_t pre_hash)
{
	if (!member.a)
		++member.a;
	uint64_t hash = (pre_hash * member.a + member.b) % FAMILY_PRIME;  // Integer overflows may influence the properties of this family
	return hash;
}

bool
hash_table_key_equals(const HashTableKey lhs, const HashTableKey rhs)
{
	if (lhs.pre_hash != rhs.pre_hash) {
		return false;
	}
	if (lhs.length != rhs.length) {
		return false;
	}
	if (lhs.bytes == rhs.bytes) {
		return true;
	}
	if (!lhs.bytes || !rhs.bytes) {
		return false;
	}
	size_t length = lhs.length;
	return memcmp(lhs.bytes, rhs.bytes, length) == 0;
}

inline static size_t
initial_position(const HashTableKey key, const HashTableDynamicData * ht)
{
	uint64_t hash = family_map(ht->family_member, key.pre_hash);
	return hash % ht->capacity;
}

inline static size_t
probe_next(size_t current_index, size_t base_index, size_t iteration, size_t modulo)
{
	(void) base_index;
	(void) iteration;
	++current_index;
	current_index %= modulo;
	return current_index;
}

static HashTableDynamicData
create_table(size_t capacity, size_t value_size, HashFamilyMember family_member)
{
	HashTableDynamicData data = {
		.value_array = NULL,
		.capacity = 0,
		.length = 0,
		.key_array = NULL,
		.family_member = family_member,
		.value_size = value_size,
	};
	void *value_array = calloc(capacity, value_size);
	if (!value_array) {
		return data;
	}
	HashTableKeyEntry *key_array = T_ALLOC(capacity, HashTableKeyEntry);
	if (!key_array) {
		free(value_array);
		return data;
	}
	data.value_array = value_array;
	data.capacity = capacity;
	data.key_array = key_array;
	return data;
}

void
hash_table_init_impl(HashTableDynamicData * data, size_t value_size, void (*value_deinit)(void*))
{
	*data = create_table(5, value_size, family_random_member());
	data->value_deinit = value_deinit;
}

void
hash_table_deinit_impl(HashTableDynamicData * data)
{
	if (data->key_array && data->value_array && data->length) {
		void (*value_deinit)(void*) = data->value_deinit;
		for (HashTableIndex i = 0; i < (ssize_t) data->capacity; ++i) {
			if (data->key_array[i].key.bytes) {
				if (value_deinit) {
					value_deinit(data->value_array + (data->value_size * (size_t) i));
				}
				hash_table_key_deinit_copied(&data->key_array[i].key);
			}
		}
	}
	if (data->key_array) {
		free(data->key_array);
		data->key_array = NULL;
	}
	if (data->value_array) {
		free(data->value_array);
		data->value_array = NULL;
	}
	data->capacity = 0;
	data->length = 0;
}

static void
hash_table_grow(HashTableDynamicData * old_ht)
{
	size_t capacity = old_ht->capacity;
	capacity += (capacity >> 1) + 1;
	const size_t value_size = old_ht->value_size;
	HashTableDynamicData new_ht = create_table(capacity, value_size, old_ht->family_member);
	if (!new_ht.key_array) {
		return;
	}

	for (size_t i = 0; i < old_ht->capacity; ++i) {
		if (!old_ht->key_array[i].key.bytes) {
			continue;
		}
		if (hash_table_insert_impl(&new_ht, old_ht->key_array[i].key, old_ht->value_array + (i * value_size)) < 0) {
			hash_table_deinit_impl(&new_ht);
			return;
		}
	}

	new_ht.value_deinit = old_ht->value_deinit;
	old_ht->value_deinit = NULL;
	// TODO avoid duplication-deletion of the keys
	hash_table_deinit_impl(old_ht);
	*old_ht = new_ht;
}

static void
hash_table_change_hash(HashTableDynamicData * old_ht)
{
	const size_t capacity = old_ht->capacity;
	const size_t value_size = old_ht->value_size;
	HashTableDynamicData new_ht = create_table(capacity, value_size, family_random_member());
	if (!new_ht.key_array) {
		return;
	}

	for (size_t i = 0; i < old_ht->capacity; ++i) {
		if (!old_ht->key_array[i].key.bytes) {
			continue;
		}
		if (hash_table_insert_impl(&new_ht, old_ht->key_array[i].key, old_ht->value_array + (i * value_size)) < 0) {
			hash_table_deinit_impl(&new_ht);
			return;
		}
	}

	new_ht.value_deinit = old_ht->value_deinit;
	old_ht->value_deinit = NULL;
	// TODO avoid duplication-deletion of the keys
	hash_table_deinit_impl(old_ht);
	*old_ht = new_ht;
}

HashTableIndex
hash_table_insert_impl(HashTableDynamicData * ht, const HashTableKey key, const void * value_ptr)
{
	if (!key.bytes) {
		return -1;
	}
	size_t length = ht->length;
	size_t capacity = ht->capacity;
	if (length + (length >> 1) >= ht->capacity) {
		hash_table_grow(ht);
		capacity = ht->capacity;
	}
	if (length >= capacity) {
		return -1;
	}

	const size_t base_index = initial_position(key, ht);
	size_t current_index = base_index;
	size_t collision_offset = 0;
	bool found = false;
	bool overwrite = false;
	bool pre_hash_collision = false;
	for (; collision_offset < capacity; current_index = probe_next(current_index, base_index, ++collision_offset, capacity)) {
		if (!ht->key_array[current_index].key.bytes) {
			found = true;
			break;
		}
		if (hash_table_key_equals(key, ht->key_array[current_index].key)) {
			overwrite = true;
			found = true;
			break;
		}
		if (!pre_hash_collision) {
			pre_hash_collision = key.pre_hash == ht->key_array[current_index].key.pre_hash;
		}
	}
	if (!found) {
		return -1;
	}

	void *value_target_ptr = ht->value_array + (ht->value_size * current_index);
	if (ht->key_array[base_index].max_collision_offset < collision_offset) {
		ht->key_array[base_index].max_collision_offset = collision_offset;
	}
	if (overwrite) {
		// Deinitialize the old value
		void (*value_deinit)(void*) = ht->value_deinit;
		if (value_deinit) {
			value_deinit(value_target_ptr);
		}
	} else {
		// Make a local copy of the key
		ht->key_array[current_index].key = hash_table_key_copy(key);
	}
	memcpy(value_target_ptr, value_ptr, ht->value_size);  // Assign the value
	ht->length = ++length;

	if (!pre_hash_collision && (collision_offset << 1) > capacity + 6) {
		hash_table_change_hash(ht);
	}

	return current_index;
}

HashTableIndex
hash_table_find_impl(const HashTableDynamicData * ht, const HashTableKey key)
{
	if (!key.bytes) {
		return -1;
	}

	const size_t capacity = ht->capacity;
	const size_t base_index = initial_position(key, ht);
	const size_t max_collision_offset = ht->key_array[base_index].max_collision_offset;
	size_t collision_offset = 0;
	size_t current_index = base_index;

	for (; collision_offset <= max_collision_offset; current_index = probe_next(current_index, base_index, ++collision_offset, capacity)) {
		if (hash_table_key_equals(key, ht->key_array[current_index].key)) {
			return current_index;
			break;
		}
	}

	return -1;
}
