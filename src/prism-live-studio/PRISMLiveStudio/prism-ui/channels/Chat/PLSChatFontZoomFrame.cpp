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
	updateUIWithScale(PLSChatHelper::getFontScaleSize());
	auto centerShow = [this, parent]() {
		QPoint parentTB = parent->mapToGlobal(QPoint(parent->width(), parent->height()));
		QPoint popLeftTop = {parentTB.x() - 174, parentTB.y() + 6};
		move(popLeftTop);
	};
	centerShow();
	pls_uistep_v2_set_value(ui->pushButton_minus, QStringLiteral("clicked"), QStringLiteral("minus"));
	pls_uistep_v2_set_value(ui->pushButton_plus, QStringLiteral("clicked"), QStringLiteral("plus"));

	connect(ui->pushButton_minus, &QPushButton::clicked, this, [this]() { fontChangeBtnClick(false); });
	connect(ui->pushButton_plus, &QPushButton::clicked, this, [this]() { fontChangeBtnClick(true); });
	this->installEventFilter(this);
	pls_uistep_v2_set_title(this, "Chat Dock Font Size Popup");
	pls_uistep_v2_set_custom_show_hide_name(this, "PLSChatFontZoomFrame");
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
			QMetaObject::invokeMethod(this, [this]() { this->close(); }, Qt::QueuedConnection);
			return true;
		}
	}

	return QWidget::eventFilter(i_Object, i_Event);
}

void PLSChatFontZoomFrame::fontChangeBtnClick(bool isPlus)
{
	auto newScale = PLS_CHAT_HELPER->getNextScaleSize(isPlus);
	PLSChatHelper::sendWebChatFontSizeChanged(newScale);
	updateUIWithScale(newScale);
	PLS_UI_ACTION("PLSChatFontZoomFrame font changed to: %s", (isPlus ? "plus" : "mminus"));
}

void PLSChatFontZoomFrame::updateUIWithScale(int scale)
{
	auto scaleStatus = PLS_CHAT_HELPER->getFontBtnStatus(scale);

	ui->pushButton_minus->setEnabled(true);
	ui->pushButton_plus->setEnabled(true);

	if (PLSChatHelper::ChatFontScale::PlusDisable == scaleStatus) {
		ui->pushButton_plus->setEnabled(false);
	} else if (PLSChatHelper::ChatFontScale::MinusDisable == scaleStatus) {
		ui->pushButton_minus->setEnabled(false);
	}
	ui->sizeLabel->setText(QString("%1%").arg(scale));
}
