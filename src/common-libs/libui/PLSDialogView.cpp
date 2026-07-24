#include "PLSDialogView.h"
#include "ui_PLSDialogView.h"
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
#include <qdialogbuttonbox.h>
#include <qcheckbox.h>
#include <qcombobox.h>

#include <liblog.h>
#include <libutils-api.h>
#include <QToolTip>

#pragma comment(lib, "Shcore.lib")

constexpr const char *TOPLEVELVIEW_MODULE = "ToplevelView";

const QString CONTENT = QStringLiteral("content");

PLSDialogView::PLSDialogView(QWidget *parent, Qt::WindowFlags f, CreateWinId createWinId) : PLSToplevelView<QDialog>(createWinId, parent, f)
{
	PLS_DISABLE_UISTEP_V2(this);
	ui = pls_new<Ui::PLSDialogView>();

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
			reject();
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

	connect(this, &PLSDialogView::windowTitleChanged, this, [this]() { updateTitleBarLayout(ui->titleBar->size()); });
	connect(ui->content, &QWidget::windowTitleChanged, this, &PLSDialogView::setWindowTitle);

	ui->content->setObjectName(QString());
	connect(ui->content, &QObject::objectNameChanged, [this](const QString &objectName) {
		if (objectName != CONTENT) {
			setObjectName(objectName);
			ui->content->setObjectName(CONTENT);
		}
	});

	pls_uistep_v2_set_title(this, [this]() { return this->windowTitle(); });
	pls_uistep_v2_set_name(ui->helpBtn, QStringLiteral("TitleBar Button"));
	pls_uistep_v2_set_custom_enter_leave_name(ui->helpBtn, QByteArrayLiteral("TitleBar Button Help"));
	pls_uistep_v2_set_info(ui->min, QStringLiteral("TitleBar Button"), QStringLiteral("Minimize"));
	pls_uistep_v2_set_info(ui->maxres, QStringLiteral("TitleBar Button"), [this]() { return isMaximized() ? QStringLiteral("Restore") : QStringLiteral("Maximize"); });
	pls_uistep_v2_set_info(ui->close, QStringLiteral("TitleBar Button"), QStringLiteral("Close"));
}

PLSDialogView::PLSDialogView(DialogInfo info, QWidget *parent, CreateWinId createWinId) : PLSDialogView(parent, Qt::WindowFlags(), createWinId)
{
	defaultInfo = info;
}
PLSDialogView::~PLSDialogView()
{
	pls_delete(ui, nullptr);
}

QWidget *PLSDialogView::titleBar() const
{
	return ui->titleBar;
}

QToolButton *PLSDialogView::closeButton() const
{
	return ui->close;
}

int PLSDialogView::exec()
{
	QPointer<QDialog> dialog(this);
	QPointer<QWidget> oldParent = parentWidget();
	QPointer<QWidget> newParent = parentWidget();

	if (!oldParent && !(this->objectName().contains("PLSLoginMainView") || this->objectName().contains("PLSUpdateTipView") || this->objectName().contains("noticeView"))) {
		newParent = pls_get_main_view();
	}

	if (pls_has_modal_view()) {
		if (auto modelView = pls_get_last_modal_view(); modelView != newParent) {
			newParent = modelView;
		}
	}

	if (!m_isUseOriginalParent && newParent && oldParent != newParent) {
#if defined(Q_OS_WIN)
		QMetaObject::invokeMethod(newParent, &QWidget::activateWindow, Qt::QueuedConnection);
#endif
		auto flags = windowFlags();
		if (isAlwaysOnTop(newParent.data()))
			flags.setFlag(Qt::WindowStaysOnTopHint);
		this->setParent(newParent, flags);
	}
	pls::HotKeyLocker locker;
	pls_push_modal_view(dialog);
	int result = PLSToplevelView<QDialog>::exec();
	pls_pop_modal_view(dialog);

	return result;
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
	if (this->owidget != nullptr) {
		ui->content->layout()->removeWidget(this->owidget);
		this->owidget->setParent(nullptr);
		pls_delete(this->owidget, nullptr);
	}
	this->owidget = widget;
	widget->setParent(ui->content);
	connect(widget, &QWidget::windowTitleChanged, this, &PLSDialogView::setWindowTitle);
	setWindowTitle(widget->windowTitle());

	auto layout = ui->content->layout();
	if (!layout) {
		layout = pls_new<QHBoxLayout>(ui->content);
		layout->setContentsMargins(0, 0, 0, 0);
		layout->setSpacing(0);
	}
	layout->addWidget(widget);
}

QWidget *PLSDialogView::titleWidget() const
{
	return otitleWidget;
}
void PLSDialogView::setTitleWidget(QWidget *widget, std::function<void(QWidget *titleWidget, const QSize &size)> &&resizeCb)
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

int PLSDialogView::getCaptionBarHeight() const
{
	return captionBarHeight;
}
void PLSDialogView::setCaptionBarHeight(int captionBarHeight)
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

int PLSDialogView::getCaptionHeight() const
{
	return captionHeight;
}
void PLSDialogView::setCaptionHeight(int captionHeight)
{
	this->captionHeight = captionHeight;
	if (captionHeight > 0) {
		ui->titleBar->setFixedHeight(captionHeight);
	} else {
		ui->titleBar->setMinimumHeight(0);
		ui->titleBar->setMaximumHeight(QWIDGETSIZE_MAX);
	}
}

int PLSDialogView::getCaptionButtonSize() const
{
	return captionButtonSize;
}
void PLSDialogView::setCaptionButtonSize(int captionButtonSize)
{
	this->captionButtonSize = captionButtonSize;
	updateTitleBarLayout(ui->titleBar->size());
}

int PLSDialogView::getCaptionButtonMargin() const
{
	return captionButtonMargin;
}

void PLSDialogView::setCaptionButtonMargin(int captionButtonMargin)
{
	this->captionButtonMargin = captionButtonMargin;
}

int PLSDialogView::getTextMarginLeft() const
{
	return textMarginLeft;
}

void PLSDialogView::setTextMarginLeft(int textMarginLeft)
{
	this->textMarginLeft = textMarginLeft;
	updateTitleBarLayout(ui->titleBar->size());
}

int PLSDialogView::getTextMarginRight() const
{
	return textMarginRight;
}
void PLSDialogView::setTextMarginRight(int textMarginRight)
{
	this->textMarginRight = textMarginRight;
	updateTitleBarLayout(ui->titleBar->size());
}

int PLSDialogView::getCloseButtonMarginRight() const
{
	return closeButtonMarginRight;
}

void PLSDialogView::setCloseButtonMarginRight(int closeButtonMarginRight)
{
	this->closeButtonMarginRight = closeButtonMarginRight;
	updateTitleBarLayout(ui->titleBar->size());
}

bool PLSDialogView::getHasCaption() const
{
	return hasCaption;
}

void PLSDialogView::setHasCaption(bool hasCaption)
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

bool PLSDialogView::getHasHLine() const
{
	return hasHLine;
}

void PLSDialogView::setHasHLine(bool hasHLine)
{
	this->hasHLine = hasHLine;
	ui->hline->setVisible(hasHLine);
}

bool PLSDialogView::getHasMinButton() const
{
	return hasMinButton;
}
void PLSDialogView::setHasMinButton(bool hasMinButton)
{
	this->hasMinButton = hasMinButton;
#if defined(Q_OS_WIN)
	ui->min->setVisible(hasMinButton);
#elif defined(Q_OS_MACOS)
	customMacWindow()->setMinButtonHidden(!hasMinButton);
#endif
	updateTitleBarLayout(ui->titleBar->size());
}

bool PLSDialogView::getHasMaxResButton() const
{
	return hasMaxResButton;
}
void PLSDialogView::setHasMaxResButton(bool hasMaxResButton)
{
	this->hasMaxResButton = hasMaxResButton;
#if defined(Q_OS_WIN)
	ui->maxres->setVisible(hasMaxResButton);
#elif defined(Q_OS_MACOS)
	customMacWindow()->setMaxButtonHidden(!hasMaxResButton);
#endif
	updateTitleBarLayout(ui->titleBar->size());
}

bool PLSDialogView::getHasCloseButton() const
{
	return hasCloseButton;
}
void PLSDialogView::setHasCloseButton(bool hasCloseButton)
{
	this->hasCloseButton = hasCloseButton;
#if defined(Q_OS_WIN)
	ui->close->setVisible(hasCloseButton);
#elif defined(Q_OS_MACOS)
	customMacWindow()->setCloseButtonHidden(!hasCloseButton);
#endif
	updateTitleBarLayout(ui->titleBar->size());
}

bool PLSDialogView::getHasHelpButton() const
{
	return hasHelpButton;
}
void PLSDialogView::setHasHelpButton(bool hasHelpButton)
{
	this->hasHelpButton = hasHelpButton;
	ui->helpBtn->setVisible(hasHelpButton);
	updateTitleBarLayout(ui->titleBar->size());
}

QColor PLSDialogView::getTitleBarBkgColor() const
{
	return m_bkgColor;
}

void PLSDialogView::setTitleBarBkgColor(QColor bkgColor)
{
	m_bkgColor = bkgColor;
}

void PLSDialogView::setHelpButtonToolTip(QString tooltip, int y)
{
	helpTooltip = tooltip;
	helpTooltipY = y;
}

bool PLSDialogView::getEscapeCloseEnabled() const
{
	return isEscapeCloseEnabled;
}

void PLSDialogView::setEscapeCloseEnabled(bool enabled)
{
	isEscapeCloseEnabled = enabled;
}

int PLSDialogView::getCaptionButtonTopMargin() const
{
	return captionButtonTopMargin;
}
void PLSDialogView::setCaptionButtonTopMargin(int captionButtonTopMargin)
{
	this->captionButtonTopMargin = captionButtonTopMargin;
	updateTitleBarLayout(ui->titleBar->size());
}

QWidget *PLSDialogView::titleLabel() const
{
	return ui->titleLabel;
}

void PLSDialogView::setCloseEventCallback(const std::function<bool(QCloseEvent *)> &closeEventCallback_)
{
	this->closeEventCallback = closeEventCallback_;
}

void PLSDialogView::callBaseCloseEvent(QCloseEvent *event)
{
	if (event) {
		PLSToplevelView<QDialog>::closeEvent(event);
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
		newSize.setHeight(newSize.height() + captionHeight);
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

void PLSDialogView::done(int result)
{
	if (closeEventCallback(nullptr)) {
		PLSToplevelView<QDialog>::done(result);
	}
}

int PLSDialogView::titleBarHeight() const
{
	return hasCaption ? ui->titleBar->height() : 0;
}

bool PLSDialogView::canMaximized() const
{
	return hasCaption && hasMaxResButton;
}

bool PLSDialogView::canFullScreen() const
{
	return false;
}

bool PLSDialogView::getMaxState() const
{
	return windowState().testFlag(Qt::WindowMaximized);
}
bool PLSDialogView::getFullScreenState() const
{
	return windowState().testFlag(Qt::WindowFullScreen);
}
void PLSDialogView::closeNoButton()
{
	PLS_UI_STEP("PLSDialogView", "auto close dialog without button clicked", "");
	done(QDialogButtonBox::NoButton);
}

void PLSDialogView::setHiddenWidget(QWidget *widget)
{
	widget->hide();
}

void PLSDialogView::setNotRetainSizeWhenHidden(QWidget *widget)
{
	QSizePolicy policy = widget->sizePolicy();
	policy.setRetainSizeWhenHidden(false);
	widget->setSizePolicy(policy);
}

void PLSDialogView::addMacTopMargin(int defaultHeight)
{
#if defined(Q_OS_MACOS)
	QMargins margin = ui->mainLayout->contentsMargins();
	margin.setTop(margin.top() + defaultHeight);
	ui->mainLayout->setContentsMargins(margin);
#endif
}

void PLSDialogView::updateTitleBarLayout(const QSize &titleBarSize)
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

void PLSDialogView::flushMaxFullScreenStateStyle()
{
	pls_flush_style(ui->maxres);
}

int PLSDialogView::getVisibleButtonWidth(QWidget *widget)
{
	if (!widget || !widget->isVisible()) {
		return 0;
	}

	return widget->width();
}

void PLSDialogView::closeEvent(QCloseEvent *event)
{
	closeEventCallback(event);
}

void PLSDialogView::showEvent(QShowEvent *event)
{
	PLSToplevelView<QDialog>::showEvent(event);
#if defined(PLS_UI_ACTION_STATS)
	auto className = this->metaObject()->className();
	if (pls_is_equal(className, "PLSDialogView")) {
		PLS_UI_ACTION("PLSDialogView %s Show", this->objectName().toUtf8().constData());
	}
	PLS_UI_ACTION("PLSDialogView %s Show", className);
#endif
	disableWinSystemBorder();
	updateTitleBarLayout(ui->titleBar->size());
	emit shown();
}

void PLSDialogView::hideEvent(QHideEvent *event)
{
	PLSToplevelView<QDialog>::hideEvent(event);
	pls_check_app_exiting();
#if defined(PLS_UI_ACTION_STATS)
	auto className = this->metaObject()->className();
	if (pls_is_equal(className, "PLSDialogView")) {
		PLS_UI_ACTION("PLSDialogView %s Hide", this->objectName().toUtf8().constData());
	}
	PLS_UI_ACTION("PLSDialogView %s Hide", this->metaObject()->className());
#endif
	if (QWidget *parent = pls_get_toplevel_view(this->parentWidget(), nullptr); parent) {
		parent->activateWindow();
	}
}

void PLSDialogView::keyPressEvent(QKeyEvent *event)
{
	if (event->key() != Qt::Key_Escape) {
		PLSToplevelView<QDialog>::keyPressEvent(event);
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
		PLSToplevelView<QDialog>::keyReleaseEvent(event);
	} else {
		event->ignore();
	}
}

bool PLSDialogView::eventFilter(QObject *watcher, QEvent *event)
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
	} else if (watcher == ui->titleBar) {
		switch (event->type()) {
		case QEvent::Resize:
			updateTitleBarLayout(static_cast<QResizeEvent *>(event)->size());
			break;
		default:
			break;
		}
	}

	return PLSToplevelView<QDialog>::eventFilter(watcher, event);
}

void PLSDialogView::windowStateChanged(QWindowStateChangeEvent *event)
{
	PLSToplevelView<QDialog>::windowStateChanged(event);
	pls_flush_style(ui->maxres);
}

void PLSDialogView::resizeEvent(QResizeEvent *event)
{
	PLSToplevelView<QDialog>::resizeEvent(event);
	updateTitleBarLayout(ui->titleBar->size());
	if (event && stabilizeFixedSizeOnResize()) {
		return;
	}
	resizing(event->size());
}

void PLSDialogView::paintEvent(QPaintEvent *event)
{
	PLSToplevelView<QDialog>::paintEvent(event);

#if defined(Q_OS_WIN)
	if (!isAfterWin10()) {
		QPainter painter(this);
		painter.setPen(QPen(QColor(17, 17, 17), 1, Qt::SolidLine));
		painter.setBrush(Qt::transparent);
		painter.drawRect(rect());
	}
#endif
}
