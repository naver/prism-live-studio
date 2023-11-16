#include "PLSRemoteChatView.h"
#include "ui_PLSRemoteChatView.h"
#include "PLSRemoteChatManage.h"
#include "PLSLaboratoryManage.h"

PLSRemoteChatView::PLSRemoteChatView(DialogInfo info, QWidget *parent) : PLSSideBarDialogView(info, parent)
{
	RC_LOG("remote chat view init");
	ui = pls_new<Ui::PLSRemoteChatView>();
	pls_add_css(this, {"PLSRemoteChat"});
	ui->setupUi(this->content());
#if defined(Q_OS_WIN)
	setFixedSize(820, 600);
#elif defined(Q_OS_MACOS)
	setFixedSize(820, 560);
#endif
	setWindowTitle(LabManage->getStringInfo(LABORATORY_REMOTECHAT_ID, laboratory_data::g_laboratoryTitle));
	setResizeEnabled(false);
	QMetaObject::connectSlotsByName(this);
	setAttribute(Qt::WA_DeleteOnClose, true);

	auto closeEvent = [this](const QCloseEvent *) {
		hide();
		m_browserWidget->closeBrowser();
		return true;
	};
	setCloseEventCallback(closeEvent);
}

PLSRemoteChatView::~PLSRemoteChatView()
{
	RC_LOG("remote chat view released");
	pls_delete(ui);
}

void PLSRemoteChatView::setURL(const QString &url)
{
	m_browserWidget = pls::browser::newBrowserWidget(pls::browser::Params().url(url).initBkgColor(QColor(17, 17, 17)).showAtLoadEnded(true));
	ui->verticalLayout_2->addWidget(m_browserWidget);
}
