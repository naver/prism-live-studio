#include "region-capture.h"
#include <QScreen>
#include <QPainter>
#include <QGuiApplication>
#include <QWindow>
#include <QDebug>
#include <QMouseEvent>
#include <QHBoxLayout>
#include <QPushButton>
#include <QTimer>
#include <set>
#include <QMutex>
#include <iostream>
#include "PLSHookKeyboard.h"
#ifdef _WIN32
#include <Windows.h>
#include <strsafe.h>
#endif

#include "libutils-api.h"
#include "libui.h"

const static int TIP_PADDING_H = 40;
const static int TIP_PADDING_V = 5;
const static int MENU_FRAME_WIDTH = 80;
const static int MENU_FRAME_HEIGHT = 40;
const static int MENU_BUTTON_WIDTH = 22;
const static int MENU_BUTTON_HEIGHT = 22;
const static int TIP_FONT_SIZE = 12;
const static QColor &tipRectColor = QColor("#effc35");
const static QColor &errorTextColor = QColor("#e02020");
const static QColor &normalTextColor = Qt::black;
const static QColor &selectRectBorderColor = QColor("#effc35");
const static QColor &backgroundColor = QColor(0, 0, 0, 1);

static void keyDownCallback(KeyMask mask, void *param)
{
	auto self = (RegionCapture *)(param);
	if (self && mask == KeyMask::Key_escape) {
		QMetaObject::invokeMethod(self, "close", Qt::QueuedConnection);
	}
}

RegionCapture::RegionCapture(QWidget *parent) : QWidget(parent)
{
	this->setMouseTracking(true);
	this->setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
	this->setAttribute(Qt::WA_TranslucentBackground);
	this->setAttribute(Qt::WA_DeleteOnClose);

	penLine.setWidth(1);
	penLine.setColor(QColor(30, 255, 255));

	penBorder.setWidth(2);
	penBorder.setColor(selectRectBorderColor);

	connect(qApp, &QGuiApplication::screenAdded, this, [this](const QScreen *) {
		qInfo("screen Added, recalculate frame rect");
		rectWhole = CalculateFrameSize();
		this->resize(rectWhole.size());
		this->move(rectWhole.x(), rectWhole.y());
	});
	connect(qApp, &QGuiApplication::screenRemoved, this, [this](const QScreen *screen) {
		auto gemetry = screen->geometry();
		qInfo("screen removed, recalculate frame rect");
		rectWhole = CalculateFrameSize();
		this->resize(rectWhole.size());
		this->move(rectWhole.x(), rectWhole.y());
		auto selectRect = QRect(mapToGlobal(rectSelected.topLeft()), rectSelected.size());
		if (gemetry.intersects(selectRect)) {
			Init();
		}
	});

	labelRect = pls_new<QLabel>(this);
	labelRect->setAttribute(Qt::WA_TransparentForMouseEvents);
	labelRect->setObjectName("labelRect");
	labelRect->setStyleSheet("#labelRect{border:2px solid #effc35;background-color:rgba(255, 255, 255, 0.2);}");
	labelRect->hide();

	menuFrame = pls_new<QFrame>(this);
	menuFrame->installEventFilter(this);
	menuFrame->setObjectName("frameMenu");

	labelTip = pls_new<QLabel>(this);
	labelTip->setAttribute(Qt::WA_TransparentForMouseEvents);
	labelTip->setAlignment(Qt::AlignVCenter | Qt::AlignHCenter);
	labelTip->setObjectName("CaptureTipLabel");

	auto btn_ok = pls_new<QPushButton>(menuFrame);
	btn_ok->setFocusPolicy(Qt::NoFocus);
	btn_ok->setObjectName("confirmBtn");
	btn_ok->setCursor(Qt::PointingHandCursor);
	connect(btn_ok, &QPushButton::clicked, this, [this]() {
		drawing = false;
		repaint();
		QPoint pos(this->mapToGlobal(rectSelected.topLeft()));
		qInfo() << "selected region point=" << pos << "size=" << rectSelected.size();
		std::cout << pos.x() << "|" << pos.y() << "|" << rectSelected.width() << "|" << rectSelected.height();
		emit selectedRegion({pos, rectSelected.size()});
		close();
	});
	auto btn_cancel = pls_new<QPushButton>(menuFrame);
	btn_cancel->setFocusPolicy(Qt::NoFocus);
	btn_cancel->setObjectName("cancelBtn");
	btn_cancel->setCursor(Qt::PointingHandCursor);
	connect(btn_cancel, &QPushButton::clicked, this, [this]() { this->Init(); });

	QLabel *label_space = pls_new<QLabel>(menuFrame);
	label_space->setObjectName("label_space");

	QHBoxLayout *layout = pls_new<QHBoxLayout>(menuFrame);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);
	layout->addStretch();
	layout->addWidget(btn_cancel);
	layout->addStretch();
	layout->addWidget(label_space);
	layout->addStretch();
	layout->addWidget(btn_ok);
	layout->addStretch();
	menuFrame->hide();

	auto css = pls_read_data(":/region-capture.css");
	menuFrame->setStyleSheet(css);
	pls_flush_style(menuFrame);
}

RegionCapture::~RegionCapture()
{
	ReleaseHook();
}

QRect RegionCapture::GetSelectedRect() const
{
	QPoint pos(this->mapToGlobal(rectSelected.topLeft()));
	qInfo() << "selected region point=" << pos << "size=" << rectSelected.size();
	return {pos, rectSelected.size()};
}

void RegionCapture::StartCapture(uint64_t maxWidth, uint64_t maxHeight)
{
	SetHook();
	maxRegionWidth = maxWidth;
	maxRegionHeight = maxHeight;
	this->setCursor(Qt::CrossCursor);
	rectWhole = CalculateFrameSize();
	qInfo() << "whole rect" << rectWhole;
	this->resize(rectWhole.size());
	this->move(rectWhole.x(), rectWhole.y());
	pointMoved = mapFromGlobal(QCursor::pos());
	SetCurrentPosDpi(mapToGlobal(pointMoved));
	SetCoordinateValue();
	this->raise();
	this->show();
}

void RegionCapture::paintEvent(QPaintEvent *event)
{
	QPainter dc(this);
	drawBackground(&dc, event->rect());
	QWidget::paintEvent(event);
}

void RegionCapture::mousePressEvent(QMouseEvent *event)
{
	if (event->button() == Qt::LeftButton) {
		labelRect->hide();
		rectSelected = QRect();
		pressed = true;
		hideMuenus();
		pointPressed = event->pos();
	}
	QWidget::mousePressEvent(event);
}

void RegionCapture::mouseMoveEvent(QMouseEvent *event)
{
	if (pressed)
		UpdateSelectedRect();
	if (rectSelected.isValid()) {
		labelRect->setGeometry(rectSelected);
		labelRect->show();
	}
	pointMoved = event->pos();
	if (menuFrame && !menuFrame->isVisible()) {
		SetCurrentPosDpi(mapToGlobal(pointMoved));
		SetCoordinateValue();
	}
	QWidget::mouseMoveEvent(event);
}

void RegionCapture::mouseReleaseEvent(QMouseEvent *event)
{
	if (event->button() == Qt::LeftButton) {
		pressed = false;
		pointReleased = event->pos();
		if (rectSelected.isValid()) {
			labelTip->hide();
			showMenus();
		} else
			labelRect->hide();
	}
	QWidget::mouseReleaseEvent(event);
}

void RegionCapture::keyReleaseEvent(QKeyEvent *event)
{
	if (event->key() == Qt::Key_Escape) {
		this->close();
	}
	QWidget::keyPressEvent(event);
}

bool RegionCapture::eventFilter(QObject *watched, QEvent *event)
{
	if (watched == menuFrame) {
		switch (event->type()) {
		case QEvent::MouseButtonPress:
		case QEvent::MouseButtonRelease:
		case QEvent::MouseButtonDblClick:
			return true;
		default:
			break;
		}
	}
	return QWidget::eventFilter(watched, event);
}

void RegionCapture::closeEvent(QCloseEvent *event)
{
	ReleaseHook();
	QWidget::closeEvent(event);
}

void RegionCapture::keyPressEvent(QKeyEvent *event)
{
	if (Qt::Key_Escape == event->key()) {
		close();
	}
	QWidget::keyPressEvent(event);
}

void RegionCapture::CheckMoveToDown(int &startX, int &startY) const
{
	//top of pressed point
	if (pointMoved.y() < pointPressed.y()) {
		// left
		if (pointMoved.x() < pointPressed.x()) {
			startX = overWidth ? (pointPressed.x() - (int)maxRegionWidth) : pointMoved.x();
			startY = overHeight ? (pointPressed.y() - (int)maxRegionHeight) : pointMoved.y();
		}
		//right
		if (pointMoved.x() > pointPressed.x()) {
			startX = pointPressed.x();
			startY = overHeight ? (pointPressed.y() - (int)maxRegionHeight) : pointMoved.y();
		}
	}
}

void RegionCapture::CheckMoveToTop(int &startX, int &startY) const
{
	//bottom  of pressed point
	if (pointMoved.y() > pointPressed.y()) {
		// left
		if (pointMoved.x() < pointPressed.x()) {
			startX = overWidth ? (pointPressed.x() - (int)maxRegionWidth) : pointMoved.x();
			startY = pointPressed.y();
		}
		//right
		if (pointMoved.x() > pointPressed.x()) {
			startX = pointPressed.x();
			startY = pointPressed.y();
		}
	}
}

void RegionCapture::UpdateSelectedRect()
{
	auto width = qAbs(pointMoved.x() - pointPressed.x());
	auto height = qAbs(pointMoved.y() - pointPressed.y());

	if (maxRegionWidth <= 0 || maxRegionHeight <= 0) {
		overWidth = false;
		overHeight = false;
	} else {
		overWidth = (width > maxRegionWidth);
		overHeight = (height > maxRegionHeight);
	}

	int startX = 0;
	int startY = 0;

	CheckMoveToDown(startX, startY);
	CheckMoveToTop(startX, startY);

	rectSelected.setRect(startX, startY, overWidth ? (int)maxRegionWidth : width, overHeight ? (int)maxRegionHeight : height);
}

void RegionCapture::drawBackground(QPainter *painter, const QRect &rect) const
{
	painter->save();
	painter->setCompositionMode(QPainter::CompositionMode_Source);
	painter->fillRect(rect, drawing ? backgroundColor : Qt::transparent);
	painter->restore();
}

void RegionCapture::drawCenterLines(QPainter *painter) const
{
	if (rectSelected.isValid())
		return;
	painter->save();
	painter->setPen(penLine);
	painter->drawLine(QPointF(pointMoved.x(), 0.0), QPointF(pointMoved.x(), this->height()));
	painter->drawLine(QPointF(0.0, pointMoved.y()), QPointF(this->width(), pointMoved.y()));
	painter->restore();
}

void RegionCapture::showMenus()
{
	this->setCursor(Qt::ArrowCursor);
	if (menuFrame) {
		SetMenuCoordinate();
		menuFrame->show();
	}
}

void RegionCapture::hideMuenus()
{
	this->setCursor(Qt::CrossCursor);
	if (menuFrame) {
		menuFrame->hide();
	}
}

void RegionCapture::Init()
{
	pointMoved = mapFromGlobal(QCursor::pos());
	SetCurrentPosDpi(mapToGlobal(pointMoved));
	pointPressed = QPoint();
	pointReleased = QPoint();
	rectSelected = QRect();
	pressed = false;
	if (labelRect) {
		labelRect->setGeometry(0, 0, 0, 0);
		labelRect->hide();
	}
	hideMuenus();
	SetCoordinateValue();
	this->repaint();
}

QRect RegionCapture::CalculateFrameSize()
{
	screenShots.clear();
	QList<QScreen *> listScreen = QGuiApplication::screens();
	int index = 0;
	int left = 0;
	int top = 0;
	int right = 0;
	int bottom = 0;
	for (auto screen : listScreen) {
		qDebug() << "screen" << index << "geometry=" << screen->geometry() << "\nDpi=" << screen->devicePixelRatio();
		QRect rect = screen->geometry();
		++index;
		if (rect.left() < left) {
			left = rect.left();
		}

		if (rect.right() > right) {
			right = rect.right();
		}

		if (rect.top() < top) {
			top = rect.top();
		}

		if (rect.bottom() > bottom) {
			bottom = rect.bottom();
		}

		ScreenShotData screenData;
		screenData.pixmap = screen->grabWindow(0);
		screenData.geometry = rect;
		screenShots << screenData;
	}
	return {QPoint(left, top), QPoint(right, bottom)};
}

void RegionCapture::SetTipCoordinate(int tipTextWidth, int tipTextHeight)
{
	auto pos = mapToGlobal(pointMoved);
	auto screen = QGuiApplication::screenAt(pos);
	QRect gloabalSelctedRect(mapToGlobal(rectSelected.topLeft()), rectSelected.size());
	if (screen) {
		QList<QRect> possibleRect;
		possibleRect << QRect(pos.x() + 5, pos.y() + 5, tipTextWidth, tipTextHeight);
		possibleRect << QRect(pos.x() - tipTextWidth - 5, pos.y() + 5, tipTextWidth, tipTextHeight);
		possibleRect << QRect(pos.x() + 5, pos.y() - tipTextHeight - 5, tipTextWidth, tipTextHeight);
		possibleRect << QRect(pos.x() - tipTextWidth - 5, pos.y() - 5 - tipTextHeight, tipTextWidth, tipTextHeight);

		for (int i = 0; i < possibleRect.size(); ++i) {
			if (screen->geometry().contains(possibleRect.at(i), true)) {
				auto targetPos = mapFromGlobal(possibleRect.at(i).topLeft());
				tipPositon.setX(targetPos.x());
				tipPositon.setY(targetPos.y());
				return;
			}
		}
	}

	tipPositon.setX(pos.x() - tipTextWidth - 5);
	tipPositon.setY(pos.y() - 5 - tipTextHeight);
}

void RegionCapture::SetMenuCoordinate()
{
	auto pos = mapToGlobal(rectSelected.bottomRight());
	auto screen = QGuiApplication::screenAt(pos);
	bool bottomRight = true;
	if (!screen) {
		bottomRight = false;
		pos = mapToGlobal(pointReleased);
		screen = QGuiApplication::screenAt(pos);
	}
	if (!screen)
		return;

	int w = MENU_FRAME_WIDTH;
	int h = MENU_FRAME_HEIGHT;
	QRect gloabalSelctedRect(mapToGlobal(rectSelected.topLeft()), rectSelected.size());
	QList<QRect> rectPossible;
	if (bottomRight) {
		rectPossible << QRect(pos.x() - w, pos.y() + 5, w, h);
		rectPossible << QRect(pos.x() + 5, pos.y() - h, w, h);
		rectPossible << QRect(pos.x() - w, pos.y() - rectSelected.height() - h - 5, w, h);
		rectPossible << QRect(pos.x() - rectSelected.width() - w - 5, pos.y() - h, w, h);
	} else {
		rectPossible << QRect(pos.x(), pos.y() + 5, w, h);
		rectPossible << QRect(pos.x(), pos.y() - 5 - h, w, h);
		rectPossible << QRect(pos.x() - w - 5, pos.y() - h, w, h);
		rectPossible << QRect(pos.x() + 5, pos.y() - h, w, h);
	}

	auto setFrameGeometry = [=](int x, int y) {
		menuGeometry.setX(x);
		menuGeometry.setY(y);
		menuGeometry.setWidth(w);
		menuGeometry.setHeight(h);
		menuFrame->setGeometry(menuGeometry);
		menuFrame->findChild<QPushButton *>("confirmBtn")->setFixedSize(MENU_BUTTON_WIDTH, MENU_BUTTON_HEIGHT);
		menuFrame->findChild<QPushButton *>("cancelBtn")->setFixedSize(MENU_BUTTON_WIDTH, MENU_BUTTON_HEIGHT);
		menuFrame->findChild<QLabel *>("label_space")->setFixedWidth(1);
	};

	for (int i = 0; i < rectPossible.size(); ++i) {
		if (screen->geometry().contains(rectPossible.at(i), true) && !gloabalSelctedRect.contains(rectPossible.at(i))) {
			auto targetPos = mapFromGlobal(rectPossible.at(i).topLeft());
			setFrameGeometry(targetPos.x(), targetPos.y());
			return;
		}
	}
	auto targetPos = mapFromGlobal(pos);
	setFrameGeometry(targetPos.x() - w, targetPos.y() - h);
}

void RegionCapture::SetCurrentPosDpi(const QPoint &posGlobal)
{
	auto screen = QGuiApplication::screenAt(posGlobal);
	if (!screen)
		return;
	auto newDpi = screen->devicePixelRatio();
	if (!screenDpi.has_value() || newDpi != screenDpi) {
		screenDpi = newDpi;
		labelTip->setStyleSheet(QString::asprintf("#CaptureTipLabel{color:black;background-color: #effc35;font-weight:bold;font-size:%dpx;}", TIP_FONT_SIZE));
		pls_flush_style(labelTip);
		pls_flush_style(menuFrame);
	}
}

void RegionCapture::SetHook()
{
	pls_init_keyboard_hook();
	struct pls_hook_callback_data cd;
	cd.callback = keyDownCallback;
	cd.param = this;
	pls_register_keydown_hook_callback(cd);
}

void RegionCapture::ReleaseHook() const
{
	pls_unregister_keydown_hook_callback(keyDownCallback, (void *)this);
}

void RegionCapture::SetCoordinateValue()
{
	QString strTip;
	if (rectSelected.isValid()) {
		if (!pressed) {
			labelTip->hide();
			return;
		}
		strTip = QString::asprintf("%d X %d", rectSelected.width(), rectSelected.height());
	} else {
		if (!pressed)
			strTip = QString::asprintf("%d,%d", pointMoved.x(), pointMoved.y());
		else
			strTip = QString::asprintf("0 X 0");
	}
	int w = labelTip->fontMetrics().horizontalAdvance(strTip) + TIP_PADDING_H;
	int h = labelTip->fontMetrics().height() + TIP_PADDING_V;
	SetTipCoordinate(w, h);
	labelTip->resize(w, h);
	labelTip->move(tipPositon);
	if (rectSelected.isValid()) {
		strTip = overWidth ? QString("<span style='color:red;'>%1</span>").arg(rectSelected.width()) : QString::number(rectSelected.width());
		strTip.append(" X ");
		strTip.append((overHeight ? QString("<span style='color:red;'>%1</span>").arg(rectSelected.height()) : QString::number(rectSelected.height())));
	}
	labelTip->setText(strTip);
	labelTip->show();
}
