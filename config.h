#ifndef CONFIG_H_
#define CONFIG_H_

#include <libconfig.h>
#include "defs.h"

typedef struct {
	const char *name;
	const char *type;
	config_setting_t *options;
} GraphNodeConfig;

typedef struct {
	size_t length;
	GraphNodeConfig *items;
} GraphNodeConfigSection;

typedef struct {
	struct {
		const char *name;
		size_t index;
	} from, to;
} GraphChannelConfig;

typedef struct {
	size_t length;
	GraphChannelConfig *items;
} GraphChannelConfigSection;

typedef struct {
	GraphNodeConfigSection nodes;
	GraphChannelConfigSection channels;
} FullConfig;

bool load_config(const config_setting_t *config_root, FullConfig *config);
void reset_config(FullConfig *config);

#endif /* end of include guard: CONFIG_H_ */
