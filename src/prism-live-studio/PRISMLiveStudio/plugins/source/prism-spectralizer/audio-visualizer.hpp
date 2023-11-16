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

#pragma once

#include <graphics/graphics.h>
#include <string>
#include <memory>
#include "audio-source.hpp"

struct visual_params;
struct config;

class audio_visualizer {
private:
	std::unique_ptr<audio_source> m_source = nullptr;
	config *m_cfg = nullptr;
	std::string m_source_id = "none"; /* where to read audio from */

public:
	explicit audio_visualizer(config *cfg);
	virtual ~audio_visualizer();

	audio_visualizer(audio_visualizer const &) = delete;
	audio_visualizer &operator=(audio_visualizer const &) = delete;

	audio_visualizer(audio_visualizer &&) = delete;
	audio_visualizer &operator=(audio_visualizer &&) = delete;

	virtual void update();

	/* Active is set to true, if the current tick is in sync with the
     * user configured fps */
	virtual void tick(float seconds);

	virtual void render() = 0;

	virtual bool update_frame() = 0;

	virtual gs_texture *get_texture() = 0;
};
