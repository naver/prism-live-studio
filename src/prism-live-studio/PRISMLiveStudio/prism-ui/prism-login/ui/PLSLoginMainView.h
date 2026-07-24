#ifndef PLSLAUNCHERMAINVIEW_H
#define PLSLAUNCHERMAINVIEW_H

#include <QWidget>
#include <PLSDialogView.h>
#include <qstringlist.h>
#include "PLSCommonConst.h"
#include <QPointer>

QT_BEGIN_NAMESPACE
namespace Ui {
class PLSLoginMainView;
}
QT_END_NAMESPACE

using AlertClickMthod = void (*)();

class PLSMouseEnterEventFilter : public QObject {
	Q_OBJECT
	using QObject::QObject;

protected:
	bool eventFilter(QObject *obj, QEvent *event) override;
};

class PLSLoginMainView : public PLSDialogView {
	Q_OBJECT
public:
	static PLSLoginMainView *instance();
	static PLSLoginMainView *get() { return g_launcerMainView; };

	explicit PLSLoginMainView(QWidget *parent = nullptr);

	~PLSLoginMainView() override;
	QWidget *changeView(pls_window_type windowType);

	void startdownloadNewInstallPackage(const QString &fileUrl, const QString &gcc);
	/** @return false if user chose to exit the app from forced update tip; caller should abort startup. */
	bool updateTipHandler();
	void showNoticeView();
	void setAlertClickMethod(AlertClickMthod _method) { m_alertClickMthod = _method; }

	void prismProgressChanged(const QString &uiStr, int progress);
	void prismUpdateFailedHandler();
	static int getCloseResultValue() { return PLSLoginMainView::closeResultValue; }

protected:
	void closeEvent(QCloseEvent *event) override;

private:
	Ui::PLSLoginMainView *ui;
	pls_window_type m_currentType = pls_window_type::PLS_LOGIN_VIEW;
	static QPointer<PLSLoginMainView> g_launcerMainView;

	AlertClickMthod m_alertClickMthod{nullptr};
	int m_appRunProgress = 0;
	static int closeResultValue;
};
#endif // PLSLAUNCHERMAINVIEW_H
