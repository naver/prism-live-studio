#include "dialog-view.hpp"
#include "ui_PLSDialogView.h"
#include <QApplication>
#include <QDesktopWidget>
#include <QScreen>
#include <QResizeEvent>
#include <QPainter>
#include <QLabel>
#include <QComboBox>
#include <QLineEdit>
#include <QTextEdit>
#include <qlistwidget.h>
#include <QAbstractSpinBox>
#include <QHBoxLayout>
#include <QTimer>
#include <QPointer>
#include <QWidget>
#include <QContiguousCache>
#include <log.h>
#include <action.h>

#if defined(Q_OS_WINDOWS)
#include <Windows.h>
#endif // Q_OS_WINDOWS

#include "frontend-api.h"

const int BORDER_WIDTH = 1;
const int CORNER_WIDTH = 5;
const QString CONTENT = QStringLiteral("content");

FRONTEND_API float getDevicePixelRatio()
{
	QScreen *screen = QApplication::screenAt(QCursor::pos());
	return screen ? screen->devicePixelRatio() : QApplication::primaryScreen()->devicePixelRatio();
}

FRONTEND_API float getDevicePixelRatio(QWidget *widget)
{
	int screenNumber = QApplication::desktop()->screenNumber(widget);
	QScreen *screen = screenNumber >= 0 ? QApplication::screens()[screenNumber] : QApplication::primaryScreen();
	return screen->devicePixelRatio();
}

FRONTEND_API QRect getScreenRect(QWidget *widget)
{
	int screenNumber = QApplication::desktop()->screenNumber(widget);
	QScreen *screen = screenNumber >= 0 ? QApplication::screens()[screenNumber] : QApplication::primaryScreen();
	return screen->geometry();
}

FRONTEND_API QRect getScreenAvailableRect(QWidget *widget)
{
	int screenNumber = QApplication::desktop()->screenNumber(widget);
	QScreen *screen = screenNumber >= 0 ? QApplication::screens()[screenNumber] : QApplication::primaryScreen();
	return screen->availableGeometry();
}

FRONTEND_API QRect getScreenAvailableRect(const QPoint &pt)
{
	QScreen *screen = QApplication::screenAt(pt);
	return screen ? screen->availableGeometry() : QApplication::primaryScreen()->availableGeometry();
}

FRONTEND_API QRect getScreenAvailableVirtualRect(const QPoint &pt)
{
	QScreen *screen = QApplication::screenAt(pt);
	return screen ? screen->availableVirtualGeometry() : QApplication::primaryScreen()->availableVirtualGeometry();
}

PLSDialogView::PLSDialogView(QWidget *parent) : PLSToplevelView<QDialog>(parent, Qt::Dialog | Qt::FramelessWindowHint), ui(new Ui::PLSDialogView)
{
	ui->setupUi(this);
	setMouseTracking(true);
	ui->titleBar->setMouseTracking(true);
	ui->contentBorder->setMouseTracking(true);
	ui->content->setMouseTracking(true);
	ui->titleLabel->setMouseTracking(this);
	ui->titleBar->installEventFilter(this);
	ui->contentBorder->installEventFilter(this);
	ui->content->installEventFilter(this);
	ui->titleLabel->installEventFilter(this);

	owidget = nullptr;
	captionHeight = captionButtonSize = GetSystemMetrics(SM_CYCAPTION);
	captionButtonMargin = 0;
	textMarginLeft = 8;
	closeButtonMarginRight = 0;
	hasCaption = true;
	hasHLine = true;
	hasBorder = true;
	hasMinButton = false;
	hasMaxResButton = false;
	hasCloseButton = true;
	isMoveInContent = false;
	closeEventCallback = [this](QCloseEvent *e) {
		callBaseCloseEvent(e);
		return true;
	};
	isEscapeCloseEnabled = false;

	this->ui->min->setVisible(hasMinButton);
	this->ui->maxres->setVisible(hasMaxResButton);

	connect(ui->close, &QToolButton::clicked, [this]() {
		PLS_UI_STEP(getModuleName(), (getViewName() + "'s Close Button").c_str(), ACTION_CLICK);
		reject();
	});
	QObject::connect(ui->maxres, &QToolButton::clicked, [this]() {
		if (!isMaxState && !isFullScreenState) {
			showMaximized();
		} else {
			showNormal();
		}
	});
	connect(ui->min, &QToolButton::clicked, [this]() {
		PLS_UI_STEP(getModuleName(), (getViewName() + "'s Minimized Button").c_str(), ACTION_CLICK);
		showMinimized();
	});

	connect(this, &PLSDialogView::windowTitleChanged, ui->titleLabel, &QLabel::setText);
	connect(ui->content, &QWidget::windowTitleChanged, this, &PLSDialogView::setWindowTitle);

	ui->content->setObjectName(QString());
	connect(ui->content, &QObject::objectNameChanged, [this](const QString &objectName) {
		if (objectName != CONTENT) {
			setObjectName(objectName);
			ui->content->setObjectName(CONTENT);
		}
	});
}

PLSDialogView::~PLSDialogView()
{
	delete ui;
}

using widgetPtr = QPointer<QWidget>;
static QContiguousCache<widgetPtr> widgetStack(20);

int PLSDialogView::exec()
{
	widgetPtr lastAvailable;
	QWidget *lastParent = this->parentWidget();
	for (int i = widgetStack.count() - 1; i >= 0; --i) {
		auto last = widgetStack.at(i);
		if (!last.isNull() && last->isVisible() && last->isModal()) {
			lastAvailable = last;
			break;
		}
	}
	auto lastFlags = this->windowFlags();
	if (!lastAvailable.isNull()) {
		this->setParent(lastAvailable);
		this->setWindowFlags(lastFlags);
		this->setStyleSheet("QPushButton { min-width: 128px;}");
	}

	widgetStack.append(widgetPtr(this));
	int ret = PLSToplevelView<QDialog>::exec();

	if (!lastAvailable.isNull()) {
		this->setParent(lastParent);
		this->setWindowFlags(lastFlags);
	}

	return ret;
}

QWidget *PLSDialogView::content() const
{
	return ui->content;
}

QWidget *PLSDialogView::widget() const
{
	return owidget;
}

void PLSDialogView::setWidget(QWidget *widget)
{
	this->owidget = widget;
	widget->setParent(ui->content);
	connect(widget, &QWidget::windowTitleChanged, this, &PLSDialogView::setWindowTitle);
	setWindowTitle(widget->windowTitle());

	QHBoxLayout *l = new QHBoxLayout(ui->content);
	l->setMargin(0);
	l->setSpacing(0);
	l->addWidget(widget);
}

int PLSDialogView::getCaptionHeight() const
{
	return captionHeight;
}

void PLSDialogView::setCaptionHeight(int captionHeight)
{
	int borderWidth = hasBorder ? BORDER_WIDTH : 0;
	this->captionHeight = captionHeight - borderWidth;
	ui->titleBar->setFixedHeight(this->captionHeight);
}

int PLSDialogView::getCaptionButtonSize() const
{
	return captionButtonSize;
}

void PLSDialogView::setCaptionButtonSize(int captionButtonSize)
{
	this->captionButtonSize = captionButtonSize;
	ui->min->setFixedSize(captionButtonSize, captionButtonSize);
	ui->maxres->setFixedSize(captionButtonSize, captionButtonSize);
	ui->close->setFixedSize(captionButtonSize, captionButtonSize);
}

int PLSDialogView::getCaptionButtonMargin() const
{
	return captionButtonMargin;
}

void PLSDialogView::setCaptionButtonMargin(int captionButtonMargin)
{
	this->captionButtonMargin = captionButtonMargin;
	ui->titleButtonLayout->setSpacing(captionButtonMargin);
}

int PLSDialogView::getTextMarginLeft() const
{
	return textMarginLeft;
}

void PLSDialogView::setTextMarginLeft(int textMarginLeft)
{
	this->textMarginLeft = textMarginLeft;
	QMargins margins = ui->titleBarLayout->contentsMargins();
	margins.setLeft(textMarginLeft);
	ui->titleBarLayout->setContentsMargins(margins);
}

int PLSDialogView::getCloseButtonMarginRight() const
{
	return closeButtonMarginRight;
}

void PLSDialogView::setCloseButtonMarginRight(int closeButtonMarginRight)
{
	this->closeButtonMarginRight = closeButtonMarginRight;
	QMargins margins = ui->titleBarLayout->contentsMargins();
	margins.setRight(closeButtonMarginRight);
	ui->titleBarLayout->setContentsMargins(margins);
}

bool PLSDialogView::getHasCaption() const
{
	return hasCaption;
}

void PLSDialogView::setHasCaption(bool hasCaption)
{
	this->hasCaption = hasCaption;
	ui->close->setVisible(hasCaption);
	ui->min->setVisible(hasCaption && hasMinButton);
	ui->titleBar->setVisible(hasCaption);
}

bool PLSDialogView::getHasHLine() const
{
	return hasHLine;
}

void PLSDialogView::setHasHLine(bool hasHLine)
{
	this->hasHLine = hasHLine;
	ui->hline->setVisible(hasHLine);
}

bool PLSDialogView::getHasBorder() const
{
	return hasBorder;
}

void PLSDialogView::setHasBorder(bool hasBorder)
{
	this->hasBorder = hasBorder;
	update();
}

bool PLSDialogView::getHasMinButton() const
{
	return hasMinButton;
}

bool PLSDialogView::getHasMaxResButton() const
{
	return hasMaxResButton;
}

void PLSDialogView::setHasMaxResButton(bool hasMaxResButton)
{
	this->hasMaxResButton = hasMaxResButton;
	ui->maxres->setVisible(hasMaxResButton);
}

void PLSDialogView::setHasMinButton(bool hasMinButton)
{
	this->hasMinButton = hasMinButton;
	ui->min->setVisible(hasMinButton);
}

bool PLSDialogView::getHasCloseButton() const
{
	return hasCloseButton;
}

void PLSDialogView::setHasCloseButton(bool hasCloseButton)
{
	this->hasCloseButton = hasCloseButton;
	ui->close->setVisible(hasCloseButton);
}

bool PLSDialogView::getEscapeCloseEnabled() const
{
	return isEscapeCloseEnabled;
}

void PLSDialogView::setEscapeCloseEnabled(bool enabled)
{
	isEscapeCloseEnabled = enabled;
}

void PLSDialogView::setCloseEventCallback(std::function<bool(QCloseEvent *)> closeEventCallback)
{
	this->closeEventCallback = closeEventCallback;
}

void PLSDialogView::callBaseCloseEvent(QCloseEvent *event)
{
	if (event) {
		QDialog::closeEvent(event);
	}
}

void PLSDialogView::sizeToContent(const QSize &size)
{
	QSize newSize = ui->content->layout()->minimumSize();
	if (size.isValid()) {
		newSize.setWidth(qMax(size.width(), newSize.width()));
		newSize.setHeight(qMax(size.height(), newSize.height()));
	}

	if (hasCaption) {
		newSize.setHeight(newSize.height() + captionHeight + (hasHLine && hasBorder ? BORDER_WIDTH : 0));
	}

	if (hasBorder) {
		newSize.setWidth(newSize.width() + BORDER_WIDTH * 2);
		newSize.setHeight(newSize.height() + BORDER_WIDTH * 2);
	}

	resize(newSize);
}

void PLSDialogView::setHeightForFixedWidth()
{
	int fixedWidth = minimumWidth();
	int height = layout()->minimumHeightForWidth(fixedWidth);
	setFixedHeight(height);
}

static inline QPoint operator+(const QPoint &pt, const QSize &sz)
{
	return QPoint(pt.x() + sz.width(), pt.y() + sz.height());
}

void PLSDialogView::moveToCenter()
{
	QWidget *toplevel = pls_get_toplevel_view(parentWidget());
	if (toplevel)
		move(toplevel->mapToGlobal(QPoint(0, 0)) + (toplevel->size() - this->size()) / 2);
}

const char *PLSDialogView::getModuleName() const
{
	return MODULE_FRONTEND_API;
}

std::string PLSDialogView::getViewName() const
{
	return metaObject()->className();
}

void PLSDialogView::done(int result)
{
	if (closeEventCallback(nullptr)) {
		QDialog::done(result);
	}
}

QRect PLSDialogView::titleBarRect() const
{
	return hasCaption ? ui->titleBar->geometry() : QRect();
}

bool PLSDialogView::canMaximized() const
{
	return hasCaption && hasMaxResButton;
}

bool PLSDialogView::canFullScreen() const
{
	return false;
}

void PLSDialogView::flushMaxFullScreenStateStyle()
{
	pls_flush_style(ui->maxres);
}

void PLSDialogView::closeEvent(QCloseEvent *event)
{
	closeEventCallback(event);
}

void PLSDialogView::showEvent(QShowEvent *event)
{
	emit shown();
	QDialog::showEvent(event);
}

void PLSDialogView::keyPressEvent(QKeyEvent *event)
{
	if (event->key() != Qt::Key_Escape) {
		QDialog::keyPressEvent(event);
	} else if (isEscapeCloseEnabled) {
		event->accept();
		close();
	} else {
		event->ignore();
	}
}

void PLSDialogView::keyReleaseEvent(QKeyEvent *event)
{
	if (event->key() != Qt::Key_Escape) {
		QDialog::keyReleaseEvent(event);
	} else {
		event->ignore();
	}
}

void PLSDialogView::mousePressEvent(QMouseEvent *event)
{
	mousePress(event);
}

void PLSDialogView::mouseReleaseEvent(QMouseEvent *event)
{
	mouseRelease(event);
}

void PLSDialogView::mouseDoubleClickEvent(QMouseEvent *event)
{
	mouseDbClick(event);
}

void PLSDialogView::mouseMoveEvent(QMouseEvent *event)
{
	mouseMove(event);
}

bool PLSDialogView::eventFilter(QObject *watcher, QEvent *event)
{
	if (watcher == ui->content) {
		switch (event->type()) {
		case QEvent::ChildAdded:
			if (QWidget *widget = dynamic_cast<QWidget *>(static_cast<QChildEvent *>(event)->child()); widget) {
				widget->setCursor(Qt::ArrowCursor);
			}
			break;
		}
	}

	if (watcher == this || watcher == ui->titleBar || watcher == ui->content || watcher == ui->contentBorder) {
		switch (event->type()) {
		case QEvent::MouseButtonPress:
			mousePress(static_cast<QMouseEvent *>(event));
			break;
		case QEvent::MouseButtonRelease:
			mouseRelease(static_cast<QMouseEvent *>(event));
			break;
		case QEvent::MouseButtonDblClick:
			mouseDbClick(static_cast<QMouseEvent *>(event));
			break;
		case QEvent::MouseMove:
			mouseMove(static_cast<QMouseEvent *>(event));
			break;
		}
	}

	if (watcher == ui->titleLabel) {
		switch (event->type()) {
		case QEvent::Resize:
			ui->titleLabel->setText(QFontMetrics(font()).elidedText(windowTitle(), Qt::ElideRight, static_cast<QResizeEvent *>(event)->size().width()));
			break;
		}
	}

	return QDialog::eventFilter(watcher, event);
}
