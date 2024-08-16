#include <string.h>
#include "config.h"

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

bool
load_config(const config_setting_t *config_root, FullConfig *config)
{
	if (!config_root || !config) {
		return false;
	}
	const config_setting_t *node_config = config_setting_get_member(config_root, "nodes");
	const config_setting_t *channel_config = config_setting_get_member(config_root, "channels");
	const config_setting_t *constants_config = config_setting_get_member(config_root, "constants");
	hash_table_init(&config->constants, NULL);
	load_constants_section(constants_config, &config->constants);
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
}

long long
resolve_constant(const ConstantRegistry * registry, const config_setting_t * setting)
{
	if (!setting) {
		return 0;
	}
	if (config_setting_type(setting) == CONFIG_TYPE_STRING) {
		if (!registry) {
			return 1;
		}
		const char *name = config_setting_get_string(setting);
		HashTableIndex idx = hash_table_find(registry, hash_table_key_from_cstr(name));
		if (idx < 0) {
			return 0;
		}
		return registry->value_array[idx];
	}
	return config_setting_get_int64(setting);
}
