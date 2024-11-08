#ifndef CONFIG_H_
#define CONFIG_H_

#include <libconfig.h>
#include "defs.h"
#include "hash_table.h"
#include "event_predicate.h"

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
typedef TYPED_HASH_TABLE(EventPredicateHandle) EventPredicateHandleRegistry;
typedef struct initialization_environment InitializationEnvironment;

typedef struct {
	GraphNodeConfigSection nodes;
	GraphChannelConfigSection channels;
	union {
		struct {
			ConstantRegistry constants;
			EventPredicateHandleRegistry predicates;
		};
		struct initialization_environment {
			const ConstantRegistry constants;
			EventPredicateHandleRegistry predicates;
		} environment;
	};
} FullConfig;

bool load_config(const config_setting_t *config_root, FullConfig *config);
void reset_config(FullConfig *config);
long long resolve_constant_or(const ConstantRegistry * registry, const config_setting_t * setting, long long dflt);
EventPredicateHandle resolve_event_predicate(EventPredicateHandleRegistry * registry, const ConstantRegistry * constants, const config_setting_t * setting);

// These are not inline to make non-breaking ABI changes
long long env_resolve_constant_or(InitializationEnvironment * env, const config_setting_t * setting, long long dflt);
EventPredicateHandle env_resolve_event_predicate(InitializationEnvironment * env, const config_setting_t * setting);

__attribute__((unused)) inline static long long
resolve_constant(const ConstantRegistry * registry, const config_setting_t * setting)
{
	return resolve_constant_or(registry, setting, 0);
}

__attribute__((unused)) inline static long long
env_resolve_constant(InitializationEnvironment * env, const config_setting_t * setting)
{
	return env_resolve_constant_or(env, setting, 0);
}

#endif /* end of include guard: CONFIG_H_ */
