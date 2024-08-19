#include <getopt.h>
#include <stdio.h>
#include "processing.h"
#include "hash_table.h"
#include "module_registry.h"

union __attribute__((transparent_union)) option_ident {
	enum {
		NCOPT_BASE = 0xFF,
		NCOPT_MODULE_HELP,
	} as_nonchar;
	int as_int;
	// No "as_char" field, because it can cause problems on big-endian CPUs
};

int
main(int argc, char ** argv)
{
	const char* config_filename = "config.cfg";

	while (true) {
		static const struct option long_options [] = {
			{"config",         required_argument, NULL, 'c'},
			{"help",           no_argument,       NULL, 'h'},
			{"list-modules",   no_argument,       NULL, 'l'},
			{"module-help",    required_argument, NULL, NCOPT_MODULE_HELP},
		{NULL, 0, NULL, 0}};
		static const char help_fstring[] =
			"Usage: %s <[options...]>\n"
			"Options:\n"
			"\t--config <filename>, -c <filename>  read configuration from <filename>\n"
			"\t                                    instead of ./config.cfg\n"
			"\t--help, -h                          show this message\n"
			"\t--list-modules, -l                  list currently loaded node types\n"
			"\t--module-help <name>                print help information provided for node type <name>\n"
		;

		union option_ident opt = {.as_int = getopt_long(argc, argv, "c:hl", long_options, NULL)};
		if (opt.as_int < 0) {
			break;
		}

		switch (opt.as_int) {
		case 'c':
			config_filename = optarg;
			break;
		case 'h':
			printf(help_fstring, argv[0]);
			return 0;
		case 'l':
			{
				const GraphNodeSpecificationRegistry * reg = get_graph_node_specification_registy();
				for (size_t i = 0; i < reg->capacity; ++i) {
					if (!reg->key_array[i].key.bytes) {
						continue;
					}
					printf("%.*s\n", (int) reg->key_array[i].key.length, reg->key_array[i].key.bytes);
				}
			};
			return 0;
		case NCOPT_MODULE_HELP:
			{
				GraphNodeSpecification *spec = lookup_graph_node_specification(optarg);
				if (!spec) {
					fprintf(stderr, "Unknown node type \"%s\"\n", optarg);
					return 1;
				}
				const char* module_help = spec->documentation;
				if (module_help) {
					printf("Help for node type \"%s\":\n", optarg);
					printf("%s\n", module_help);
				} else {
					printf("No help provided for node type \"%s\"\n", optarg);
				}
			};
			return 0;
		default:
			fprintf(stderr, "Unexpected option ");
			if ((unsigned int) opt.as_int <= 0xFF) {
				fprintf(stderr, "'%c'\n", (char) opt.as_int);
			} else {
				fprintf(stderr, "%d\n", opt.as_int);
			}
			return 1;
		}
	}
	if (optind < argc) {
		fprintf(stderr, "Unexpected positional argument at position %d: %s\n", optind, argv[optind]);
		return 1;
	}

	ProcessingState state = (ProcessingState) {
		.wait_delay = NULL,
		.reached_time = get_current_time(),
	};
	io_subscription_list_init(&state.wait_input, 5);
	io_subscription_list_init(&state.wait_output, 5);

	config_t config_tree;
	FullConfig loaded_config;
	config_init(&config_tree);
	if (config_read_file(&config_tree, config_filename) != CONFIG_TRUE) {
		fprintf(stderr, "Config syntax error: %s:%d: %s\n", config_error_file(&config_tree), config_error_line(&config_tree), config_error_text(&config_tree));
		exit(1);
	}
	config_set_auto_convert(&config_tree, CONFIG_TRUE);
	if (!load_config(config_root_setting(&config_tree), &loaded_config)) {
		perror("Failed to load config");
		exit(1);
	}

	GraphNode **nodes = T_ALLOC(loaded_config.nodes.length, GraphNode*);
	TYPED_HASH_TABLE(size_t) named_nodes;
	hash_table_init(&named_nodes, NULL);
	for (size_t i = 0; i < loaded_config.nodes.length; ++i) {
		const char* type_name = loaded_config.nodes.items[i].type;
		if (!type_name) {
			fprintf(stderr, "No node type for node %ld \"%s\"\n", i, loaded_config.nodes.items[i].name);
			exit(1);
		}
		GraphNodeSpecification *spec = lookup_graph_node_specification(type_name);
		if (!spec) {
			fprintf(stderr, "Unknown node type \"%s\" for node %ld \"%s\"\n", type_name, i, loaded_config.nodes.items[i].name);
			exit(1);
		}
		if (!(nodes[i] = graph_node_new(spec, &loaded_config.nodes.items[i], &loaded_config.environment))) {
			perror("Failed to create node");
			fprintf(stderr, "Node %ld \"%s\"\n", i, loaded_config.nodes.items[i].name);
			exit(1);
		}
		if (loaded_config.nodes.items[i].name) {
			hash_table_insert(&named_nodes, hash_table_key_from_cstr(loaded_config.nodes.items[i].name), &i);
		}
	}

	GraphChannel *channels = T_ALLOC(loaded_config.channels.length, GraphChannel);
	for (size_t i = 0; i < loaded_config.channels.length; ++i) {
		const char *node_names[2];
		GraphNode *end_nodes[2] = {NULL, NULL};
		node_names[0] = loaded_config.channels.items[i].from.name;
		node_names[1] = loaded_config.channels.items[i].to.name;
		for (int j = 0; j < 2; ++j) {
			HashTableIndex k = hash_table_find(&named_nodes, hash_table_key_from_cstr(node_names[j]));
			if (k < 0) {
				perror("Errno");
				fprintf(stderr, "No node named \"%s\"\n", node_names[j]);
				exit(1);
			}
			end_nodes[j] = nodes[named_nodes.value_array[k]];
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

	hash_table_deinit(&named_nodes);
	for (ssize_t i = loaded_config.nodes.length - 1; i >= 0; --i) {
		graph_node_delete(nodes[i]);
	}
	event_destroy_all();
	free(channels);
	free(nodes);

	reset_config(&loaded_config);
	event_predicate_reset();
	config_destroy(&config_tree);

	io_subscription_list_deinit(&state.wait_output);
	io_subscription_list_deinit(&state.wait_input);
	return 0;
}
