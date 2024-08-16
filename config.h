#ifndef CONFIG_H_
#define CONFIG_H_

#include <libconfig.h>
#include "defs.h"
#include "hash_table.h"

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

typedef TYPED_HASH_TABLE(long long) ConstantRegistry;

typedef struct {
	GraphNodeConfigSection nodes;
	GraphChannelConfigSection channels;
	ConstantRegistry constants;
} FullConfig;

bool load_config(const config_setting_t *config_root, FullConfig *config);
void reset_config(FullConfig *config);
long long resolve_constant(const ConstantRegistry * registry, const config_setting_t * setting);

#endif /* end of include guard: CONFIG_H_ */