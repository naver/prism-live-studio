#include "PLSScheLiveNotice.h"
#include "ui_PLSScheLiveNotice.h"

#include "ChannelCommonFunctions.h"

#include "log/log.h"
#include "action.h"

#include <QResizeEvent>

constexpr auto MODULE_NAME = "PLSScheLiveNotice";

PLSScheLiveNotice::PLSScheLiveNotice(const PLSPlatformBase *, const QString &title_, const QString &startTime, QWidget *parent) : PLSDialogView(parent), title(title_)
{
	ui = pls_new<Ui::PLSScheLiveNotice>();
	pls_add_css(this, {"PLSScheLiveNotice"});
	setResizeEnabled(false);

	setupUi(ui);
#if defined(Q_OS_WIN)
	initSize({410, 297});
#elif defined(Q_OS_MACOS)
	initSize({410, 257});
#endif

	ui->scheLiveTitleLabel->installEventFilter(this);

	ui->scheLiveTitleLabel->setText(title);
	ui->scheLiveStartTimeLabel->setText(startTime);

	setMessageText(tr("Schedule.Live.Notice.Message"));
	activateWindow();
}

PLSScheLiveNotice::~PLSScheLiveNotice()
{
	pls_delete(ui, nullptr);
}

void PLSScheLiveNotice::setMessageText(const QString &messageText_)
{
	this->messageText = messageText_;
	ui->messageLabel->setText(messageText_);
}

bool PLSScheLiveNotice::eventFilter(QObject *watched, QEvent *event)
{
	if ((ui->scheLiveTitleLabel == watched) && (event->type() == QEvent::Resize)) {
		ui->scheLiveTitleLabel->setText(ui->scheLiveTitleLabel->fontMetrics().elidedText(title, Qt::ElideRight, static_cast<QResizeEvent *>(event)->size().width()));
	}

	return PLSDialogView::eventFilter(watched, event);
}

void PLSScheLiveNotice::on_okButton_clicked()
{
	PLS_UI_STEP(MODULE_NAME, "Schedule Live Notice's Ok Button", ACTION_CLICK);
	accept();
}
