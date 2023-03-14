/*************************************************************************
 * This file is part of spectralizer
 * github.con/univrsal/spectralizer
 * Copyright 2020 univrsal <universailp@web.de>.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *************************************************************************/

#include <obs-module.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("prism-spectralizer", "en-US")

MODULE_EXPORT const char *obs_module_description(void)
{
	return "Spectrum visualizer";
}

extern void register_visualiser();
//extern void release_prism_monitor_data();

bool obs_module_load()
{
	register_visualiser();
	return true;
}

void obs_module_unload()
{
	/* NO-OP */
}

const char *obs_module_name(void)
{
	return obs_module_description();
}
