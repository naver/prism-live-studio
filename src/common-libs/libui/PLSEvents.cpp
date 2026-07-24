#include "PLSEvents.h"

PLSEvents *PLSEvents::instance()
{
	static PLSEvents s_instance;
	return &s_instance;
}
