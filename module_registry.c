#include "module_registry.h"
#include "hash_table.h"

static TYPED_HASH_TABLE(GraphNodeSpecification*) registry;
static bool initialized = false;

static void
ensure_initialized()
{
	if (!initialized) {
		hash_table_init(&registry, NULL);
		initialized = true;
	}
}

void
register_graph_node_specification(GraphNodeSpecification * spec)
{
	ensure_initialized();
	if (!spec->name) {
		return;
	}
	hash_table_insert(&registry, hash_table_key_from_cstr(spec->name), &spec);
}

GraphNodeSpecification *
lookup_graph_node_specification(const char * name)
{
	ensure_initialized();
	HashTableIndex idx = hash_table_find(&registry, hash_table_key_from_cstr(name));
	if (idx < 0) {
		return NULL;
	}
	return registry.value_array[idx];
}

void
__attribute__((destructor)) void
destroy_graph_node_specification_registry()
{
	ensure_initialized();
	hash_table_deinit(&registry);
	initialized = false;
}
