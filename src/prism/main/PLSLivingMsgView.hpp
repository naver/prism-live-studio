#ifndef PLSLIVINGMSGVIEW_H
#define PLSLIVINGMSGVIEW_H

#include <QWidget>
#include <dialog-view.hpp>
#include <qstackedwidget.h>
#include <QTimer>
#include "frontend-api.h"

class PLSLivingMsgItem;

namespace Ui {
class PLSLivingMsgView;
}
enum class LivingMsgType { Error = 1, Waring = 2 };

class PLSLivingMsgView : public PLSDialogView {
	Q_OBJECT

public:
	explicit PLSLivingMsgView(DialogInfo info, QWidget *parent = nullptr, PLSDpiHelper dpiHelper = PLSDpiHelper());
	~PLSLivingMsgView();
	void initializeView();
	void setGeometryOfNormal(const QRect &geometry);
	void addMsgItem(const QString &msgInfo, const long long time, pls_toast_info_type type);
	QString getInfoWithUrl(const QString &str, const QString &url, const QString &replaceStr);
	void clearMsgView();
	void setShow(bool isVisable);

protected:
	virtual void showEvent(QShowEvent *event);
	virtual void hideEvent(QHideEvent *event);
	virtual void closeEvent(QCloseEvent *event);
	virtual void resizeEvent(QResizeEvent *event);

signals:
	void hideSignal();

private:
	Ui::PLSLivingMsgView *ui;
	qint64 m_currentTime;
	QList<PLSLivingMsgItem *> m_msgItems;
	QTimer m_t;
};

#endif // PLSLIVINGMSGVIEW_H
