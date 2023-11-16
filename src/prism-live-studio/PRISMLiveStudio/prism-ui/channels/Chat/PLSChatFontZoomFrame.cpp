#include "PLSChatFontZoomFrame.h"
#include <frontend-api.h>
#include <QMouseEvent>
#include "PLSChatHelper.h"
#include "libui.h"
#include "log/log.h"
#include "ui_PLSChatFontZoomFrame.h"
static const char *const s_chatFontModuleName = "PLSChat";

PLSChatFontZoomFrame::PLSChatFontZoomFrame(QWidget *parent, QWidget *ignoreWidget) : QFrame(parent), m_ignoreWidget(ignoreWidget)
{
	ui = pls_new<Ui::PLSChatFontZoomFrame>();
	ui->setupUi(this);

	pls_add_css(this, {"PLSChatFontZoomFrame"});
	updateUIWithScale(PLSChatHelper::getFontSacleSize());
	auto centerShow = [this, parent]() {
		QPoint parentTB = parent->mapToGlobal(QPoint(parent->width(), parent->height()));
		QPoint popLeftTop = {parentTB.x() - 174, parentTB.y() + 6};
		move(popLeftTop);
	};
	centerShow();

	connect(ui->pushButton_minus, &QPushButton::clicked, this, [this]() {
		PLS_UI_STEP(s_chatFontModuleName, "PLSChat Dialog font size minus", ACTION_CLICK);
		fontChangeBtnClick(false);
	});
	connect(ui->pushButton_plus, &QPushButton::clicked, this, [this]() {
		PLS_UI_STEP(s_chatFontModuleName, "PLSChat Dialog font size plus", ACTION_CLICK);
		fontChangeBtnClick(true);
	});
	this->installEventFilter(this);
	PLS_INFO(s_chatFontModuleName, "PLSChat Dialog font size frame shown");
}

PLSChatFontZoomFrame::~PLSChatFontZoomFrame()
{
	pls_delete(ui, nullptr);
}

static bool isClickInWidget(QEvent *i_Event, const QWidget *noticeWidget)
{
	if (!noticeWidget) {
		return false;
	}
	const QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(i_Event);
	if (!mouseEvent) {
		return false;
	}

	QPoint noticePoint = noticeWidget->mapToGlobal(QPoint(0, 0));
	QRect noticeRect(noticePoint.x(), noticePoint.y(), noticeWidget->width(), noticeWidget->height());

	return noticeRect.contains(mouseEvent->globalPosition().toPoint());
}

bool PLSChatFontZoomFrame::eventFilter(QObject *i_Object, QEvent *i_Event)
{
	if (i_Object == this && (i_Event->type() == QEvent::MouseButtonPress)) {
		if (isClickInWidget(i_Event, parentWidget()) || isClickInWidget(i_Event, m_ignoreWidget)) {
			QMetaObject::invokeMethod(
				this, [this]() { this->close(); }, Qt::QueuedConnection);
			return true;
		}
	}

	return QWidget::eventFilter(i_Object, i_Event);
}

void PLSChatFontZoomFrame::fontChangeBtnClick(bool isPlus)
{
	auto newScale = PLS_CHAT_HELPER->getNextSacelSize(isPlus);
	PLSChatHelper::sendWebChatFontSizeChanged(newScale);
	updateUIWithScale(newScale);
}

void PLSChatFontZoomFrame::updateUIWithScale(int scale)
{
	auto scaleStatus = PLS_CHAT_HELPER->getFontBtnStatus(scale);

	ui->pushButton_minus->setEnabled(true);
	ui->pushButton_plus->setEnabled(true);

	if (PLSChatHelper::ChatFontSacle::PlusDisable == scaleStatus) {
		ui->pushButton_plus->setEnabled(false);
	} else if (PLSChatHelper::ChatFontSacle::MinusDisable == scaleStatus) {
		ui->pushButton_minus->setEnabled(false);
	}
	ui->sizeLabel->setText(QString("%1%").arg(scale));
}
