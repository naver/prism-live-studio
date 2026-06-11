#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <qlabel.h>

#include <frontend-api.h>
#include <PLSDialogView.h>

#include "PLSRemoteControlHistoryView.h"

QT_FORWARD_DECLARE_CLASS(QVBoxLayout);
QT_FORWARD_DECLARE_CLASS(QHBoxLayout);

QT_BEGIN_NAMESPACE
namespace Ui {
class PLSRemoteControlConfigView;
}
QT_END_NAMESPACE

class PLSRemoteControlConfigView : public PLSDialogView {
	Q_OBJECT

public:
	explicit PLSRemoteControlConfigView(QWidget *parent = nullptr);
	virtual ~PLSRemoteControlConfigView() noexcept(true);

	void load() const;

protected:
	void resizeEvent(QResizeEvent *event) override;
	void showEvent(QShowEvent *event) override;
	void hideEvent(QHideEvent *event) override;
	void closeEvent(QCloseEvent *event) override;

private:
	static void pls_frontend_event_received(pls_frontend_event event, const QVariantList &, void *context);

	Ui::PLSRemoteControlConfigView *ui;
	QVBoxLayout *m_verticalLayout;
	QHBoxLayout *m_HLayout;
	PLSRemoteControlHistoryView *m_historyView;
	bool _registered{false};
	bool _registeredNetworkState{false};
};
#endif // MAINWINDOW_H
