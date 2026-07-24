#ifndef PLSPROGRESSVIEW_H
#define PLSPROGRESSVIEW_H

#include <QWidget>
#include "../PLSCommonConst.h"
#include <qtimer.h>
#include "login-view.hpp"
#include <QMovie>
namespace Ui {
class PLSProgressView;
}

class PLSProgressView : public QWidget {
	Q_OBJECT

public:
	explicit PLSProgressView(QWidget *parent = nullptr);
	~PLSProgressView() override;
	void updateView(const QString &tipText, pls_window_type type);

public slots:
	void updateProgress(const int &currentValue, const int &maxValue);
	void updateProgressAndText(const QString &text, const int &currentValue, const int &maxValue);

protected:
	void hideEvent(QHideEvent *event) override;

private:
	void updateProgressView();
	void resetProgressValues();
	void finshHandle(pls_window_type winType) const;

	Ui::PLSProgressView *ui;
	int m_minValue = 0;
	int m_maxValue = 100;
	int m_currentValue = 0;
	pls_window_type m_winType = pls_window_type::PLS_CHECK_UPDATE_VIEW;
	QTimer m_timer;
	PLSLoginView m_loginView;
	QMovie m_logoMovie;
	bool m_isFirstShow = true;
};

#endif // PLSPROGRESSVIEW_H
