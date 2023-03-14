/******************************************************************************
    Copyright (C) 2013 by Hugh Bailey <obs.jim@gmail.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#pragma once

#include <util/c99defs.h>

#include <string>
#include <vector>

class QWidget;

/* Gets the path of PRISMLiveStudio specific data files (such as locale) */
bool GetDataFilePath(const char *data, std::string &path);

/* Updates the working directory for OSX application bundles */
bool InitApplicationBundle();

std::string GetDefaultVideoSavePath();

std::vector<std::string> GetPreferredLocales();

bool IsAlwaysOnTop(QWidget *window, const char *name);
void SetAlwaysOnTop(QWidget *window, const char *name, bool enable);
void bringWindowToTop(QWidget *window);

#ifdef _WIN32
uint32_t GetWindowsVersion(void);
uint32_t GetWindowsBuildVersion();
void SetAeroEnabled(bool enable);
void SetProcessPriority(const char *priority);
void SetWin32DropStyle(QWidget *window);
bool DisableAudioDucking(bool disable);
std::string GetFileName(std::string full_path);
std::string GetExtension(std::string full_path);
#endif

#ifdef __APPLE__
void EnableOSXVSync(bool enable);
void EnableOSXDockIcon(bool enable);
#endif

//fake comment for clang format error
