#ifndef PLSLIVINGMSGVIEW_H
#define PLSLIVINGMSGVIEW_H

#include <QWidget>
#include <qstackedwidget.h>
#include <QTimer>
#include "PLSSideBarDialogView.h"

class PLSLivingMsgItem;

namespace Ui {
class PLSLivingMsgView;
}
enum class LivingMsgType { Error = 1, Waring = 2 };
class PLSLivingMsgView : public PLSSideBarDialogView {
	Q_OBJECT

public:
	explicit PLSLivingMsgView(DialogInfo info, QWidget *parent = nullptr);
	~PLSLivingMsgView() override;
	void initializeView();

	void addMsgItem(const QString &msgInfo, const long long time, pls_toast_info_type type);
	QString getInfoWithUrl(const QString &str, const QString &url, const QString &replaceStr) const;
	void clearMsgView();
	void setShow(bool isVisable);
	int alertMessageCount() const { return static_cast<int>(m_msgItems.count()); }

protected:
	void showEvent(QShowEvent *event) override;
	void hideEvent(QHideEvent *event) override;
	void closeEvent(QCloseEvent *event) override;

signals:
	void hideSignal();

private:
	Ui::PLSLivingMsgView *ui;
	qint64 m_currentTime = 0;
	QList<PLSLivingMsgItem *> m_msgItems;
	QTimer m_t;
};

#endif // PLSLIVINGMSGVIEW_H
