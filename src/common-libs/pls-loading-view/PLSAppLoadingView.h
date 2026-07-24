#ifndef PLSAPPLOADINGVIEW_H
#define PLSAPPLOADINGVIEW_H

#include <QTimer>
#include "PLSDialogView.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class PLSAppLoadingView;
}
QT_END_NAMESPACE

class PLSAppLoadingView : public PLSDialogView {
	Q_OBJECT

public:
	static PLSAppLoadingView *instance()
	{
		static PLSAppLoadingView view;
		return &view;
	}
	void setPosition(const QString &parentGeometry);

private:
	PLSAppLoadingView(QWidget *parent = nullptr);
	~PLSAppLoadingView();
	void pollingTextContent();

private:
	Ui::PLSAppLoadingView *ui;
	QTimer m_timer;
	int m_currentIndex = 0;
};
#endif // PLSAPPLOADINGVIEW_H
