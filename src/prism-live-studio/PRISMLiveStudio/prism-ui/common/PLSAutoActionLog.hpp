#pragma once
#include <QMenu>
#include <string>
#include "liblog.h"

class CustomMenu : public QMenu {
	Q_OBJECT
public:
	explicit CustomMenu(const char *log_content, const QString &title, QWidget *parent = nullptr) : QMenu(title, parent), str(log_content ? log_content : "") {}

protected:
	void showEvent(QShowEvent *event) override
	{
		QMenu::showEvent(event);

		if (!shown) {
			shown = true;
			PLS_UI_ACTION("show %s", str.c_str());
		}
	}

private:
	bool shown = false;
	std::string str;
};