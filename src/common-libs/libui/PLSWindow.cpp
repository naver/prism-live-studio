#include "PLSWindow.h"
#include "ui_PLSWindow.h"
#include "PLSTrackers.h"

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

PLSWindow::PLSWindow(QWidget *parent, Qt::WindowFlags f, CreateWinId createWinId) : PLSToplevelView<QWidget>(createWinId, parent, f)
{
	PLS_DISABLE_UISTEP_V2(this);
	ui = pls_new<Ui::PLSWindow>();

	ui->setupUi(this);
#if defined(Q_OS_WIN)
	ui->titleBar->installEventFilter(this);
	ui->content->setAttribute(Qt::WA_NativeWindow);
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
	if (!isAfterWin10()) {
		ui->mainLayout->setContentsMargins(1, 1, 1, 1);
	}

	this->ui->min->setVisible(hasMinButton);
	this->ui->maxres->setVisible(hasMaxResButton);

	connect(ui->close, &QToolButton::clicked, [this]() {
		if (closeButtonHook()) {
			close();
		}
	});
	QObject::connect(ui->maxres, &QToolButton::clicked, [this]() {
		resizeTracker()->disableTracking();
		if (!getMaxState() && !getFullScreenState()) {
			showMaximized();
		} else {
			showNormal();
		}
		pls_flush_style(ui->maxres);
	});
	connect(ui->min, &QToolButton::clicked, [this]() {
		resizeTracker()->disableTracking();
		showMinimized();
	});

#endif

	this->ui->helpBtn->setVisible(hasHelpButton);
	ui->helpBtn->installEventFilter(this);
	ui->helpBtn->setProperty("ignoreHideToolTip", true);
	setMoveExcludeChecker(ui->titleBar);

	connect(this, &PLSWindow::windowTitleChanged, this, [this]() { updateTitleBarLayout(ui->titleBar->size()); });
	connect(ui->content, &QWidget::windowTitleChanged, this, &PLSWindow::setWindowTitle);

	ui->content->setObjectName(QString());
	connect(ui->content, &QObject::objectNameChanged, [this](const QString &objectName) {
		if (objectName != CONTENT) {
			setObjectName(objectName);
			ui->content->setObjectName(CONTENT);
		}
	});

	pls_uistep_v2_set_title(this, [this]() { return this->windowTitle().toUtf8(); });
	pls_uistep_v2_set_name(ui->helpBtn, QStringLiteral("TitleBar Button"));
	pls_uistep_v2_set_custom_enter_leave_name(ui->helpBtn, QByteArrayLiteral("TitleBar Button Help"));
	pls_uistep_v2_set_info(ui->min, QStringLiteral("TitleBar Button"), QStringLiteral("Minimize"));
	pls_uistep_v2_set_info(ui->maxres, QStringLiteral("TitleBar Button"), [this]() { return isMaximized() ? QStringLiteral("Restore") : QStringLiteral("Maximize"); });
	pls_uistep_v2_set_info(ui->close, QStringLiteral("TitleBar Button"), QStringLiteral("Close"));
}

PLSWindow::~PLSWindow()
{
	pls_delete(ui, nullptr);
}

QWidget *PLSWindow::titleBar() const
{
	return ui->titleBar;
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
	if (this->owidget != nullptr) {
		ui->content->layout()->removeWidget(this->owidget);
		this->owidget->setParent(nullptr);
		pls_delete(this->owidget, nullptr);
	}
	this->owidget = widget;
	widget->setParent(ui->content);
	connect(widget, &QWidget::windowTitleChanged, this, &PLSWindow::setWindowTitle);
	setWindowTitle(widget->windowTitle());

	auto layout = ui->content->layout();
	if (!layout) {
		layout = pls_new<QHBoxLayout>(ui->content);
		layout->setContentsMargins(0, 0, 0, 0);
		layout->setSpacing(0);
	}
	layout->addWidget(widget);
}

QWidget *PLSWindow::titleWidget() const
{
	return otitleWidget;
}
void PLSWindow::setTitleWidget(QWidget *widget, std::function<void(QWidget *titleWidget, const QSize &size)> &&resizeCb)
{
	if (otitleWidget != nullptr) {
		ui->titleBar->layout()->removeWidget(otitleWidget);
		otitleWidget->setParent(nullptr);
		pls_delete(otitleWidget, nullptr);
	}
	otitleWidget = widget;
	titleWidgetResizeCb = std::move(resizeCb);
	widget->setParent(ui->titleBar);
	if (titleWidgetResizeCb) {
		titleWidgetResizeCb(widget, ui->titleBar->size());
	} else {
		auto layout = ui->titleBar->layout();
		if (!layout) {
			layout = pls_new<QHBoxLayout>(ui->titleBar);
			layout->setContentsMargins(0, 0, 0, 0);
			layout->setSpacing(0);
		}
		layout->addWidget(widget);
	}

	widget->lower();
	widget->setAttribute(Qt::WA_NativeWindow, false);
}

int PLSWindow::getCaptionBarHeight() const
{
	return captionBarHeight;
}
void PLSWindow::setCaptionBarHeight(int captionBarHeight)
{
	this->captionBarHeight = captionBarHeight;
	if (captionBarHeight > 0) {
		ui->titleLabel->setVisible(true);
		ui->helpBtn->setVisible(hasHelpButton);
		ui->min->setVisible(hasMinButton);
		ui->maxres->setVisible(hasMaxResButton);
		ui->close->setVisible(hasCloseButton);
		updateTitleBarLayout(ui->titleBar->size());
	} else {
		ui->titleLabel->setVisible(false);
		ui->helpBtn->setVisible(false);
		ui->min->setVisible(false);
		ui->maxres->setVisible(false);
		ui->close->setVisible(false);
	}
}

int PLSWindow::getCaptionHeight() const
{
	return captionHeight;
}

void PLSWindow::setCaptionHeight(int captionHeight)
{
	this->captionHeight = captionHeight;
	if (captionHeight > 0) {
		ui->titleBar->setFixedHeight(captionHeight);
	} else {
		ui->titleBar->setMinimumHeight(0);
		ui->titleBar->setMaximumHeight(QWIDGETSIZE_MAX);
	}
}

int PLSWindow::getCaptionButtonSize() const
{
	return captionButtonSize;
}

void PLSWindow::setCaptionButtonSize(int captionButtonSize)
{
	this->captionButtonSize = captionButtonSize;
	updateTitleBarLayout(ui->titleBar->size());
}

int PLSWindow::getCaptionButtonMargin() const
{
	return captionButtonMargin;
}

void PLSWindow::setCaptionButtonMargin(int captionButtonMargin)
{
	this->captionButtonMargin = captionButtonMargin;
}

int PLSWindow::getTextMarginLeft() const
{
	return textMarginLeft;
}

void PLSWindow::setTextMarginLeft(int textMarginLeft)
{
	this->textMarginLeft = textMarginLeft;
	updateTitleBarLayout(ui->titleBar->size());
}

int PLSWindow::getTextMarginRight() const
{
	return textMarginRight;
}
void PLSWindow::setTextMarginRight(int textMarginRight)
{
	this->textMarginRight = textMarginRight;
	updateTitleBarLayout(ui->titleBar->size());
}

int PLSWindow::getCloseButtonMarginRight() const
{
	return closeButtonMarginRight;
}

void PLSWindow::setCloseButtonMarginRight(int closeButtonMarginRight)
{
	this->closeButtonMarginRight = closeButtonMarginRight;
	updateTitleBarLayout(ui->titleBar->size());
}

bool PLSWindow::getHasCaption() const
{
	return hasCaption;
}

void PLSWindow::setHasCaption(bool hasCaption)
{
	this->hasCaption = hasCaption;

#if defined(Q_OS_WIN)
	ui->titleBar->setVisible(hasCaption);
#elif defined(Q_OS_MACOS)
	if (!hasCaption) {
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

void PLSWindow::setHasHLine(bool hasHLine)
{
	this->hasHLine = hasHLine;
	ui->hline->setVisible(hasHLine);
}

bool PLSWindow::getHasMinButton() const
{
	return hasMinButton;
}
void PLSWindow::setHasMinButton(bool hasMinButton)
{
	this->hasMinButton = hasMinButton;
#if defined(Q_OS_WIN)
	ui->min->setVisible(hasMinButton);
#elif defined(Q_OS_MACOS)
	customMacWindow()->setMinButtonHidden(!hasMinButton);
#endif
	updateTitleBarLayout(ui->titleBar->size());
}

bool PLSWindow::getHasMaxResButton() const
{
	return hasMaxResButton;
}
void PLSWindow::setHasMaxResButton(bool hasMaxResButton)
{
	this->hasMaxResButton = hasMaxResButton;
#if defined(Q_OS_WIN)
	ui->maxres->setVisible(hasMaxResButton);
#elif defined(Q_OS_MACOS)
	customMacWindow()->setMaxButtonHidden(!hasMaxResButton);
#endif
	updateTitleBarLayout(ui->titleBar->size());
}

bool PLSWindow::getHasCloseButton() const
{
	return hasCloseButton;
}
void PLSWindow::setHasCloseButton(bool hasCloseButton)
{
	this->hasCloseButton = hasCloseButton;
#if defined(Q_OS_WIN)
	ui->close->setVisible(hasCloseButton);
#elif defined(Q_OS_MACOS)
	customMacWindow()->setCloseButtonHidden(!hasCloseButton);
#endif
	updateTitleBarLayout(ui->titleBar->size());
}

bool PLSWindow::getHasHelpButton() const
{
	return hasHelpButton;
}

void PLSWindow::setHasHelpButton(bool hasHelpButton)
{
	this->hasHelpButton = hasHelpButton;
	ui->helpBtn->setVisible(hasHelpButton);
	updateTitleBarLayout(ui->titleBar->size());
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
	return captionButtonTopMargin;
}

void PLSWindow::setCaptionButtonTopMargin(int captionButtonTopMargin)
{
	this->captionButtonTopMargin = captionButtonTopMargin;
	updateTitleBarLayout(ui->titleBar->size());
}

QWidget *PLSWindow::titleLabel() const
{
	return ui->titleLabel;
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

void PLSWindow::updateTitleBarLayout(const QSize &titleBarSize)
{
#if defined(Q_OS_WIN)
	if (!hasCaption)
		return;

	if (captionBarHeight > 0) {
		int buttonLeft = titleBarSize.width() - closeButtonMarginRight;
		int buttonTop = captionButtonTopMargin;
		if (hasCloseButton) {
			buttonLeft -= captionButtonSize;
			ui->close->setGeometry(buttonLeft, buttonTop, captionButtonSize, captionButtonSize);
		}

		if (hasMaxResButton) {
			if (hasCloseButton)
				buttonLeft -= captionButtonMargin;
			buttonLeft -= captionButtonSize;
			ui->maxres->setGeometry(buttonLeft, buttonTop, captionButtonSize, captionButtonSize);
		}

		if (hasMinButton) {
			if (hasMaxResButton || hasCloseButton)
				buttonLeft -= captionButtonMargin;
			buttonLeft -= captionButtonSize;
			ui->min->setGeometry(buttonLeft, buttonTop, captionButtonSize, captionButtonSize);
		}

		int titleWidth = 0;

		auto title = windowTitle();
		int availWidth = titleBarSize.width() - textMarginLeft - textMarginRight - getVisibleButtonWidth(ui->helpBtn) - (titleBarSize.width() - buttonLeft);
		QFontMetrics fm = ui->titleLabel->fontMetrics();
		if (int realWidth = fm.horizontalAdvance(title); realWidth > availWidth) {
			ui->titleLabel->setText(fm.elidedText(title, Qt::ElideRight, availWidth));
			titleWidth = availWidth;
		} else {
			ui->titleLabel->setText(title);
			titleWidth = realWidth;
		}

		auto captionBarHeight = getCaptionBarHeight();
		titleWidth = getTitleWidth(titleWidth);
		ui->titleLabel->setFixedWidth(titleWidth);
		ui->titleLabel->setGeometry(textMarginLeft, 0, titleWidth, captionBarHeight);

		if (hasHelpButton) {
			auto helpSize = ui->helpBtn->size();
			auto helpButtonLeft = textMarginLeft + titleWidth + textMarginRight;
			auto helpButtonTop = (captionBarHeight - helpSize.height()) / 2;
			ui->helpBtn->move(helpButtonLeft, helpButtonTop);
		}
	}

	if (otitleWidget && titleWidgetResizeCb) {
		titleWidgetResizeCb(otitleWidget, titleBarSize);
	}
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
	PLS_UI_ACTION("PLSWindow %s Show", this->metaObject()->className());
	disableWinSystemBorder();
	updateTitleBarLayout(ui->titleBar->size());
	emit shown();
}

void PLSWindow::hideEvent(QHideEvent *event)
{
	PLSToplevelView<QWidget>::hideEvent(event);
	pls_check_app_exiting();
	PLS_UI_ACTION("PLSWindow %s Hide", this->metaObject()->className());

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

	else if (watcher == this && event->type() == QEvent::WindowActivate && !pls_is_visible_in_some_screen(this->geometry())) {
		restoreGeometry(saveGeometry());
	}

	else if ((watcher == ui->min) || (watcher == ui->maxres) || (watcher == ui->close)) {
		switch (event->type()) {
		case QEvent::KeyPress:
		case QEvent::KeyRelease:
			return true;
		default:
			break;
		}
	}

	else if (watcher == ui->helpBtn) {
		if (event->type() == QMouseEvent::Enter) {
			QPoint pos = this->rect().topLeft();
			QPoint global = mapToGlobal(pos);
			QPoint x = mapToParent(ui->helpBtn->pos());
			int helpBtnLeftMargin = 1;
			QToolTip::showText(QPoint(x.x() - helpBtnLeftMargin, global.y() + helpTooltipY), helpTooltip, this);
		}
	}

	else if (watcher == ui->titleBar) {
		if (event->type() == QEvent::Resize) {
			updateTitleBarLayout(static_cast<QResizeEvent *>(event)->size());
		}
	}

	return PLSToplevelView<QWidget>::eventFilter(watcher, event);
}

void PLSWindow::windowStateChanged(QWindowStateChangeEvent *event)
{
	PLSToplevelView<QWidget>::windowStateChanged(event);
	pls_flush_style(ui->maxres);
}

void PLSWindow::resizeEvent(QResizeEvent *event)
{
	PLSToplevelView<QWidget>::resizeEvent(event);
	updateTitleBarLayout(ui->titleBar->size());
	if (event && stabilizeFixedSizeOnResize()) {
		return;
	}
	resizing(event->size());
}

void PLSWindow::paintEvent(QPaintEvent *event)
{
	PLSToplevelView<QWidget>::paintEvent(event);

#if defined(Q_OS_WIN)
	if (!isAfterWin10()) {
		QPainter painter(this);
		painter.setPen(QPen(QColor(17, 17, 17), 1, Qt::SolidLine));
		painter.setBrush(Qt::transparent);
		painter.drawRect(rect());
	}
#endif
}
