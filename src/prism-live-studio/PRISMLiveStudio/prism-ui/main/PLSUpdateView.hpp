#ifndef PLSUPDATEVIEW_HPP
#define PLSUPDATEVIEW_HPP

#include "PLSDialogView.h"
#include <QObject>
#include <QString>
#include "libbrowser.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class PLSUpdateView;
}
QT_END_NAMESPACE

class PLSUpdateView : public PLSDialogView {
	Q_OBJECT

public:
	PLSUpdateView(bool manualUpdate, bool isForceUpdate, const QString &version, const QString &fileUrl, const QString &updateInfoUrl, QWidget *parent = nullptr);
	~PLSUpdateView() override;

	/** Cleared when opening the update dialog; read after it closes with takeUserChoseExitApp(). */
	static void resetUserChoseExitApp();
	/** Returns whether user chose exit on forced update in the last dialog, then clears the flag. */
	static bool takeUserChoseExitApp();
	/** If forced update and dialog did not accept (e.g. Alt+F4 / reject), treat like exit-app for defer/save logic. */
	static void noteForceUpdateDialogResult(bool is_force_update, int exec_result);

private:
	void initUI();
	void initConnect() const;

private slots:
	void on_nextUpdateBtn_clicked();
	void on_nowUpdateBtn_clicked();
	void isShowMainView(bool isShow);
	void updateBrowserUrl(const QString &url) const;

protected:
	void showEvent(QShowEvent *event) override;
	void closeEvent(QCloseEvent *event) override;

private:
	Ui::PLSUpdateView *ui;
	bool m_manualUpdate;
	bool m_isForceUpdate;
	QString m_version;
	QString m_fileUrl;
	QString m_updateInfoUrl;
	pls::browser::BrowserWidget *m_browserWidget{nullptr};
	QPoint m_mouseStartPoint;
	QPoint m_windowTopLeftPoint;
	bool m_pressed;
	static bool s_userChoseExitApp;
};
#endif // PLSUPDATEVIEW_HPP
