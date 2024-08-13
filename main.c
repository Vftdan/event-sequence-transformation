#include "processing.h"
#include "nodes/print.h"
#include "nodes/getchar.h"

int
main(int argc, char ** argv)
{
	(void)argc;
	(void)argv;

	ProcessingState state = (ProcessingState) {
		.wait_delay = NULL,
	};
	clock_gettime(CLOCK_MONOTONIC, &state.reached_time);
	io_subscription_list_init(&state.wait_input, 5);
	io_subscription_list_init(&state.wait_output, 5);

	GraphNode * input_node = graph_node_new(&nodespec_getchar);
	GraphNode * output_node = graph_node_new(&nodespec_print);
	GraphChannel chan;
	graph_channel_init(&chan, input_node, 0, output_node, 0);
	graph_node_register_io(input_node, &state);
	graph_node_register_io(output_node, &state);

	while (true) {
		process_iteration(&state);
	}

	graph_node_delete(output_node);
	graph_node_delete(input_node);

	io_subscription_list_deinit(&state.wait_output);
	io_subscription_list_deinit(&state.wait_input);
	return 0;
}
