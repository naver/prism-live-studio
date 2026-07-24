#ifndef PLSUpdateTipView_HPP
#define PLSUpdateTipView_HPP

#include <QWidget>
#include <QObject>
#include <QString>
#include <libbrowser.h>
#include <PLSDialogView.h>

QT_BEGIN_NAMESPACE
namespace Ui {
class PLSUpdateTipView;
}
QT_END_NAMESPACE

class PLSUpdateTipView : public PLSDialogView {
	Q_OBJECT

public:
	explicit PLSUpdateTipView(QWidget *parent = nullptr);
	~PLSUpdateTipView() override;
	void updateView(bool isForceUpdate, const QString &updateInfoUrl, const QString &version);
	bool needPopupUpdateView(const QString &version) const;

private slots:
	void on_nextUpdateBtn_clicked();
	void on_nowUpdateBtn_clicked();

private:
	Ui::PLSUpdateTipView *ui;
	bool m_isForceUpdate = false;
	QString m_version;
	QString m_fileUrl;
	QString m_updateInfoUrl;
	pls::browser::BrowserWidget *m_browserWidget = nullptr;
	QPoint m_mouseStartPoint;
	QPoint m_windowTopLeftPoint;
	bool m_pressed = false;
};
#endif // PLSUpdateTipView_HPP
