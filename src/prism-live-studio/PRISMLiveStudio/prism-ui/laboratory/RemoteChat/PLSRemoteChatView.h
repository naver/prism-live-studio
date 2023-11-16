#ifndef PLSREMOTECHATVIEW_H
#define PLSREMOTECHATVIEW_H
#include "libbrowser.h"

#include "PLSSideBarDialogView.h"

namespace Ui {
class PLSRemoteChatView;
}

class PLSRemoteChatView : public PLSSideBarDialogView {
	Q_OBJECT

public:
	PLSRemoteChatView(DialogInfo info, QWidget *parent = nullptr);
	~PLSRemoteChatView() override;
	void setURL(const QString &url);

private:
	Ui::PLSRemoteChatView *ui;
	pls::browser::BrowserWidget *m_browserWidget;
};

#endif // PLSREMOTECHATVIEW_H
