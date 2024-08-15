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
		config_setting_lookup_string(config_member, "namd", &result.name);
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
load_channel_end_config(const config_setting_t *config_member, const char **name_ptr, size_t *index_ptr)
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
		*name_ptr = config_setting_name(config_member);
		*index_ptr = config_setting_get_int64(config_member);
		return;
	}
	*name_ptr = config_setting_get_string_elem(config_member, 0);
	*index_ptr = config_setting_get_int64_elem(config_member, 1);
}

static GraphChannelConfig
load_single_channel_config(const config_setting_t *config_member)
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
	load_channel_end_config(ends[0], &result.from.name, &result.from.index);
	load_channel_end_config(ends[1], &result.to.name, &result.to.index);
	return result;
}

static GraphChannelConfigSection
load_channels_section(const config_setting_t *config_section)
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
		result.items[i] = load_single_channel_config(config_setting_get_elem(config_section, i));
	}
	result.length = length;
	return result;
}

bool
load_config(const config_setting_t *config_root, FullConfig *config)
{
	if (!config_root || !config) {
		return false;
	}
	const config_setting_t *node_config = config_setting_get_member(config_root, "nodes");
	const config_setting_t *channel_config = config_setting_get_member(config_root, "channels");
	config->nodes = load_nodes_section(node_config);
	config->channels = load_channels_section(channel_config);
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
}
