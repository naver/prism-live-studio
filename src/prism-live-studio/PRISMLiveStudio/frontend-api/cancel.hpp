#pragma once

#include <QObject>

#include "frontend-api-global.h"

class FRONTEND_API PLSCancel : public QObject {
	Q_OBJECT

public:
	using QObject::QObject;

	explicit operator bool() const { return m_cancel; }
	PLSCancel &operator=(bool cancel)
	{
		if (m_cancel != cancel) {
			m_cancel = cancel;
			emit cancelSignal(cancel);
		}
		return *this;
	}

signals:
	void cancelSignal(bool);

private:
	bool m_cancel = false;
};
