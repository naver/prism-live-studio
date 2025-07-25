/**
 * Copyright Off World Live Ltd (https://offworld.live), 2019-2021
 *
 * and licenced under the GPL v2 (https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html)
 *
 * Many thanks to authors of https://github.com/baffler/OBS-OpenVR-Input-Plugin which
 * was used as guidance to working with the OBS Studio APIs
 */

//PRISM/FanZirong/20241203/PRISM_PC-1675/add log fields
#include <pls/pls-base.h>

#ifndef WINSPOUT_H
#define WINSPOUT_H

//PRISM/FanZirong/20241203/PRISM_PC-1675/add log fields
#define blogex(kr, log_level, fields, field_count, message, ...) blogex(kr, log_level, fields, field_count, "[win_spout] " message, ##__VA_ARGS__)
//PRISM/FanZirong/20241203/PRISM_PC-1675/add log fields

#define blog(log_level, message, ...) blog(log_level, "[win_spout] " message, ##__VA_ARGS__)

void spout_output_start(const char *SpoutName);
void spout_output_stop();

#endif // WINSPOUT_H
