#ifndef MODULE_REGISTRY_H_
#define MODULE_REGISTRY_H_

#include "graph.h"

void register_graph_node_specification(GraphNodeSpecification * spec);
GraphNodeSpecification * lookup_graph_node_specification(const char * name);
void destroy_graph_node_specification_registry();

#endif /* end of include guard: MODULE_REGISTRY_H_ */
