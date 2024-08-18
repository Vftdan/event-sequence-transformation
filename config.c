#include <string.h>
#include "config.h"
#include "event_code_names.h"

static GraphNodeConfig
load_single_node_config(const config_setting_t *config_member)
{
	GraphNodeConfig result = {
		.name = NULL,
		.type = NULL,
		.options = NULL,
	};
	if (!config_member) {
		return result;
	}
	result.name = config_setting_name(config_member);
	if (!result.name) {
		config_setting_lookup_string(config_member, "name", &result.name);
	}
	config_setting_lookup_string(config_member, "type", &result.type);
	result.options = config_setting_get_member(config_member, "options");
	return result;
}

static GraphNodeConfigSection
load_nodes_section(const config_setting_t *config_section)
{
	GraphNodeConfigSection result = {
		.length = 0,
		.items = NULL,
	};
	if (!config_section) {
		return result;
	}
	ssize_t length = config_setting_length(config_section);
	if (length <= 0) {
		return result;
	}
	result.items = T_ALLOC(length, GraphNodeConfig);
	if (!result.items) {
		return result;
	}
	for (ssize_t i = 0; i < length; ++i) {
		result.items[i] = load_single_node_config(config_setting_get_elem(config_section, i));
	}
	result.length = length;
	return result;
}

inline static void
load_channel_end_config(const config_setting_t *config_member, const char **name_ptr, size_t *index_ptr, const ConstantRegistry *constants)
{
	ssize_t length = config_setting_length(config_member);
	if (length < 1) {
		return;
	}
	if (length == 1) {
		config_setting_t *kv = config_setting_get_elem(config_member, 0);
		if (!kv) {
			return;
		}
		*name_ptr = config_setting_name(kv);
		*index_ptr = resolve_constant(constants, kv);
		return;
	}
	*name_ptr = config_setting_get_string_elem(config_member, 0);
	*index_ptr = resolve_constant(constants, config_setting_get_elem(config_member, 1));
}

static GraphChannelConfig
load_single_channel_config(const config_setting_t *config_member, const ConstantRegistry *constants)
{
	GraphChannelConfig result = {
		.from = {NULL, 0},
		.to = {NULL, 0},
	};
	if (!config_member) {
		return result;
	}
	config_setting_t *ends[2];
	bool flip = true;
	const char *flip_keys[2] = {"to", "from"};
	for (int i = 0; i < 2; ++i) {
		if (!(ends[i] = config_setting_get_elem(config_member, i))) {
			return result;
		}
		const char *name = config_setting_name(ends[i]);
		if (!name || strcmp(name, flip_keys[i]) != 0) {
			flip = false;
		}
	}
	if (flip) {
		config_setting_t *tmp = ends[0];
		ends[0] = ends[1];
		ends[1] = tmp;
	}
	load_channel_end_config(ends[0], &result.from.name, &result.from.index, constants);
	load_channel_end_config(ends[1], &result.to.name, &result.to.index, constants);
	return result;
}

static GraphChannelConfigSection
load_channels_section(const config_setting_t *config_section, const ConstantRegistry *constants)
{
	GraphChannelConfigSection result = {
		.length = 0,
		.items = NULL,
	};
	if (!config_section) {
		return result;
	}
	ssize_t length = config_setting_length(config_section);
	if (length <= 0) {
		return result;
	}
	result.items = T_ALLOC(length, GraphChannelConfig);
	if (!result.items) {
		return result;
	}
	for (ssize_t i = 0; i < length; ++i) {
		result.items[i] = load_single_channel_config(config_setting_get_elem(config_section, i), constants);
	}
	result.length = length;
	return result;
}

static void
load_constants_section(const config_setting_t *config_section, ConstantRegistry *constants)
{
	if (!config_section) {
		return;
	}
	ssize_t length = config_setting_length(config_section);
	if (length <= 0) {
		return;
	}
	for (ssize_t i = 0; i < length; ++i) {
		config_setting_t *item = config_setting_get_elem(config_section, i);
		if (!item) {
			continue;
		}
		const char *name;
		long long value;
		if (!(name = config_setting_name(item))) {
			config_setting_lookup_string(item, "name", &name);
		}
		if (!name) {
			continue;
		}
		if (config_setting_is_scalar(item) == CONFIG_TRUE) {
			value = config_setting_get_int64(item);
		} else if (config_setting_is_group(item) == CONFIG_TRUE) {
			if (config_setting_lookup_int64(item, "value", &value) == CONFIG_FALSE) {
				continue;
			}
		} else {
			continue;
		}
		hash_table_insert(constants, hash_table_key_from_cstr(name), &value);
	}
}

static void
put_enum_value(ConstantRegistry *registry, const char *enum_name, const char *member_name, const long long value)
{
	if (!enum_name || !member_name) {
		return;
	}
	size_t enum_len = strlen(enum_name);
	size_t member_len = strlen(member_name);
	size_t mem_size = enum_len + /* "." */ 1 + member_len + /* terminator */ 1 + /* additional protection ??? */ 2;
	if (mem_size <= enum_len || mem_size <= member_len) {
		// Overflow
		return;
	}
	char* qualified_name = malloc(mem_size);
	if (!qualified_name) {
		return;
	}
	qualified_name[0] = '\0';
	strncat(qualified_name, enum_name, enum_len + 1);  // -Wstringop-truncation, why?
	strncat(qualified_name, ".", 1);  // But you don't want 2 here?
	strncat(qualified_name, member_name, member_len + 1);
	hash_table_insert(registry, hash_table_key_from_cstr(qualified_name), &value);
	free(qualified_name);
}

static void
load_single_enum(const char *enum_name, const config_setting_t *enum_def, ConstantRegistry *constants)
{
	ssize_t length = config_setting_length(enum_def);
	if (length <= 0) {
		return;
	}
	long long prev_value = -1;
	for (ssize_t i = 0; i < length; ++i) {
		config_setting_t *member = config_setting_get_elem(enum_def, i);
		if (!member) {
			continue;
		}

		const config_setting_t *value_setting = member;
		long long value = prev_value + 1;  // FIXME in theory, signed overflow is UB
		bool only_name = false;

		const char *name = config_setting_name(member);
		if (!name) {
			config_setting_lookup_string(member, "name", &name);
		}
		if (!name) {
			name = config_setting_get_string(member);
			only_name = true;
		}
		if (!name) {
			continue;
		}

		if (config_setting_is_aggregate(member)) {
			value_setting = config_setting_get_member(member, "value");
		}

		if (!only_name) {
			value = resolve_constant_or(constants, value_setting, value);
		}
		prev_value = value;

		put_enum_value(constants, enum_name, name, value);
	}
}

static void
load_enums_section(const config_setting_t *config_section, ConstantRegistry *constants)
{
	if (!config_section) {
		return;
	}
	ssize_t length = config_setting_length(config_section);
	if (length <= 0) {
		return;
	}
	for (ssize_t i = 0; i < length; ++i) {
		config_setting_t *enum_def = config_setting_get_elem(config_section, i);
		if (!enum_def) {
			continue;
		}
		const char *name = config_setting_name(enum_def);
		// DEBUG_PRINT_VALUE(name, "%s");
		if (!name) {
			continue;
		}
		load_single_enum(name, enum_def, constants);
	}
}

static EventPredicateType
parse_event_predicate_type(const char *name)
{
	if (!name) {
		return EVPRED_INVALID;
	}
	if (strcmp(name, "accept") == 0) {
		return EVPRED_ACCEPT;
	}
	if (strcmp(name, "code_ns") == 0) {
		return EVPRED_CODE_NS;
	}
	if (strcmp(name, "code_major") == 0) {
		return EVPRED_CODE_MAJOR;
	}
	if (strcmp(name, "code_minor") == 0) {
		return EVPRED_CODE_MINOR;
	}
	if (strcmp(name, "payload") == 0) {
		return EVPRED_PAYLOAD;
	}
	if (strcmp(name, "input_index") == 0) {
		return EVPRED_INPUT_INDEX;
	}
	if (strcmp(name, "conjunction") == 0 || strcmp(name, "and") == 0) {
		return EVPRED_CONJUNCTION;
	}
	if (strcmp(name, "disjunction") == 0 || strcmp(name, "or") == 0) {
		return EVPRED_DISJUNCTION;
	}
	if (strcmp(name, "modifier") == 0) {
		return EVPRED_MODIFIER;
	}
	return EVPRED_INVALID;
}

static EventPredicateHandle
load_single_predicate(const config_setting_t * setting, EventPredicateHandleRegistry * registry, const ConstantRegistry * constants)
{
	const char *name = NULL;
	const char *type_name = NULL;
	config_setting_lookup_string(setting, "name", &name);
	config_setting_lookup_string(setting, "type", &type_name);
	if (!type_name) {
		return -1;
	}

	EventPredicate predicate;
	EventPredicateType type = parse_event_predicate_type(type_name);
	predicate.type = type;

	switch (type) {
	case EVPRED_INVALID:
		return -1;
	case EVPRED_ACCEPT:
		break;
	case EVPRED_CODE_NS...EVPRED_INPUT_INDEX:
		{
			int64_t min_value = INT64_MIN;
			int64_t max_value = INT64_MAX;
			min_value = resolve_constant_or(constants, config_setting_get_member(setting, "min"), min_value);
			max_value = resolve_constant_or(constants, config_setting_get_member(setting, "max"), max_value);
			predicate.range_data.min_value = min_value;
			predicate.range_data.max_value = max_value;
		}
		break;
	case EVPRED_CONJUNCTION:
	case EVPRED_DISJUNCTION:
		{
			config_setting_t *args = config_setting_get_member(setting, "args");
			EventPredicateHandle *handles = NULL;
			ssize_t length = config_setting_length(args);
			if (length > 0) {
				handles = T_ALLOC(length, EventPredicateHandle);
				for (ssize_t i = 0; i < length; ++i) {
					handles[i] = resolve_event_predicate(registry, constants, config_setting_get_elem(args, i));
				}
			}
			predicate.aggregate_data.length = length;
			predicate.aggregate_data.handles = handles;
		}
		break;
	case EVPRED_MODIFIER:
		{
			long long modifier = resolve_constant_or(constants, config_setting_get_member(setting, "modifier"), -1);
			if (modifier < 0 || modifier > MODIFIER_MAX) {
				return -1;
			}
			predicate.single_modifier = modifier;
		}
		break;
	default:
		return -1;
	}

	long long enabled = resolve_constant_or(constants, config_setting_get_member(setting, "enabled"), true);
	long long inverted = resolve_constant_or(constants, config_setting_get_member(setting, "inverted"), false);
	predicate.enabled = enabled != 0;
	predicate.inverted = inverted != 0;

	EventPredicateHandle handle = event_predicate_register(predicate);
	if (handle < 0) {
		if ((predicate.type == EVPRED_CONJUNCTION || predicate.type == EVPRED_DISJUNCTION) && predicate.aggregate_data.handles) {
			free(predicate.aggregate_data.handles);
			predicate.aggregate_data.handles = NULL;
			predicate.aggregate_data.length = 0;
		}
	}

	return handle;
}

static void
load_predicates_section(const config_setting_t *config_section, EventPredicateHandleRegistry *predicates, const ConstantRegistry *constants)
{
	if (!config_section) {
		return;
	}
	ssize_t length = config_setting_length(config_section);
	if (length <= 0) {
		return;
	}
	for (ssize_t i = 0; i < length; ++i) {
		config_setting_t *predicate_def = config_setting_get_elem(config_section, i);
		if (!predicate_def) {
			continue;
		}
		const char *name = config_setting_name(predicate_def);
		EventPredicateHandle handle = resolve_event_predicate(predicates, constants, predicate_def);
		if (handle >= 0 && name != NULL) {
			hash_table_insert(predicates, hash_table_key_from_cstr(name), &handle);
		}
	}
}

bool
load_config(const config_setting_t *config_root, FullConfig *config)
{
	if (!config_root || !config) {
		return false;
	}
	const config_setting_t *node_config = config_setting_get_member(config_root, "nodes");
	const config_setting_t *channel_config = config_setting_get_member(config_root, "channels");
	const config_setting_t *constants_config = config_setting_get_member(config_root, "constants");
	const config_setting_t *enums_config = config_setting_get_member(config_root, "enums");
	const config_setting_t *predicates_config = config_setting_get_member(config_root, "predicates");
	hash_table_init(&config->constants, NULL);
	hash_table_insert(&config->constants, hash_table_key_from_cstr("false"), (long long[1]) {0});
	hash_table_insert(&config->constants, hash_table_key_from_cstr("true"), (long long[1]) {1});
	populate_event_codes(&config->constants);
	load_constants_section(constants_config, &config->constants);
	load_enums_section(enums_config, &config->constants);
	hash_table_init(&config->predicates, NULL);
	load_predicates_section(predicates_config, &config->predicates, &config->constants);
	config->nodes = load_nodes_section(node_config);
	config->channels = load_channels_section(channel_config, &config->constants);
	return true;
}

void
reset_config(FullConfig *config)
{
	if (!config) {
		return;
	}
	if (config->nodes.items) {
		free(config->nodes.items);
		config->nodes.items = NULL;
		config->nodes.length = 0;
	}
	if (config->channels.items) {
		free(config->channels.items);
		config->channels.items = NULL;
		config->channels.length = 0;
	}
	hash_table_deinit(&config->constants);
	hash_table_deinit(&config->predicates);
}

long long
resolve_constant_or(const ConstantRegistry * registry, const config_setting_t * setting, long long dflt)
{
	if (!setting) {
		return dflt;
	}
	if (config_setting_type(setting) == CONFIG_TYPE_STRING) {
		if (!registry) {
			return dflt;
		}
		const char *name = config_setting_get_string(setting);
		HashTableIndex idx = hash_table_find(registry, hash_table_key_from_cstr(name));
		if (idx < 0) {
			return dflt;
		}
		return registry->value_array[idx];
	}
	if (config_setting_is_number(setting)) {
		return config_setting_get_int64(setting);
	}
	return dflt;
}

EventPredicateHandle
resolve_event_predicate(EventPredicateHandleRegistry * registry, const ConstantRegistry * constants, const config_setting_t * setting)
{
	if (!setting) {
		return -1;
	}
	if (config_setting_type(setting) == CONFIG_TYPE_STRING) {
		if (!registry) {
			return -1;
		}
		const char *name = config_setting_get_string(setting);
		HashTableIndex idx = hash_table_find(registry, hash_table_key_from_cstr(name));
		if (idx < 0) {
			return -1;
		}
		return registry->value_array[idx];
	}
	return load_single_predicate(setting, registry, constants);
}

long long
env_resolve_constant_or(InitializationEnvironment * env, const config_setting_t * setting, long long dflt)
{
	return resolve_constant_or(&env->constants, setting, dflt);
}

EventPredicateHandle
env_resolve_event_predicate(InitializationEnvironment * env, const config_setting_t * setting)
{
	return resolve_event_predicate(&env->predicates, &env->constants, setting);
}
