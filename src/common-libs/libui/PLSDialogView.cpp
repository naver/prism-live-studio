#include "PLSDialogView.h"
#include "ui_PLSDialogView.h"

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

PLSDialogView::PLSDialogView(QWidget *parent, Qt::WindowFlags f) : PLSToplevelView<QDialog>(parent, f)
{
	ui = pls_new<Ui::PLSDialogView>();

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
			reject();
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

	connect(this, &PLSDialogView::windowTitleChanged, this, &PLSDialogView::SetNameLabelText);
	connect(ui->content, &QWidget::windowTitleChanged, this, &PLSDialogView::setWindowTitle);

	ui->content->setObjectName(QString());
	connect(ui->content, &QObject::objectNameChanged, [this](const QString &objectName) {
		if (objectName != CONTENT) {
			setObjectName(objectName);
			ui->content->setObjectName(CONTENT);
		}
	});
}

PLSDialogView::PLSDialogView(DialogInfo info, QWidget *parent) : PLSDialogView(parent)
{
	defaultInfo = info;
}
PLSDialogView::~PLSDialogView()
{
	pls_delete(ui, nullptr);
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
	this->owidget = widget;
	widget->setParent(ui->content);
	connect(widget, &QWidget::windowTitleChanged, this, &PLSDialogView::setWindowTitle);
	setWindowTitle(widget->windowTitle());

	QHBoxLayout *l = pls_new<QHBoxLayout>(ui->content);
	l->setContentsMargins(0, 0, 0, 0);
	l->setSpacing(0);
	l->addWidget(widget);
}

int PLSDialogView::getCaptionHeight() const
{
	return captionHeight;
}

void PLSDialogView::setCaptionHeight(int captionHeight_)
{
	this->captionHeight = captionHeight_;
	ui->titleBar->setFixedHeight(this->captionHeight);
}

int PLSDialogView::getCaptionButtonSize() const
{
	return captionButtonSize;
}

void PLSDialogView::setCaptionButtonSize(int captionButtonSize_)
{
	this->captionButtonSize = captionButtonSize_;
	ui->min->setFixedSize(captionButtonSize, captionButtonSize);
	ui->maxres->setFixedSize(captionButtonSize, captionButtonSize);
	ui->close->setFixedSize(captionButtonSize, captionButtonSize);
}

int PLSDialogView::getCaptionButtonMargin() const
{
	return captionButtonMargin;
}

void PLSDialogView::setCaptionButtonMargin(int captionButtonMargin_)
{
	this->captionButtonMargin = captionButtonMargin_;
	ui->titleButtonLayout->setSpacing(captionButtonMargin);
}

int PLSDialogView::getTextMarginLeft() const
{
	return textMarginLeft;
}

void PLSDialogView::setTextMarginLeft(int textMarginLeft_)
{
	this->textMarginLeft = textMarginLeft_;
	QMargins margins = ui->titleBarLayout->contentsMargins();
	margins.setLeft(textMarginLeft);
	ui->titleBarLayout->setContentsMargins(margins);
}

int PLSDialogView::getCloseButtonMarginRight() const
{
	return closeButtonMarginRight;
}

void PLSDialogView::setCloseButtonMarginRight(int closeButtonMarginRight_)
{
	this->closeButtonMarginRight = closeButtonMarginRight_;
	QMargins margins = ui->titleBarLayout->contentsMargins();
	margins.setRight(closeButtonMarginRight);
	ui->titleBarLayout->setContentsMargins(margins);
}

bool PLSDialogView::getHasCaption() const
{
	return hasCaption;
}

void PLSDialogView::setHasCaption(bool hasCaption_)
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

bool PLSDialogView::getHasHLine() const
{
	return hasHLine;
}

void PLSDialogView::setHasHLine(bool hasHLine_)
{
	this->hasHLine = hasHLine_;
	ui->hline->setVisible(hasHLine);
}

bool PLSDialogView::getHasMinButton() const
{
	return hasMinButton;
}

bool PLSDialogView::getHasMaxResButton() const
{
	return hasMaxResButton;
}

void PLSDialogView::setHasMaxResButton(bool hasMaxResButton_)
{
	this->hasMaxResButton = hasMaxResButton_;
#if defined(Q_OS_WIN)
	ui->maxres->setVisible(hasMaxResButton);
#elif defined(Q_OS_MACOS)
	customMacWindow()->setMaxButtonHidden(!hasMaxResButton_);
#endif
}

void PLSDialogView::setHasMinButton(bool hasMinButton_)
{
	this->hasMinButton = hasMinButton_;
#if defined(Q_OS_WIN)
	ui->min->setVisible(hasMinButton);
#elif defined(Q_OS_MACOS)
	customMacWindow()->setMinButtonHidden(!hasMinButton_);
#endif
}

bool PLSDialogView::getHasCloseButton() const
{
	return hasCloseButton;
}

void PLSDialogView::setHasCloseButton(bool hasCloseButton_)
{
	this->hasCloseButton = hasCloseButton_;
#if defined(Q_OS_WIN)
	ui->close->setVisible(hasCloseButton);
#elif defined(Q_OS_MACOS)
	customMacWindow()->setCloseButtonHidden(!hasCloseButton_);
#endif
}

bool PLSDialogView::getHasHelpButton() const
{
	return hasHelpButton;
}

void PLSDialogView::setHasHelpButton(bool hasHelpButton_)
{
	hasHelpButton = hasHelpButton_;
	ui->helpBtn->setVisible(hasHelpButton);
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
	return ui->titleButtonLayout->contentsMargins().top();
}

void PLSDialogView::setCaptionButtonTopMargin(int captionButtonTopMargin)
{
	QMargins margins = ui->titleButtonLayout->contentsMargins();
	margins.setTop(captionButtonTopMargin);
	ui->titleButtonLayout->setContentsMargins(margins);
}

QWidget *PLSDialogView::titleLabel() const
{
	return ui->titleLabel;
}

int PLSDialogView::setTitleCustomWidth(int width)
{
	return titleCustomWidth = width;
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
		if (hasCaption) {
			newSize.setHeight(newSize.height() + captionHeight);
		}
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

void PLSDialogView::addMacTopMargin()
{
#if defined(Q_OS_MACOS)
	QMargins margin = ui->mainLayout->contentsMargins();
	margin.setTop(margin.top() + 20);
	ui->mainLayout->setContentsMargins(margin);
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
	emit shown();
}

void PLSDialogView::hideEvent(QHideEvent *event)
{
	PLSToplevelView<QDialog>::hideEvent(event);
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
	}

	return PLSToplevelView<QDialog>::eventFilter(watcher, event);
}

bool PLSDialogView::event(QEvent *event)
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
	return PLSToplevelView<QDialog>::event(event);
}

void PLSDialogView::windowStateChanged(QWindowStateChangeEvent *event)
{
	PLSToplevelView<QDialog>::windowStateChanged(event);
	pls_flush_style(ui->maxres);
}

void PLSDialogView::SetNameLabelText(const QString &title)
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
