#include <linux/input.h>
#include <linux/input-event-codes.h>
#include "event_code_names.h"

void
populate_event_codes(ConstantRegistry * registry)
{
	if (!registry) {
		return;
	}

#define DECLARE_EVENT_CODE(enum_name, prefix_name, unqualified_name) { \
	const long long value = prefix_name##_##unqualified_name; \
	hash_table_insert(registry, hash_table_key_from_cstr(#prefix_name "_" #unqualified_name), &value); \
	hash_table_insert(registry, hash_table_key_from_cstr(#enum_name "." #unqualified_name), &value); \
}

#include "event_code_names.cc"

}
