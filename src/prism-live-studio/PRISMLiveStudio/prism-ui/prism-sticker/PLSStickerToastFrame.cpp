#include "PLSStickerToastFrame.h"
#include "action.h"
#include "liblog.h"
#include "log/module_names.h"
#include <QHBoxLayout>
#include <QPainter>
#include <QStyleOption>
#include "utils-api.h"
#include "libutils-api.h"
#include "libui.h"

const int STICKER_TOAST_DISAPPEAR = 3 * 1000;

PLSStickerToastFrame::PLSStickerToastFrame(QWidget *parent) : QFrame(parent)
{
	pls_uistep_v2_set_custom_show_hide_name(this, "Sticker Toast View");
	setAttribute(Qt::WA_NativeWindow);
	timerDisappear.setSingleShot(true);
	setAttribute(Qt::WA_DeleteOnClose, true);
	connect(&timerDisappear, &QTimer::timeout, this, &QFrame::close);
	this->setCursor(Qt::ArrowCursor);
	btnClose = pls_new<QPushButton>(this);
	btnClose->setFocusPolicy(Qt::NoFocus);
	btnClose->setObjectName("stickerToastCloseBtn");
	pls_uistep_v2_set_value(btnClose, "Sticker Toast Close");
	connect(btnClose, &QPushButton::clicked, [this]() {
		PLS_UI_STEP(MAIN_PRISM_STICKER, "close sticker toast button", ACTION_CLICK);
		if (timerDisappear.isActive())
			timerDisappear.stop();
		this->close();
	});

	editMessage = pls_new<QTextEdit>(this);
	editMessage->setReadOnly(true);
	editMessage->setTextInteractionFlags(Qt::NoTextInteraction);
	editMessage->setContextMenuPolicy(Qt::NoContextMenu);
	editMessage->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
	editMessage->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
	editMessage->setObjectName("stickerToastEdit");
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

PLSStickerToastFrame::~PLSStickerToastFrame()
{
	if (timerDisappear.isActive())
		timerDisappear.stop();
}

void PLSStickerToastFrame::SetMessage(const QString &message)
{
	QTextOption option;
	option.setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
	option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
	editMessage->document()->setDefaultTextOption(option);
	editMessage->setText(message);
}

QString PLSStickerToastFrame::GetMessageContent() const
{
	return editMessage->toPlainText();
}

void PLSStickerToastFrame::ShowToast()
{
	timerDisappear.start(STICKER_TOAST_DISAPPEAR);
}

void PLSStickerToastFrame::HideToast()
{
	if (timerDisappear.isActive())
		timerDisappear.stop();
	this->close();
}

void PLSStickerToastFrame::calcFixedHeight()
{
	auto size = editMessage->document()->size();
	int top;
	int bottom;
	int left;
	int right;
	this->layout()->getContentsMargins(&left, &top, &right, &bottom);
	auto heightValue = size.height() + top + bottom;
	setFixedHeight(heightValue);
}

void PLSStickerToastFrame::resizeEvent(QResizeEvent *event)
{
	const int padding = 5;
	btnClose->move(this->width() - btnClose->width() - padding, padding);
	btnClose->raise();
	QFrame::resizeEvent(event);
}

void PLSStickerToastFrame::showEvent(QShowEvent *event)
{
	calcFixedHeight();
	QFrame::showEvent(event);
}
