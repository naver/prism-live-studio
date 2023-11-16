#pragma once

#include <PLSDock.h>

class OBSDock : public PLSDock {
	Q_OBJECT

public:
	inline OBSDock(QWidget *parent = nullptr) : PLSDock(parent) {}

	virtual void closeEvent(QCloseEvent *event);
};
