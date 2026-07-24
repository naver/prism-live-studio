#include "PLSToastMsgPopup.hpp"
#include "ui_PLSToastMsgPopup.h"
#include <QStyle>
#include "libutils-api.h"

constexpr auto MSGTYPE = "toastWaning";
constexpr int TOAST_CONTENT_PADDING = 12;
constexpr int TOAST_CONTENT_HEIGHT_MARGIN = 4;

PLSToastMsgPopup::PLSToastMsgPopup(QWidget *parent) : QLabel(parent)
{
	ui = pls_new<Ui::PLSToastMsgPopup>();
	setAttribute(Qt::WA_NativeWindow);
	m_timer = pls_new<QTimer>(this);
	ui->setupUi(this);
	setFixedWidth(325);
	setMargin(TOAST_CONTENT_PADDING);
	setScaledContents(false);
	connect(m_timer, &QTimer::timeout, [this]() {
		m_timer->stop();
		this->hide();
	});
	this->style()->unpolish(this);
	this->style()->polish(this);
}

PLSToastMsgPopup::~PLSToastMsgPopup()
{
	pls_delete(ui);
}

void PLSToastMsgPopup::showMsg(const QString &msg, pls_toast_info_type type)
{
	if (m_timer->isActive()) {
		m_timer->stop();
	}
	m_timer->start(5000);
	switch (type) {
	case pls_toast_info_type::PLS_TOAST_NOTICE:
	case pls_toast_info_type::PLS_TOAST_REPLY_BUFFER:
		setProperty(MSGTYPE, true);
		break;
	case pls_toast_info_type::PLS_TOAST_ERROR:
		setProperty(MSGTYPE, false);
		break;
	default:
		break;
	}
	pls_flush_style(this);
	setText(msg);
	const int contentMargin = margin();
	const int contentWidth = qMax(1, width() - contentMargin * 2);
	const auto size = pls_calculate_size_for_width(msg, font(), contentWidth);
	resize(width(), qMax(minimumHeight(), size.height() + contentMargin * 2 + TOAST_CONTENT_HEIGHT_MARGIN));
}

void PLSToastMsgPopup::mousePressEvent(QMouseEvent *event)
{
	if (event->button() == Qt::LeftButton) {
		hide();
	}
	QLabel::mousePressEvent(event);
}
