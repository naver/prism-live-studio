#pragma once

// PRISM_PC-5670: Shared helpers for the five configurable built-in transitions marked as Prism defaults
// (private setting prism_builtin_default). Used by InitDefaultTransitions and scene JSON merge.

#include <cstring>

#include "obs.h"
#include "obs.hpp"

namespace pls {

inline constexpr const char *kPrismBuiltinDefaultTransitionKey = "prism_builtin_default";

inline bool IsPrismFiveMarkedBuiltinTransitionId(const char *id)
{
	return id && (strcmp(id, "swipe_transition") == 0 || strcmp(id, "slide_transition") == 0 ||
		      strcmp(id, "obs_stinger_transition") == 0 || strcmp(id, "fade_to_color_transition") == 0 ||
		      strcmp(id, "wipe_transition") == 0);
}

inline void MarkPrismBuiltinDefaultTransition(obs_source_t *source)
{
	if (!source)
		return;
	OBSDataAutoRelease priv = obs_source_get_private_settings(source);
	obs_data_set_bool(priv, kPrismBuiltinDefaultTransitionKey, true);
}

inline bool IsPrismBuiltinDefaultMarkedInPrivateSettings(obs_source_t *source)
{
	if (!source)
		return false;
	OBSDataAutoRelease priv = obs_source_get_private_settings(source);
	return obs_data_get_bool(priv, kPrismBuiltinDefaultTransitionKey);
}

} // namespace pls
