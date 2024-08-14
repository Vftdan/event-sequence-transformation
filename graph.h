#ifndef GRAPH_H_
#define GRAPH_H_

#include "events.h"
#include "processing.h"
#include "config.h"

typedef struct graph_node GraphNode;
typedef struct graph_channel GraphChannel;
typedef struct graph_node_specification GraphNodeSpecification;

typedef struct {
	size_t length;
	GraphChannel ** elements;
} GraphChannelList;

#define EMPTY_GRAPH_CHANNEL_LIST ((GraphChannelList) {.length = 0, .elements = NULL})

struct graph_node {
	EventPositionBase as_EventPositionBase;
	GraphNodeSpecification * specification;
	GraphChannelList inputs, outputs;
};

struct graph_channel {
	EventPositionBase as_EventPositionBase;
	GraphNode *start, *end;
	size_t idx_start, idx_end;
};

struct graph_node_specification {
	GraphNode * (*create)(GraphNodeSpecification * self, GraphNodeConfig * config);
	void (*destroy)(GraphNodeSpecification * self, GraphNode * target);
	void (*register_io)(GraphNodeSpecification * self, GraphNode * target, ProcessingState * state);
	char *name;
};

void graph_channel_init(GraphChannel * ch, GraphNode * start, size_t start_idx, GraphNode * end, size_t end_idx);
GraphNode *graph_node_new(GraphNodeSpecification * spec, GraphNodeConfig * config);
void graph_node_delete(GraphNode * self);
void graph_node_register_io(GraphNode * self, ProcessingState * state);
void graph_channel_list_init(GraphChannelList * lst);
void graph_channel_list_deinit(GraphChannelList * lst);

#endif /* end of include guard: GRAPH_H_ */
