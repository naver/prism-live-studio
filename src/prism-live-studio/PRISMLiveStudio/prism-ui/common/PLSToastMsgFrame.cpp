#include "PLSToastMsgFrame.h"
//#include "PLSDpiHelper.h"
#include "action.h"
#include "liblog.h"
#include "log/module_names.h"
#include <QHBoxLayout>
#include <QPainter>
#include <QStyleOption>
#include "utils-api.h"

const int TOAST_DISAPPEAR = 5 * 1000;

PLSToastMsgFrame::PLSToastMsgFrame(QWidget *parent) : QFrame(parent)
{
	setAttribute(Qt::WA_NativeWindow);
	timerDisappear.setSingleShot(true);
	connect(&timerDisappear, &QTimer::timeout, this, &QFrame::hide);
	this->setCursor(Qt::ArrowCursor);
	btnClose = pls_new<QPushButton>(this);
	btnClose->setFocusPolicy(Qt::NoFocus);
	btnClose->setObjectName("toastMsgCloseBtn");
	connect(btnClose, &QPushButton::clicked, [this]() {
		PLS_UI_STEP(MAIN_GIPHY_STICKER_MODULE, "close toast button", ACTION_CLICK);
		if (timerDisappear.isActive())
			timerDisappear.stop();
		this->hide();
	});

	editMessage = pls_new<QTextEdit>(this);
	editMessage->setReadOnly(true);
	editMessage->setTextInteractionFlags(Qt::NoTextInteraction);
	editMessage->setContextMenuPolicy(Qt::NoContextMenu);
	editMessage->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
	editMessage->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
	editMessage->setObjectName("toastMsgEdit");
	editMessage->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	editMessage->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	editMessage->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
	editMessage->viewport()->setCursor(Qt::ArrowCursor);
	editMessage->setCursor(Qt::ArrowCursor);

	auto main_layout = pls_new<QHBoxLayout>(this);
	main_layout->setContentsMargins(10, 10, 10, 10);
	main_layout->setSpacing(0);
	main_layout->addWidget(editMessage);
}

PLSToastMsgFrame::~PLSToastMsgFrame()
{
	if (timerDisappear.isActive())
		timerDisappear.stop();
}

void PLSToastMsgFrame::SetMessage(const QString &message)
{
	QTextOption option;
	option.setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
	option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
	editMessage->document()->setDefaultTextOption(option);
	editMessage->setText(message);
	QTimer::singleShot(0, this, SLOT(resizeToContent()));
}

QString PLSToastMsgFrame::GetMessageContent() const
{
	return editMessage->toPlainText();
}

void PLSToastMsgFrame::SetShowWidth(int width)
{
	showWidth = width;
	QTimer::singleShot(0, this, SLOT(resizeToContent()));
}

void PLSToastMsgFrame::ShowToast()
{
	this->raise();
	timerDisappear.start(TOAST_DISAPPEAR);
	QTimer::singleShot(0, this, [this]() {
		this->show();
		resizeToContent();
	});
}

void PLSToastMsgFrame::HideToast()
{
	if (timerDisappear.isActive())
		timerDisappear.stop();
	this->hide();
}

void PLSToastMsgFrame::resizeEvent(QResizeEvent *event)
{
	const int padding = 7;
	btnClose->move(this->width() - btnClose->width() - padding, padding);
	btnClose->raise();
	QFrame::resizeEvent(event);
	resizeToContent();
}

void PLSToastMsgFrame::resizeToContent()
{
	auto size = editMessage->document()->size();
	int top;
	int bottom;
	int left;
	int right;
	this->layout()->getContentsMargins(&left, &top, &right, &bottom);
	auto heightValue = size.height() + top + bottom + 10;
	this->resize(showWidth, static_cast<int>(heightValue));
	emit resizeFinished();
}
