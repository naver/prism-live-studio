/******************************************************************************
    Copyright (C) 2014 by Hugh Bailey <obs.jim@gmail.com>

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

#include <memory>
#include <functional>

#include <obs.hpp>

#include "dialog-view.hpp"
#include "properties-view.hpp"

class PLSBasic;

#include "ui_PLSBasicInteraction.h"

class PLSEventFilter;

class PLSBasicInteraction : public PLSDialogView {
	Q_OBJECT

private:
	PLSBasic *main;

	std::unique_ptr<Ui::PLSBasicInteraction> ui;
	OBSSource source;
	OBSSignal removedSignal;
	OBSSignal renamedSignal;
	std::unique_ptr<PLSEventFilter> eventFilter;

	static void SourceRemoved(void *data, calldata_t *params);
	static void SourceRenamed(void *data, calldata_t *params);
	static void DrawPreview(void *data, uint32_t cx, uint32_t cy);

	bool GetSourceRelativeXY(int mouseX, int mouseY, int &x, int &y);

	bool HandleMouseClickEvent(QMouseEvent *event);
	bool HandleMouseMoveEvent(QMouseEvent *event);
	bool HandleMouseWheelEvent(QWheelEvent *event);
	bool HandleFocusEvent(QFocusEvent *event);
	bool HandleKeyEvent(QKeyEvent *event);

	PLSEventFilter *BuildEventFilter();

public:
	explicit PLSBasicInteraction(QWidget *parent, OBSSource source_);
	~PLSBasicInteraction();

	void Init();

protected:
	virtual void closeEvent(QCloseEvent *event) override;
};

typedef std::function<bool(QObject *, QEvent *)> EventFilterFunc;

class PLSEventFilter : public QObject {
	Q_OBJECT
public:
	explicit PLSEventFilter(EventFilterFunc filter_) : filter(filter_) {}

protected:
	bool eventFilter(QObject *obj, QEvent *event) { return filter(obj, event); }

private:
	EventFilterFunc filter;
};
