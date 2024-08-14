#include "processing.h"
#include "nodes/print.h"
#include "nodes/getchar.h"
#include "nodes/evdev.h"

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

	GraphNode * input_node = graph_node_new(&nodespec_getchar);
	GraphNode * output_node = graph_node_new(&nodespec_print);
	GraphNode * evdev_node = graph_node_new(&nodespec_evdev);
	GraphChannel chan[2];
	graph_channel_init(&chan[0], input_node, 0, output_node, 0);
	graph_channel_init(&chan[1], evdev_node, 0, output_node, 1);
	graph_node_register_io(input_node, &state);
	graph_node_register_io(output_node, &state);
	graph_node_register_io(evdev_node, &state);

	while (true) {
		process_iteration(&state);
	}

	graph_node_delete(evdev_node);
	graph_node_delete(output_node);
	graph_node_delete(input_node);

	io_subscription_list_deinit(&state.wait_output);
	io_subscription_list_deinit(&state.wait_input);
	return 0;
}
