#include "PLSWindow.h"
#include "ui_PLSWindow.h"

#include <qapplication.h>
#include <qscreen.h>
#include <qevent.h>
#include <qpainter.h>
#include <qlabel.h>
#include <qcombobox.h>
#include <qlineedit.h>
#include <qtextedit.h>
#include <qlistwidget.h>
#include <qabstractspinbox.h>
#include <qboxlayout.h>
#include <qtimer.h>
#include <qpointer.h>
#include <qwidget.h>
#include <qcheckbox.h>
#include <qcombobox.h>

#include <liblog.h>
#include <libutils-api.h>
#include <QToolTip>

#pragma comment(lib, "Shcore.lib")

constexpr const char *TOPLEVELVIEW_MODULE = "ToplevelView";

const QString CONTENT = QStringLiteral("content");

PLSWindow::PLSWindow(QWidget *parent, Qt::WindowFlags f) : PLSToplevelView<QWidget>(parent, f)
{
	ui = pls_new<Ui::PLSWindow>();

	ui->setupUi(this);
#if defined(Q_OS_WIN)
	ui->min->setAttribute(Qt::WA_NativeWindow);
#endif
	closeEventCallback = [this](QCloseEvent *e) {
		callBaseCloseEvent(e);
		return true;
	};

#if defined(Q_OS_MACOS)

	ui->titleBar->hide();

	setHasMinButton(hasMinButton);
	setHasMaxResButton(hasMaxResButton);

#else
	this->ui->min->setVisible(hasMinButton);
	this->ui->maxres->setVisible(hasMaxResButton);

	connect(ui->close, &QToolButton::clicked, [this]() {
		if (closeButtonHook()) {
			close();
		}
	});
	QObject::connect(ui->maxres, &QToolButton::clicked, [this]() {
		if (!getMaxState() && !getFullScreenState()) {
			showMaximized();
		} else {
			showNormal();
		}
		pls_flush_style(ui->maxres);
	});
	connect(ui->min, &QToolButton::clicked, [this]() { showMinimized(); });

#endif

	this->ui->helpBtn->setVisible(hasHelpButton);
	ui->helpBtn->installEventFilter(this);
	setCustomChecker(ui->content);

	connect(this, &PLSWindow::windowTitleChanged, this, &PLSWindow::SetNameLabelText, Qt::QueuedConnection);
	connect(ui->content, &QWidget::windowTitleChanged, this, &PLSWindow::setWindowTitle);

	ui->content->setObjectName(QString());
	connect(ui->content, &QObject::objectNameChanged, [this](const QString &objectName) {
		if (objectName != CONTENT) {
			setObjectName(objectName);
			ui->content->setObjectName(CONTENT);
		}
	});
}

PLSWindow::~PLSWindow()
{
	pls_delete(ui, nullptr);
}

QWidget *PLSWindow::content() const
{
	return ui->content;
}

QWidget *PLSWindow::widget() const
{
	return owidget;
}

void PLSWindow::setWidget(QWidget *widget)
{
	this->owidget = widget;
	widget->setParent(ui->content);
	connect(widget, &QWidget::windowTitleChanged, this, &PLSWindow::setWindowTitle);
	setWindowTitle(widget->windowTitle());

	QHBoxLayout *l = pls_new<QHBoxLayout>(ui->content);
	l->setContentsMargins(0, 0, 0, 0);
	l->setSpacing(0);
	l->addWidget(widget);
}

int PLSWindow::getCaptionHeight() const
{
	return captionHeight;
}

void PLSWindow::setCaptionHeight(int captionHeight_)
{
	this->captionHeight = captionHeight_;
	ui->titleBar->setFixedHeight(this->captionHeight);
}

int PLSWindow::getCaptionButtonSize() const
{
	return captionButtonSize;
}

void PLSWindow::setCaptionButtonSize(int captionButtonSize_)
{
	this->captionButtonSize = captionButtonSize_;
	ui->min->setFixedSize(captionButtonSize, captionButtonSize);
	ui->maxres->setFixedSize(captionButtonSize, captionButtonSize);
	ui->close->setFixedSize(captionButtonSize, captionButtonSize);
}

int PLSWindow::getCaptionButtonMargin() const
{
	return captionButtonMargin;
}

void PLSWindow::setCaptionButtonMargin(int captionButtonMargin_)
{
	this->captionButtonMargin = captionButtonMargin_;
	ui->titleButtonLayout->setSpacing(captionButtonMargin);
}

int PLSWindow::getTextMarginLeft() const
{
	return textMarginLeft;
}

void PLSWindow::setTextMarginLeft(int textMarginLeft_)
{
	this->textMarginLeft = textMarginLeft_;
	QMargins margins = ui->titleBarLayout->contentsMargins();
	margins.setLeft(textMarginLeft);
	ui->titleBarLayout->setContentsMargins(margins);
}

int PLSWindow::getCloseButtonMarginRight() const
{
	return closeButtonMarginRight;
}

void PLSWindow::setCloseButtonMarginRight(int closeButtonMarginRight_)
{
	this->closeButtonMarginRight = closeButtonMarginRight_;
	QMargins margins = ui->titleBarLayout->contentsMargins();
	margins.setRight(closeButtonMarginRight);
	ui->titleBarLayout->setContentsMargins(margins);
}

bool PLSWindow::getHasCaption() const
{
	return hasCaption;
}

void PLSWindow::setHasCaption(bool hasCaption_)
{
	this->hasCaption = hasCaption_;
#if defined(Q_OS_WIN)
	ui->close->setVisible(hasCaption);
	ui->min->setVisible(hasCaption && hasMinButton);
	ui->titleBar->setVisible(hasCaption);
#elif defined(Q_OS_MACOS)
	if (!hasCaption_) {
		setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
	} else {
		setWindowFlags(windowFlags() & ~Qt::FramelessWindowHint);
	}
#endif
}

bool PLSWindow::getHasHLine() const
{
	return hasHLine;
}

void PLSWindow::setHasHLine(bool hasHLine_)
{
	this->hasHLine = hasHLine_;
	ui->hline->setVisible(hasHLine);
}

bool PLSWindow::getHasMinButton() const
{
	return hasMinButton;
}

bool PLSWindow::getHasMaxResButton() const
{
	return hasMaxResButton;
}

void PLSWindow::setHasMaxResButton(bool hasMaxResButton_)
{
	this->hasMaxResButton = hasMaxResButton_;
#if defined(Q_OS_WIN)
	ui->maxres->setVisible(hasMaxResButton);
#elif defined(Q_OS_MACOS)
	customMacWindow()->setMaxButtonHidden(!hasMaxResButton_);
#endif
}

void PLSWindow::setHasMinButton(bool hasMinButton_)
{
	this->hasMinButton = hasMinButton_;
#if defined(Q_OS_WIN)
	ui->min->setVisible(hasMinButton);
#elif defined(Q_OS_MACOS)
	customMacWindow()->setMinButtonHidden(!hasMinButton_);
#endif
}

bool PLSWindow::getHasCloseButton() const
{
	return hasCloseButton;
}

void PLSWindow::setHasCloseButton(bool hasCloseButton_)
{
	this->hasCloseButton = hasCloseButton_;
#if defined(Q_OS_WIN)
	ui->close->setVisible(hasCloseButton);
#elif defined(Q_OS_MACOS)
	customMacWindow()->setCloseButtonHidden(!hasCloseButton_);
#endif
}

bool PLSWindow::getHasHelpButton() const
{
	return hasHelpButton;
}

void PLSWindow::setHasHelpButton(bool hasHelpButton_)
{
	hasHelpButton = hasHelpButton_;
	ui->helpBtn->setVisible(hasHelpButton);
}

QColor PLSWindow::getTitleBarBkgColor() const
{
	return m_bkgColor;
}

void PLSWindow::setTitleBarBkgColor(QColor bkgColor)
{
	m_bkgColor = bkgColor;
}

void PLSWindow::setHelpButtonToolTip(QString tooltip, int y)
{
	helpTooltip = tooltip;
	helpTooltipY = y;
}

bool PLSWindow::getEscapeCloseEnabled() const
{
	return isEscapeCloseEnabled;
}

void PLSWindow::setEscapeCloseEnabled(bool enabled)
{
	isEscapeCloseEnabled = enabled;
}

int PLSWindow::getCaptionButtonTopMargin() const
{
	return ui->titleButtonLayout->contentsMargins().top();
}

void PLSWindow::setCaptionButtonTopMargin(int captionButtonTopMargin)
{
	QMargins margins = ui->titleButtonLayout->contentsMargins();
	margins.setTop(captionButtonTopMargin);
	ui->titleButtonLayout->setContentsMargins(margins);
}

QWidget *PLSWindow::titleLabel() const
{
	return ui->titleLabel;
}

int PLSWindow::setTitleCustomWidth(int width)
{
	return titleCustomWidth = width;
}

void PLSWindow::setCloseEventCallback(const std::function<bool(QCloseEvent *)> &closeEventCallback_)
{
	this->closeEventCallback = closeEventCallback_;
}

void PLSWindow::callBaseCloseEvent(QCloseEvent *event)
{
	if (event) {
		PLSToplevelView<QWidget>::closeEvent(event);
	}
}

void PLSWindow::sizeToContent(const QSize &size)
{
	QSize newSize = ui->content->layout()->minimumSize();
	if (size.isValid()) {
		newSize.setWidth(qMax(size.width(), newSize.width()));
		newSize.setHeight(qMax(size.height(), newSize.height()));
	}

	if (hasCaption) {
		if (hasCaption) {
			newSize.setHeight(newSize.height() + captionHeight);
		}
	}

	resize(newSize);
}

void PLSWindow::setHeightForFixedWidth()
{
	int fixedWidth = minimumWidth();
	int height = layout()->minimumHeightForWidth(fixedWidth);
	setFixedHeight(height);
}

static inline QPoint operator+(const QPoint &pt, const QSize &sz)
{
	return QPoint(pt.x() + sz.width(), pt.y() + sz.height());
}

int PLSWindow::titleBarHeight() const
{
	return hasCaption ? ui->titleBar->height() : 0;
}

bool PLSWindow::canMaximized() const
{
	return hasCaption && hasMaxResButton;
}

bool PLSWindow::canFullScreen() const
{
	return false;
}

bool PLSWindow::getMaxState() const
{
	return windowState().testFlag(Qt::WindowMaximized);
}
bool PLSWindow::getFullScreenState() const
{
	return windowState().testFlag(Qt::WindowFullScreen);
}

void PLSWindow::setHiddenWidget(QWidget *widget)
{
	widget->hide();
}

void PLSWindow::setNotRetainSizeWhenHidden(QWidget *widget)
{
	QSizePolicy policy = widget->sizePolicy();
	policy.setRetainSizeWhenHidden(false);
	widget->setSizePolicy(policy);
}

void PLSWindow::addMacTopMargin()
{
#if defined(Q_OS_MACOS)
	QMargins margin = ui->mainLayout->contentsMargins();
	margin.setTop(margin.top() + 20);
	ui->mainLayout->setContentsMargins(margin);
#endif
}

void PLSWindow::flushMaxFullScreenStateStyle()
{
	pls_flush_style(ui->maxres);
}

int PLSWindow::getVisibleButtonWidth(QWidget *widget)
{
	if (!widget || !widget->isVisible()) {
		return 0;
	}

	return widget->width();
}

void PLSWindow::closeEvent(QCloseEvent *event)
{
	closeEventCallback(event);
}

void PLSWindow::showEvent(QShowEvent *event)
{
	PLSToplevelView<QWidget>::showEvent(event);
	disableWinSystemBorder();
	emit shown();
}

void PLSWindow::hideEvent(QHideEvent *event)
{
	PLSToplevelView<QWidget>::hideEvent(event);
	pls_check_app_exiting();

	QWidget *parent = this->parentWidget();
	if (parent) {
		parent->activateWindow();
		return;
	}

	if (auto mainView = pls_get_main_view(); mainView) {
		mainView->activateWindow();
	}
}

void PLSWindow::keyPressEvent(QKeyEvent *event)
{
	if (event->key() != Qt::Key_Escape) {
		PLSToplevelView<QWidget>::keyPressEvent(event);
	} else if (isEscapeCloseEnabled) {
		event->accept();
		close();
	} else {
		event->ignore();
	}
}

void PLSWindow::keyReleaseEvent(QKeyEvent *event)
{
	if (event->key() != Qt::Key_Escape) {
		PLSToplevelView<QWidget>::keyReleaseEvent(event);
	} else {
		event->ignore();
	}
}

bool PLSWindow::eventFilter(QObject *watcher, QEvent *event)
{
	if (watcher == ui->content) {
		switch (event->type()) {
		case QEvent::ChildAdded:
			if (auto widget = dynamic_cast<QWidget *>(static_cast<QChildEvent *>(event)->child()); widget) {
				widget->setCursor(Qt::ArrowCursor);
			}
			break;
		default:
			break;
		}
	}

	if (watcher == this && event->type() == QEvent::WindowActivate && !pls_is_visible_in_some_screen(this->geometry())) {
		restoreGeometry(saveGeometry());
	}

	if ((watcher == ui->min) || (watcher == ui->maxres) || (watcher == ui->close)) {
		switch (event->type()) {
		case QEvent::KeyPress:
		case QEvent::KeyRelease:
			return true;
		default:
			break;
		}
	}

	if (watcher == ui->helpBtn) {
		if (event->type() == QMouseEvent::Enter) {
			QPoint pos = this->rect().topLeft();
			QPoint global = mapToGlobal(pos);
			QPoint x = mapToParent(ui->helpBtn->pos());
			int helpBtnLeftMargin = 1;
			QToolTip::showText(QPoint(x.x() - helpBtnLeftMargin, global.y() + helpTooltipY), helpTooltip, this);
		}
	}

	return PLSToplevelView<QWidget>::eventFilter(watcher, event);
}

bool PLSWindow::event(QEvent *event)
{
	switch (event->type()) {
	case QEvent::Resize:
		SetNameLabelText(windowTitle());
		break;
	case QEvent::Move: {
		int aa = 1;
		break;
	}
	default:
		break;
	}
	return PLSToplevelView<QWidget>::event(event);
}

void PLSWindow::windowStateChanged(QWindowStateChangeEvent *event)
{
	PLSToplevelView<QWidget>::windowStateChanged(event);
	pls_flush_style(ui->maxres);
}

void PLSWindow::SetNameLabelText(const QString &title)
{
	if (title.isEmpty()) {
		ui->titleLabel->setFixedWidth(titleCustomWidth);
		ui->titleLabel->setText(title);
		return;
	}
	int availWidth = this->width() - ui->titleBarLayout->spacing() - ui->titleButtonLayout->spacing() - getVisibleButtonWidth(ui->helpBtn) - getVisibleButtonWidth(ui->close) -
			 getVisibleButtonWidth(ui->min) - getVisibleButtonWidth(ui->maxres) - textMarginLeft;

	QFontMetrics font(ui->titleLabel->font());
	int realWidth = font.horizontalAdvance(title);

	if (realWidth > availWidth) {
		ui->titleLabel->setFixedWidth(availWidth);
		ui->titleLabel->setText(font.elidedText(title, Qt::ElideRight, availWidth));

	} else {
		ui->titleLabel->setFixedWidth(realWidth);
		ui->titleLabel->setText(title);
	}
}
