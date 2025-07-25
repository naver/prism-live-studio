/******************************************************************************
    Copyright (C) 2015 by Ruwen Hahn <palana@stunned.de>

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

#include <QLabel>
#include <obs.hpp>

class SourceLabel : public QLabel {
	Q_OBJECT

public:
	explicit SourceLabel(QWidget *p) : QLabel(p) {}
	explicit SourceLabel(const QString &text, QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());

	~SourceLabel() override = default;

	void setText(const QString &text); // overwrite the func of QLabel
	void setText(const char *text);    // overwrite the func of QLabel
	QString GetText() const;           // overwrite the func of QLabel
	void appendDeviceName(const char *name, const char *appendDeviceName);
	void setPadding(int value) { m_iPadding = value; }

protected:
	void resizeEvent(QResizeEvent *event) override;
	void paintEvent(QPaintEvent *event) override;

	QString SnapSourceName();

private:
	QString currentText = "";
	int m_iPadding = 5;
};

class OBSSourceLabel : public SourceLabel {
	Q_OBJECT

public:
	OBSSignal renamedSignal;
	OBSSignal removedSignal;
	OBSSignal destroyedSignal;

	OBSSourceLabel(const obs_source_t *source, QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags())
		: SourceLabel(nullptr, parent, f),
		  renamedSignal(obs_source_get_signal_handler(source), "rename", &OBSSourceLabel::SourceRenamed, this),
		  removedSignal(obs_source_get_signal_handler(source), "remove", &OBSSourceLabel::SourceRemoved, this),
		  destroyedSignal(obs_source_get_signal_handler(source), "destroy", &OBSSourceLabel::SourceDestroyed,
				  this)
	{
		setText(obs_source_get_name(source));
	}

	void clearSignals();

protected:
	static void SourceRenamed(void *data, calldata_t *params);
	static void SourceRemoved(void *data, calldata_t *params);
	static void SourceDestroyed(void *data, calldata_t *params);

signals:
	void Renamed(const char *name);
	void Removed();
	void Destroyed();
};
