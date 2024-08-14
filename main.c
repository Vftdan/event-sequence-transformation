#include "processing.h"
#include "nodes/print.h"
#include "nodes/getchar.h"
#include "nodes/evdev.h"

GraphNodeSpecification *node_specifications[] = {
	&nodespec_getchar,
	&nodespec_print,
	&nodespec_evdev,
};

int
main(int argc, char ** argv)
{
	(void)argc;
	(void)argv;

	ProcessingState state = (ProcessingState) {
		.wait_delay = NULL,
		.reached_time = get_current_time(),
	};
	io_subscription_list_init(&state.wait_input, 5);
	io_subscription_list_init(&state.wait_output, 5);

	config_t config_tree;
	FullConfig loaded_config;
	config_init(&config_tree);
	config_read_file(&config_tree, "config.cfg");
	config_set_auto_convert(&config_tree, CONFIG_TRUE);
	if (!load_config(config_root_setting(&config_tree), &loaded_config)) {
		perror("Failed to load config");
		exit(1);
	}

	GraphNode **nodes = calloc(loaded_config.nodes.length, sizeof(GraphNode*));
	for (size_t i = 0; i < loaded_config.nodes.length; ++i) {
		const char* type_name = loaded_config.nodes.items[i].type;
		if (!type_name) {
			fprintf(stderr, "No node type for node %ld \"%s\"\n", i, loaded_config.nodes.items[i].name);
			exit(1);
		}
		GraphNodeSpecification *spec = NULL;
		for (size_t j = 0; j < lengthof(node_specifications); ++j) {
			if (strcmp(type_name, node_specifications[j]->name) == 0) {
				spec = node_specifications[j];
				break;
			}
		}
		if (!spec) {
			fprintf(stderr, "Unknown node type \"%s\" for node %ld \"%s\"\n", type_name, i, loaded_config.nodes.items[i].name);
			exit(1);
		}
		if (!(nodes[i] = graph_node_new(spec, &loaded_config.nodes.items[i]))) {
			perror("Failed to create node");
			fprintf(stderr, "%ld \"%s\"\n", i, loaded_config.nodes.items[i].name);
			exit(1);
		}
	}

	GraphChannel *channels = calloc(loaded_config.channels.length, sizeof(GraphChannel));
	for (size_t i = 0; i < loaded_config.channels.length; ++i) {
		const char *node_names[2];
		GraphNode *end_nodes[2] = {NULL, NULL};
		node_names[0] = loaded_config.channels.items[i].from.name;
		node_names[1] = loaded_config.channels.items[i].to.name;
		for (int j = 0; j < 2; ++j) {
			for (size_t k = 0; k < loaded_config.nodes.length; ++k) {
				if (strcmp(loaded_config.nodes.items[k].name, node_names[j]) == 0) {
					end_nodes[j] = nodes[k];
					break;
				}
			}
			if (!end_nodes[j]) {
				fprintf(stderr, "No node named \"%s\"\n", node_names[j]);
				exit(1);
			}
		}
		graph_channel_init(&channels[i],
			end_nodes[0], loaded_config.channels.items[i].from.index,
			end_nodes[1], loaded_config.channels.items[i].to.index
		);
	}

	for (size_t i = 0; i < loaded_config.nodes.length; ++i) {
		graph_node_register_io(nodes[i], &state);
	}

	while (true) {
		process_iteration(&state);
	}

	for (ssize_t i = loaded_config.nodes.length - 1; i >= 0; --i) {
		graph_node_delete(nodes[i]);
	}
	free(channels);
	free(nodes);

	config_destroy(&config_tree);

	io_subscription_list_deinit(&state.wait_output);
	io_subscription_list_deinit(&state.wait_input);
	return 0;
}
