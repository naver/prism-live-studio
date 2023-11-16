#ifndef PLSOPENSOURCEVIEW_H
#define PLSOPENSOURCEVIEW_H

#include "PLSDialogView.h"
#include "libbrowser.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class PLSOpenSourceView;
}
QT_END_NAMESPACE

class PLSOpenSourceView : public PLSDialogView {
	Q_OBJECT

public:
	PLSOpenSourceView(QWidget *parent = nullptr);
	~PLSOpenSourceView() override;
	void loadURL(const QString &url);

private slots:
	void on_confirmButton_clicked();

private:
	Ui::PLSOpenSourceView *ui;
	pls::browser::BrowserWidget *m_browserWidget;
};

#endif // PLSOPENSOURCEVIEW_H
