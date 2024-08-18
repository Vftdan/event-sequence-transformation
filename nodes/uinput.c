#include <libevdev/libevdev-uinput.h>
#include <stdio.h>
#include <limits.h>
#include <errno.h>
#include "../graph.h"
#include "../module_registry.h"

typedef struct {
	GraphNode as_GraphNode;
	IOHandling subscription;
	struct libevdev *dev;
	struct libevdev_uinput *uidev;
} UinputGraphNode;

typedef struct {
	enum {
		CODESPECDATA_NONE,
		CODESPECDATA_INT,
		CODESPECDATA_ABSINFO,
	} which;
	union {
		struct input_absinfo as_absinfo;
		int as_int;
	};
} CodeSpecData;

inline static CodeSpecData
load_code_spec_data(const config_setting_t * setting, InitializationEnvironment * env)
{
	CodeSpecData data = {.which = CODESPECDATA_NONE, .as_absinfo = {0, 0, 0, 0, 0, 0}};
	if (!setting) {
		return data;
	}
	if (config_setting_type(setting) == CONFIG_TYPE_GROUP) {
		data.which = CODESPECDATA_ABSINFO;
		data.as_absinfo = (struct input_absinfo) {
			.value = (int) env_resolve_constant_or(env, config_setting_get_member(setting, "value"), 0),
			.minimum = (int) env_resolve_constant_or(env, config_setting_get_member(setting, "minimum"), INT_MIN),
			.maximum = (int) env_resolve_constant_or(env, config_setting_get_member(setting, "maximum"), INT_MAX),
			.fuzz = (int) env_resolve_constant_or(env, config_setting_get_member(setting, "fuzz"), 0),
			.flat = (int) env_resolve_constant_or(env, config_setting_get_member(setting, "flat"), 0),
			.resolution = (int) env_resolve_constant_or(env, config_setting_get_member(setting, "resolution"), 1),
		};
	} else if (config_setting_is_scalar(setting)) {
		data.which = CODESPECDATA_INT;
		data.as_int = (int) env_resolve_constant(env, setting);
	}
	return data;
}

static void
load_enable_code(struct libevdev * dev, const config_setting_t * setting, InitializationEnvironment * env)
{
	if (!setting) {
		return;
	}
	int type = (int) env_resolve_constant(env, config_setting_get_member(setting, "major"));
	int code = (int) env_resolve_constant(env, config_setting_get_member(setting, "minor"));
	CodeSpecData data = load_code_spec_data(config_setting_get_member(setting, "data"), env);
	const void *data_ptr = NULL;
	if (data.which == CODESPECDATA_INT) {
		data_ptr = &data.as_int;
	} else if (data.which == CODESPECDATA_ABSINFO) {
		data_ptr = &data.as_absinfo;
	}
	libevdev_enable_event_code(dev, type, code, data_ptr);
}

static bool
handle_event(EventPositionBase * self, EventNode * event)
{
	UinputGraphNode *node = DOWNCAST(UinputGraphNode, GraphNode, DOWNCAST(GraphNode, EventPositionBase, self));
	libevdev_uinput_write_event(node->uidev, (unsigned int) event->data.code.major, (unsigned int) event->data.code.minor, (int) event->data.payload);
	event_destroy(event);
	return true;
}

static GraphNode *
create(GraphNodeSpecification * spec, GraphNodeConfig * config, InitializationEnvironment * env)
{
	UinputGraphNode * node = T_ALLOC(1, UinputGraphNode);
	if (!node) {
		return NULL;
	}

	const char *name;
	config_setting_lookup_string(config->options, "name", &name);
	if (!name) {
		free(node);
		return NULL;
	}

	const config_setting_t *enabled_codes_setting = config_setting_get_member(config->options, "enabled_codes");
	if (!enabled_codes_setting) {
		free(node);
		return NULL;
	}

	struct libevdev *dev = libevdev_new();
	if (!dev) {
		free(node);
		return NULL;
	}

	libevdev_set_name(dev, name);
	size_t codes_length = config_setting_length(enabled_codes_setting);
	for (size_t i = 0; i < codes_length; ++i) {
		load_enable_code(dev, config_setting_get_elem(enabled_codes_setting, i), env);
	}

	struct libevdev_uinput *uidev;
	int err;
	if ((err = libevdev_uinput_create_from_device(dev, LIBEVDEV_UINPUT_OPEN_MANAGED, &uidev) != 0)) {
		libevdev_free(dev);
		free(node);
		if (err < 0) {
			errno = -err;
		}
		return NULL;
	}

	*node = (UinputGraphNode) {
		.as_GraphNode = {
			.as_EventPositionBase = {
				.handle_event = &handle_event,
				.waiting_new_event = false,
			},
			.specification = spec,
			.inputs = EMPTY_GRAPH_CHANNEL_LIST,
			.outputs = EMPTY_GRAPH_CHANNEL_LIST,
		},
		.dev = dev,
		.uidev = uidev,
	};
	return &node->as_GraphNode;
}

static void destroy
(GraphNodeSpecification * self, GraphNode * target)
{
	(void) self;
	UinputGraphNode * node = DOWNCAST(UinputGraphNode, GraphNode, target);
	if (node->uidev) {
		libevdev_uinput_destroy(node->uidev);
		node->uidev = NULL;
	}
	if (node->dev) {
		libevdev_free(node->dev);
		node->dev = NULL;
	}
	free(target);
}

GraphNodeSpecification nodespec_uinput = (GraphNodeSpecification) {
	.create = &create,
	.destroy = &destroy,
	.register_io = NULL,
	.name = "uinput",
	.documentation = "Writes received events to a new uinput device\nAccepts events on any connector\nDoes not send events"
	                 "\nOption 'name' (required): device name provided to uinput"
	                 "\nOption 'enabled_codes' (required): collection of event code specifications:"
	                 "\n\tField 'major' (required): evdev event type"
	                 "\n\tField 'minor' (required): evdev event code"
	                 "\n\tField 'data' (optional): integer (useful with major = \"even_type.REP\") or absinfo (useful with major = \"even_type.ABS\"):"
	                 "\n\t\tField 'value' (optional): initial axis value"
	                 "\n\t\tField 'minimum' (optional): minimum axis value"
	                 "\n\t\tField 'maximum' (optional): maximum axis value"
	                 "\n\t\tField 'fuzz' (optional): axis noise gate"
	                 "\n\t\tField 'flat' (optional): axis dead zone"
	                 "\n\t\tField 'resolution' (optional): axis resolution"
	,
};

MODULE_CONSTRUCTOR(init)
{
	register_graph_node_specification(&nodespec_uinput);
}
