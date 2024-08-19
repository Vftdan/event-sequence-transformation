#ifndef HASH_TABLE_H_
#define HASH_TABLE_H_

#include <string.h>
#include "defs.h"

#define HASH_TABLE_INTERFACE_FIELDS \
	size_t capacity; \
	size_t length; \
	HashTableKeyEntry *key_array; \
	HashFamilyMember family_member; \
;

#define TYPED_HASH_TABLE(T) union { HashTableDynamicData as_HashTableDynamicData; struct { typeof(T) *value_array; HASH_TABLE_INTERFACE_FIELDS; void (*value_deinit)(typeof(T)*); }; }

typedef ssize_t HashTableIndex;  // Invalidated after hashtable modification

typedef struct {
	size_t length;
	const char *bytes;
	uint64_t pre_hash;
} HashTableKey;

typedef struct {
	HashTableKey key;
	size_t max_collision_offset;
} HashTableKeyEntry;

typedef struct {
	uint64_t a, b;
} HashFamilyMember;

typedef struct {
	void *value_array;
	HASH_TABLE_INTERFACE_FIELDS;
	void (*value_deinit)(void*);
	size_t value_size;
} HashTableDynamicData;

HashTableKey hash_table_key_from_bytes(const char *bytes, size_t size);
HashTableKey hash_table_key_copy(const HashTableKey old);
void hash_table_key_deinit_copied(HashTableKey *key);
bool hash_table_key_equals(const HashTableKey lhs, const HashTableKey rhs);

__attribute__((unused)) inline static HashTableKey
hash_table_key_from_cstr(const char *s)
{
	size_t length = strlen(s);
	return hash_table_key_from_bytes(s, length);
}

void hash_table_init_impl(HashTableDynamicData * dyndata, size_t value_size, void (*value_deinit)(void*));
void hash_table_deinit_impl(HashTableDynamicData * dyndata);
HashTableIndex hash_table_insert_impl(HashTableDynamicData * dyndata, const HashTableKey key, const void * value_ptr);
HashTableIndex hash_table_find_impl(const HashTableDynamicData * dyndata, const HashTableKey key);
bool hash_table_delete_at_index_impl(HashTableDynamicData * dyndata, const HashTableIndex index);

__attribute__((unused)) inline static bool
hash_table_delete_by_key_impl(HashTableDynamicData * dyndata, const HashTableKey key)
{
	return hash_table_delete_at_index_impl(dyndata, hash_table_find_impl(dyndata, key));
}

#define hash_table_init(ht, value_deinit) hash_table_init_impl(&(ht)->as_HashTableDynamicData, sizeof(*(ht)->value_array), IMPLICIT_CAST(void(void*), void(typeof((ht)->value_array)), value_deinit))
#define hash_table_deinit(ht) hash_table_deinit_impl(&(ht)->as_HashTableDynamicData)
#define hash_table_insert(ht, key, value_ptr) hash_table_insert_impl(&(ht)->as_HashTableDynamicData, key, IMPLICIT_CAST(const void, const typeof(*(ht)->value_array), value_ptr))
#define hash_table_find(ht, key) hash_table_find_impl(&(ht)->as_HashTableDynamicData, key)
#define hash_table_delete_at_index(ht, index) hash_table_delete_at_index_impl(&(ht)->as_HashTableDynamicData, index)
#define hash_table_delete_by_key(ht, key) hash_table_delete_by_key_impl(&(ht)->as_HashTableDynamicData, key)

#endif /* end of include guard: HASH_TABLE_H_ */
