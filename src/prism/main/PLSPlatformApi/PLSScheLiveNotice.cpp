#include "PLSScheLiveNotice.h"
#include "ui_PLSScheLiveNotice.h"

#include "ChannelCommonFunctions.h"

#include "log.h"
#include "action.h"

#include <QResizeEvent>

#define MODULE_NAME "PLSScheLiveNotice"

PLSScheLiveNotice::PLSScheLiveNotice(PLSPlatformBase *, const QString &title_, const QString &startTime, QWidget *parent, PLSDpiHelper dpiHelper)
	: PLSDialogView(parent, dpiHelper), ui(new Ui::PLSScheLiveNotice), title(title_)
{
	dpiHelper.setCss(this, {PLSCssIndex::PLSScheLiveNotice});

	setResizeEnabled(false);

	ui->setupUi(content());
	QMetaObject::connectSlotsByName(this);

	ui->scheLiveTitleLabel->installEventFilter(this);

	ui->scheLiveTitleLabel->setText(title);
	ui->scheLiveStartTimeLabel->setText(startTime);

	setMessageText(tr("Schedule.Live.Notice.Message"));
	activateWindow();
}

PLSScheLiveNotice::~PLSScheLiveNotice()
{
	delete ui;
}

void PLSScheLiveNotice::setMessageText(const QString &messageText)
{
	this->messageText = messageText;
	ui->messageLabel->setText(messageText);
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
