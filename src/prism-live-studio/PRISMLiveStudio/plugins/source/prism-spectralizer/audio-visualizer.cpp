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

#include "audio-visualizer.hpp"
#include "prism-visualizer-source.hpp"
#include "audio-source.hpp"
#include "obs-internal-source.hpp"

audio_visualizer::audio_visualizer(config *cfg) : m_cfg(cfg) {}

audio_visualizer::~audio_visualizer()
{
	m_source.reset();
}

void audio_visualizer::update()
{
	if (m_source)
		m_source->update();
	if (!m_source || m_cfg->audio_source_name != m_source_id) {
		m_source_id = m_cfg->audio_source_name;
		if (m_source) {
			m_source.reset();
		}
		if (m_cfg->audio_source_name.empty() || m_cfg->audio_source_name == std::string(defaults::audio_source)) {
			m_source.reset();
		} else {
			m_source = std::make_unique<obs_internal_source>(m_cfg);
		}
	}
}

void audio_visualizer::tick(float seconds)
{
	if (m_source)
		m_cfg->read_data = m_source->tick(seconds);
	else
		m_cfg->read_data = false;
}
