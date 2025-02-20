#include "PLSFiltersItemView.h"
#include "ui_PLSFiltersItemView.h"

#include "qt-wrappers.hpp"
#include "pls-common-define.hpp"
#include "obs-app.hpp"
#include "liblog.h"
#include "action.h"
#include "log/module_names.h"
#include "ChannelCommonFunctions.h"
#include <QFontMetrics>
#include <QLineEdit>
#include <QStyle>
#include <QMenu>

#include <libui.h>

using namespace common;
PLSFiltersItemView::PLSFiltersItemView(obs_source_t *source_, QWidget *parent)
	: QWidget(parent),
	  source(source_),
	  enabledSignal(obs_source_get_signal_handler(source), "enable", OBSSourceEnabled, this),
	  renamedSignal(obs_source_get_signal_handler(source), "rename", OBSSourceRenamed, this)
{
	uint32_t flags = obs_source_get_output_flags(source_);
	async = (flags & OBS_SOURCE_ASYNC) != 0;

	ui = pls_new<Ui::PLSFiltersItemView>();
	ui->setupUi(this);
	ui->visibleButton->hide();
	ui->advButton->hide();
	ui->visibleButton->setProperty("visibilityCheckBox", true);
	setProperty("showHandCursor", true);
	setContentsMargins(0, 0, 0, 0);
	pls_add_css(this, {"OBSBasicFilters", "VisibilityCheckBox"});

	name = obs_source_get_name(source);
	UpdateNameStyle();
	ui->nameLabel->setText(GetNameElideString());
	ui->nameLabel->setToolTip(name);
	ui->nameLabel->installEventFilter(this);
	ui->visibleButton->installEventFilter(this);
	ui->advButton->installEventFilter(this);
	bool enabled = obs_source_enabled(source);
	ui->visibleButton->setChecked(enabled);

#if defined(Q_OS_WIN)
	ui->horizontalLayout->setSpacing(6);
#elif defined(Q_OS_MACOS)
	ui->horizontalLayout->setSpacing(-1);
#endif

	setCursor(Qt::ArrowCursor);
	setProperty("notShowHandCursor", true);

	connect(ui->visibleButton, &QPushButton::clicked, this, &PLSFiltersItemView::OnVisibilityButtonClicked);
	connect(ui->advButton, &QPushButton::clicked, this, [=]() { emit OnCreateCustomContextMenu(QCursor::pos(), async); });
}

PLSFiltersItemView::~PLSFiltersItemView()
{
	pls_delete(ui);
}

void PLSFiltersItemView::SetText(const QString &text)
{
	name = text;
	ui->nameLabel->setText(GetNameElideString());
}

void PLSFiltersItemView::SetColor(const QColor &, bool, bool) const {}

OBSSource PLSFiltersItemView::GetFilter() const
{
	return source;
}

bool PLSFiltersItemView::GetCurrentState() const
{
	return current;
}

void PLSFiltersItemView::SetCurrentItemState(bool state)
{
	if (this->current != state) {
		this->current = state;
		UpdateNameStyle();
		ui->nameLabel->setText(GetNameElideString());
	}
}

void PLSFiltersItemView::mousePressEvent(QMouseEvent *event)
{
	PLS_UI_STEP(MAINFILTER_MODULE, QT_TO_UTF8(name), ACTION_CLICK);
	OnMouseStatusChanged(PROPERTY_VALUE_MOUSE_STATUS_PRESSED);

	emit CurrentItemChanged(this);
	QWidget::mousePressEvent(event);
}

void PLSFiltersItemView::mouseDoubleClickEvent(QMouseEvent *event)
{
	PLS_UI_STEP(MAINFILTER_MODULE, QT_TO_UTF8(name), ACTION_DBCLICK);
	emit CurrentItemChanged(this);
	QWidget::mouseDoubleClickEvent(event);
}

void PLSFiltersItemView::mouseReleaseEvent(QMouseEvent *event)
{
	OnMouseStatusChanged(PROPERTY_VALUE_MOUSE_STATUS_HOVER);

	QWidget::mouseReleaseEvent(event);
}

void PLSFiltersItemView::enterEvent(QEnterEvent *event)
{
	ui->visibleButton->show();
	ui->advButton->show();
	OnMouseStatusChanged(PROPERTY_VALUE_MOUSE_STATUS_HOVER);
	QWidget::enterEvent(event);
}

void PLSFiltersItemView::leaveEvent(QEvent *event)
{
	OnMouseStatusChanged(PROPERTY_VALUE_MOUSE_STATUS_NORMAL);

	ui->visibleButton->hide();
	ui->advButton->hide();

	QWidget::leaveEvent(event);
}

bool PLSFiltersItemView::eventFilter(QObject *object, QEvent *event)
{
	if (object == ui->nameLabel && event->type() == QEvent::Resize) {
		QMetaObject::invokeMethod(
			this, [this]() { ui->nameLabel->setText(GetNameElideString()); }, Qt::QueuedConnection);
		return true;
	}
	if ((object == ui->visibleButton || object == ui->advButton) && event->type() == QEvent::MouseMove) {
		return true;
	}

	return QWidget::eventFilter(object, event);
}

void PLSFiltersItemView::contextMenuEvent(QContextMenuEvent *event)
{
	QWidget::contextMenuEvent(event);
}

void PLSFiltersItemView::OBSSourceEnabled(void *param, calldata_t *data)
{
	auto window = static_cast<PLSFiltersItemView *>(param);
	bool enabled = calldata_bool(data, "enabled");

	QMetaObject::invokeMethod(window, "SourceEnabled", Q_ARG(bool, enabled));
}

void PLSFiltersItemView::OBSSourceRenamed(void *param, calldata_t *data)
{
	auto window = static_cast<PLSFiltersItemView *>(param);
	const char *name = calldata_string(data, "new_name");

	QMetaObject::invokeMethod(window, "SourceRenamed", Q_ARG(QString, QT_UTF8(name)));
}

QString PLSFiltersItemView::GetNameElideString() const
{
	QFontMetrics fontWidth(ui->nameLabel->font());
	if (fontWidth.horizontalAdvance(name) > ui->nameLabel->width())
		return fontWidth.elidedText(name, Qt::ElideRight, ui->nameLabel->width());

	return name;
}

void PLSFiltersItemView::OnMouseStatusChanged(const QString &status)
{
	SetProperty(this, PROPERTY_NAME_MOUSE_STATUS, status);
}

void PLSFiltersItemView::OnVisibilityButtonClicked(bool visible) const
{
	QString log = QString("[%1 : %2] visibility button: %3").arg(obs_source_get_id(source)).arg(name).arg(visible ? "checked" : "unchecked");
	PLS_UI_STEP(MAINFILTER_MODULE, QT_TO_UTF8(log), ACTION_CLICK);
	obs_source_set_enabled(source, visible);
}

void PLSFiltersItemView::UpdateNameStyle()
{
	QString visibleStr = obs_source_enabled(source) ? STATUS_VISIBLE : STATUS_INVISIBLE;
	QString selectStr = current ? STATUS_SELECTED : STATUS_UNSELECTED;

	QString value = visibleStr + QString(".") + selectStr;
	SetProperty(ui->nameLabel, STATUS, value);
}

void PLSFiltersItemView::SetProperty(QWidget *widget, const char *property, const QVariant &value) const
{
	if (widget && property) {
		widget->setProperty(property, value);
		pls_flush_style(widget);
	}
}

void PLSFiltersItemView::OnRemoveActionTriggered()
{
	QString log = name + " delete button";
	PLS_UI_STEP(MAINFILTER_MODULE, QT_TO_UTF8(log), ACTION_CLICK);
	emit FilterRemoveTriggered(this);
}

void PLSFiltersItemView::SourceEnabled(bool enabled)
{
	ui->visibleButton->setChecked(enabled);
	UpdateNameStyle();
}

void PLSFiltersItemView::SourceRenamed(QString name_)
{
	this->name = name_;
	ui->nameLabel->setText(GetNameElideString());
	ui->nameLabel->setToolTip(name_);
}

PLSFiltersItemDelegate::PLSFiltersItemDelegate(QObject *parent) : QStyledItemDelegate(parent) {}

void PLSFiltersItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
	QStyledItemDelegate::paint(painter, option, index);

	QObject *parentObj = parent();
	QListWidget *list = qobject_cast<QListWidget *>(parentObj);
	if (!list)
		return;

	QListWidgetItem *item = list->item(index.row());
	PLSFiltersItemView *widget = qobject_cast<PLSFiltersItemView *>(list->itemWidget(item));
	if (!widget)
		return;

	bool selected = option.state.testFlag(QStyle::State_Selected);
	bool active = option.state.testFlag(QStyle::State_Active);

	QPalette palette = list->palette();
#if defined(_WIN32) || defined(__APPLE__)
	QPalette::ColorGroup group = active ? QPalette::Active : QPalette::Inactive;
#else
	QPalette::ColorGroup group = QPalette::Active;
#endif

#ifdef _WIN32
	QPalette::ColorRole highlightRole = QPalette::WindowText;
#else
	QPalette::ColorRole highlightRole = QPalette::HighlightedText;
#endif

	QPalette::ColorRole role;

	if (selected && active)
		role = highlightRole;
	else
		role = QPalette::WindowText;

	widget->SetCurrentItemState(selected);
	widget->SetColor(palette.color(group, role), active, selected);
}

QWidget *PLSFiltersItemDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
	auto editor = new QLineEdit(parent);
	editor->setAlignment(Qt::AlignVCenter);

	int width = 210;
	QListWidget *listWidget = qobject_cast<QListWidget *>(parent->parent());
	if (listWidget)
		width = listWidget->width();

	QString itemText = index.data(Qt::DisplayRole).toString();
	editor->setText(itemText);
	editor->selectAll();
	editor->setFixedWidth(width);
	return editor;
}

bool PLSFiltersItemDelegate::eventFilter(QObject *object, QEvent *event)
{
	if (!object)
		return false;

	if (event->type() == QEvent::KeyPress) {
		auto keyEvent = static_cast<QKeyEvent *>(event);

		if (keyEvent->key() == Qt::Key_Tab || keyEvent->key() == Qt::Key_Backtab) {
			return false;
		}
	}

	return QStyledItemDelegate::eventFilter(object, event);
}

void PLSFiltersItemDelegate::initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const
{
	auto font = option->font;
	font.setBold(true);
	option->font = font;
	QStyledItemDelegate::initStyleOption(option, index);
}