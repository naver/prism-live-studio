#include "window-basic-main.hpp"
#include "obs-app.hpp"
#include "source-tree.hpp"
#include "qt-wrappers.hpp"
#include "visibility-checkbox.hpp"
#include "locked-checkbox.hpp"
#include "expand-checkbox.hpp"
#include "platform.hpp"
#include "pls-common-define.hpp"
#include "PLSAction.h"
#include "action.h"
#include "liblog.h"
#include "log/module_names.h"
#include "PLSBasic.h"
#include "ChannelCommonFunctions.h"
#include <obs-frontend-api.h>
#include <obs.h>
#include <string>
#include <QLabel>
#include <QLineEdit>
#include <QSpacerItem>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QAccessible>
#include <QPainter>
#include <QStylePainter>
#include <QStyleOptionFocusRect>
#include "pls/pls-source.h"
#if defined(Q_OS_MACOS)
#include "mac/PLSPermissionHelper.h"
#endif

using namespace common;

static inline OBSScene GetCurrentScene()
{
	OBSBasic *main = reinterpret_cast<OBSBasic *>(App()->GetMainWindow());
	return main->GetCurrentScene();
}

const auto SOURCEITEM_MARGIN_UNNORMAL_LONG =
	30; // while scroll is hiden and mouse status is unnormal
const auto SOURCEITEM_MARGIN_UNNORMAL_SHORT =
	20; // while scroll is shown and mouse status is unnormal

const auto SOURCEITEM_MARGIN_NORMAL_LONG =
	20; // while scroll is hiden and mouse status is normal
const auto SOURCEITEM_MARGIN_NORMAL_SHORT =
	10; // while scroll is shown and mouse status is normal

const auto SOURCEITEM_SPACE_BEFORE_VIS = 5;
const auto SOURCEITEM_SPACE_BEFORE_LOCK = 10;

/* ========================================================================= */
void SourceLabel::resizeEvent(QResizeEvent *event)
{
	update();
	QLabel::resizeEvent(event);
}

void SourceLabel::paintEvent(QPaintEvent *event)
{
	QPainter dc(this);
	int padding = 5;
	dc.setFont(font());

	QStyleOption opt;
	opt.initFrom(this);
	auto textColor = opt.palette.color(QPalette::Text);

	QTextOption option(Qt::AlignLeft | Qt::AlignVCenter);
	option.setWrapMode(QTextOption::NoWrap);

	dc.setPen(textColor);
	dc.drawText(QRect(padding, 0, width() - padding, height()),
		    SnapSourceName(), option);
	QLabel::paintEvent(event);
}

QString SourceLabel::SnapSourceName()
{
	if (currentText.isEmpty())
		return currentText;

	QFontMetrics fontWidth(font());
	if (fontWidth.horizontalAdvance(currentText) > width() - 5)
		return fontWidth.elidedText(currentText, Qt::ElideRight,
					    width() - 5);
	else
		return currentText;
}

void SourceLabel::setText(const QString &text)
{
	currentText = text;
	update();
}

void SourceLabel::setText(const char *text)
{
	currentText = text ? text : "";
	update();
}

QString SourceLabel::GetText() const
{
	return currentText;
}

SourceTreeItem::SourceTreeItem(SourceTree *tree_, OBSSceneItem sceneitem_)
	: tree(tree_),
	  sceneitem(sceneitem_),
	  isScrollShowed(tree_->scrollBar->isVisible())
{
	setAttribute(Qt::WA_TranslucentBackground);
	setMouseTracking(true);
	setProperty("showHandCursor", true);

	connect(tree_->scrollBar, &QSourceScrollBar::SourceScrollShow, this,
		&SourceTreeItem::OnSourceScrollShow);
	obs_source_t *source = obs_sceneitem_get_source(sceneitem);
	const char *name = obs_source_get_name(source);

	OBSBasic *main = reinterpret_cast<OBSBasic *>(App()->GetMainWindow());
	const char *id = obs_source_get_id(source);

	bool sourceVisible = obs_sceneitem_visible(sceneitem);

	if (tree->iconsVisible) {
		QIcon icon;

		if (strcmp(id, "scene") == 0)
			icon = main->GetSceneIcon();
		else if (strcmp(id, "group") == 0)
			icon = main->GetGroupIcon();
		else
			icon = main->GetSourceIcon(id);

		QPixmap pixmap = icon.pixmap(QSize(16, 16));

		iconLabel = new QLabel();
		iconLabel->setObjectName("sourceIconLabel");
		//iconLabel->setPixmap(pixmap);
		iconLabel->setEnabled(sourceVisible);
		iconLabel->setStyleSheet("background: none");
	}

	vis = new VisibilityCheckBox();
	vis->setObjectName("sourceIconViewCheckbox");
	vis->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	vis->setChecked(sourceVisible);
	vis->setStyleSheet("background: none");
	vis->setAccessibleName(QTStr("Basic.Main.Sources.Visibility"));
	vis->setAccessibleDescription(
		QTStr("Basic.Main.Sources.VisibilityDescription").arg(name));

	lock = new LockedCheckBox();
	lock->setObjectName("sourceLockCheckbox");
	lock->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	lock->setChecked(obs_sceneitem_locked(sceneitem));
	lock->setStyleSheet("background: none");
	lock->setAccessibleName(QTStr("Basic.Main.Sources.Lock"));
	lock->setAccessibleDescription(
		QTStr("Basic.Main.Sources.LockDescription").arg(name));

	label = new SourceLabel(this);
	OBSDataAutoRelease data = obs_source_get_private_settings(source);
	auto deviceName = obs_data_get_default_string(data, "deviceName");
	label->setText(QString::fromStdString(name) + deviceName);
	label->setToolTip(QString::fromStdString(name) + deviceName);
	label->setObjectName("sourceNameLabel");
	label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	label->setAttribute(Qt::WA_TranslucentBackground);
	label->setEnabled(sourceVisible);

	aboveIndicator = pls_new<QLabel>(this);
	belowIndicator = pls_new<QLabel>(this);

	aboveIndicator->setFixedHeight(1);
	belowIndicator->setFixedHeight(1);
	UpdateIndicator(IndicatorType::IndicatorNormal);

#ifdef __APPLE__
	vis->setAttribute(Qt::WA_LayoutUsesWidgetRect);
	lock->setAttribute(Qt::WA_LayoutUsesWidgetRect);
#endif

	spaceBeforeVis = pls_new<QSpacerItem>(SOURCEITEM_SPACE_BEFORE_VIS, 1);
	spaceBeforeLock = pls_new<QSpacerItem>(SOURCEITEM_SPACE_BEFORE_LOCK, 1);
	rightMargin = pls_new<QSpacerItem>(SOURCEITEM_MARGIN_NORMAL_LONG, 1);
	boxLayout = new QHBoxLayout();
	boxLayout->setContentsMargins(18, 0, 0, 0);
	if (iconLabel) {
		boxLayout->addWidget(iconLabel);
		boxLayout->addSpacing(2);
	}
	boxLayout->addWidget(label);
	boxLayout->addItem(spaceBeforeVis);
	boxLayout->addWidget(vis);
	boxLayout->addItem(spaceBeforeLock);
	boxLayout->addWidget(lock);
	boxLayout->addItem(rightMargin);

	auto vLayout = pls_new<QVBoxLayout>(this);
	vLayout->setContentsMargins(0, 0, 0, 0);
	vLayout->setSpacing(0);
	vLayout->addWidget(aboveIndicator);
	vLayout->addLayout(boxLayout);
	vLayout->addWidget(belowIndicator);

	OBSDataAutoRelease privData =
		obs_sceneitem_get_private_settings(sceneitem);
	auto preset = obs_data_get_int(privData, "color-preset");

	if (preset == 1) {
		const char *color = obs_data_get_string(privData, "color");
		SetBgColor(
			SourceItemBgType::BgCustom,
			(void *)color); // format of color is like "#5555aa7f"
	} else if (preset > 1) {
		long long presetIndex =
			(preset - 2); // the index should start with 0
		SetBgColor(SourceItemBgType::BgPreset, (void *)presetIndex);
	} else {
		SetBgColor(SourceItemBgType::BgDefault, nullptr);
	}

	UpdateIcon();
	Update(false);

	/* --------------------------------------------------------- */
	auto setItemVisible = [this](bool val) {
		obs_scene_t *scene = obs_sceneitem_get_scene(sceneitem);
		obs_source_t *scenesource = obs_scene_get_source(scene);
		int64_t id = obs_sceneitem_get_id(sceneitem);
		const char *name = obs_source_get_name(scenesource);
		obs_source_t *source = obs_sceneitem_get_source(sceneitem);

		auto undo_redo = [](const std::string &name, int64_t id,
				    bool val) {
			OBSSourceAutoRelease s =
				obs_get_source_by_name(name.c_str());
			obs_scene_t *sc = obs_group_or_scene_from_source(s);
			obs_sceneitem_t *si =
				obs_scene_find_sceneitem_by_id(sc, id);
			if (si)
				obs_sceneitem_set_visible(si, val);
		};

		QString str = QTStr(val ? "Undo.ShowSceneItem"
					: "Undo.HideSceneItem");

		OBSBasic *main = OBSBasic::Get();
		main->undo_s.add_action(
			str.arg(obs_source_get_name(source), name),
			std::bind(undo_redo, std::placeholders::_1, id, !val),
			std::bind(undo_redo, std::placeholders::_1, id, val),
			name, name);

		SignalBlocker sourcesSignalBlocker(this);
		obs_sceneitem_set_visible(sceneitem, val);
	};

	auto setItemLocked = [this](bool checked) {
		SignalBlocker sourcesSignalBlocker(this);
		obs_sceneitem_set_locked(sceneitem, checked);
	};

	connect(vis, &QAbstractButton::clicked, setItemVisible);
	connect(lock, &QAbstractButton::clicked, setItemLocked);
	OnSelectChanged(obs_sceneitem_selected(sceneitem_));
	OnMouseStatusChanged(PROPERTY_VALUE_MOUSE_STATUS_NORMAL);
}

void SourceTreeItem::paintEvent(QPaintEvent *event)
{
	QStyleOption opt;
	opt.initFrom(this);
	QPainter p(this);
	style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);

	QWidget::paintEvent(event);
}

void SourceTreeItem::DisconnectSignals()
{
	sceneRemoveSignal.Disconnect();
	itemRemoveSignal.Disconnect();
	selectSignal.Disconnect();
	deselectSignal.Disconnect();
	visibleSignal.Disconnect();
	lockedSignal.Disconnect();
	renameSignal.Disconnect();
	renameExtSignal.Disconnect();
	removeSignal.Disconnect();

	if (obs_sceneitem_is_group(sceneitem))
		groupReorderSignal.Disconnect();
}

void SourceTreeItem::Clear()
{
	DisconnectSignals();
	sceneitem = nullptr;
}

void SourceTreeItem::ReconnectSignals()
{
	if (!sceneitem)
		return;

	DisconnectSignals();

	/* --------------------------------------------------------- */

	auto removeItem = [](void *data, calldata_t *cd) {
		pls_unused(cd);
		auto this_ = static_cast<SourceTreeItem *>(data);
		auto curItem = (obs_sceneitem_t *)calldata_ptr(cd, "item");

		if (curItem == this_->sceneitem) {
			QMetaObject::invokeMethod(this_->tree, "Remove",
						  Qt::QueuedConnection,
						  Q_ARG(OBSSceneItem, curItem));
			curItem = nullptr;
		}
		if (!curItem)
			QMetaObject::invokeMethod(this_, "Clear",
						  Qt::QueuedConnection);
	};

	auto itemVisible = [](void *data, calldata_t *cd) {
		pls_unused(cd);
		auto this_ = static_cast<SourceTreeItem *>(data);
		auto curItem = (obs_sceneitem_t *)calldata_ptr(cd, "item");
		bool visible = calldata_bool(cd, "visible");

		if (curItem == this_->sceneitem)
			QMetaObject::invokeMethod(this_, "VisibilityChanged",
						  Qt::QueuedConnection,
						  Q_ARG(bool, visible));
	};

	auto itemLocked = [](void *data, calldata_t *cd) {
		pls_unused(cd);
		auto this_ = static_cast<SourceTreeItem *>(data);
		auto curItem = (obs_sceneitem_t *)calldata_ptr(cd, "item");
		bool locked = calldata_bool(cd, "locked");

		if (curItem == this_->sceneitem)
			QMetaObject::invokeMethod(this_, "LockedChanged",
						  Qt::QueuedConnection,
						  Q_ARG(bool, locked));
	};

	auto itemSelect = [](void *data, calldata_t *cd) {
		SourceTreeItem *this_ =
			reinterpret_cast<SourceTreeItem *>(data);
		obs_sceneitem_t *curItem =
			(obs_sceneitem_t *)calldata_ptr(cd, "item");

		if (curItem == this_->sceneitem)
			QMetaObject::invokeMethod(this_, "Select");
	};

	auto itemDeselect = [](void *data, calldata_t *cd) {
		pls_unused(cd);
		auto this_ = static_cast<SourceTreeItem *>(data);
		auto curItem = (obs_sceneitem_t *)calldata_ptr(cd, "item");

		if (curItem == this_->sceneitem)
			QMetaObject::invokeMethod(this_, "Deselect",
						  Qt::QueuedConnection);
	};

	auto reorderGroup = [](void *data, calldata_t *cd) {
		pls_unused(cd);
		auto this_ = static_cast<SourceTreeItem *>(data);
		QMetaObject::invokeMethod(this_->tree, "ReorderItems",
					  Qt::QueuedConnection);
	};

	const obs_scene_t *scene = obs_sceneitem_get_scene(sceneitem);
	const obs_source_t *sceneSource = obs_scene_get_source(scene);
	signal_handler_t *signal = obs_source_get_signal_handler(sceneSource);

	sceneRemoveSignal.Connect(signal, "remove", removeItem, this);
	itemRemoveSignal.Connect(signal, "item_remove", removeItem, this);
	visibleSignal.Connect(signal, "item_visible", itemVisible, this);
	lockedSignal.Connect(signal, "item_locked", itemLocked, this);
	selectSignal.Connect(signal, "item_select", itemSelect, this);
	deselectSignal.Connect(signal, "item_deselect", itemDeselect, this);

	if (obs_sceneitem_is_group(sceneitem)) {
		const obs_source_t *source =
			obs_sceneitem_get_source(sceneitem);
		signal = obs_source_get_signal_handler(source);

		groupReorderSignal.Connect(signal, "reorder", reorderGroup,
					   this);
	}

	/* --------------------------------------------------------- */

	auto renamed = [](void *data, calldata_t *cd) {
		pls_unused(cd);
		auto this_ = (SourceTreeItem *)(data);
		const char *name = calldata_string(cd, "new_name");
		QMetaObject::invokeMethod(this_, "Renamed",
					  Q_ARG(QString, QT_UTF8(name)));
	};

	auto renamedExt = [](void *data, calldata_t *cd) {
		pls_unused(cd);
		auto this_ = (SourceTreeItem *)(data);
		QMetaObject::invokeMethod(this_, "RenamedExt");
	};

	auto removeSource = [](void *data, calldata_t *) {
		SourceTreeItem *this_ =
			reinterpret_cast<SourceTreeItem *>(data);
		this_->DisconnectSignals();
		this_->sceneitem = nullptr;
		QMetaObject::invokeMethod(this_->tree, "RefreshItems");
	};

	const obs_source_t *source = obs_sceneitem_get_source(sceneitem);
	signal = obs_source_get_signal_handler(source);
	renameSignal.Connect(signal, "rename", renamed, this);
	//removeSignal.Connect(signal, "remove", removeSource, this);
}

void SourceTreeItem::mouseDoubleClickEvent(QMouseEvent *event)
{
	QWidget::mouseDoubleClickEvent(event);

	if (expand) {
		expand->setChecked(!expand->isChecked());
	} else {
		obs_source_t *source = obs_sceneitem_get_source(sceneitem);
		auto main = dynamic_cast<OBSBasic *>(App()->GetMainWindow());
		if (source && main) {
			if (!obs_source_configurable(source)) {
#if defined(Q_OS_MACOS)
				PLSPermissionHelper::AVType avType;
				auto permissionStatus = PLSPermissionHelper::
					checkPermissionWithSource(source,
								  avType);
				QMetaObject::invokeMethod(
					this,
					[avType, permissionStatus, this]() {
						PLSPermissionHelper::
							showPermissionAlertIfNeeded(
								avType,
								permissionStatus);
					},
					Qt::QueuedConnection);
#endif
				return;
			}
			main->CreatePropertiesWindow(source,
						     OPERATION_NONE /*, main*/);
		}
	}
}

void SourceTreeItem::mousePressEvent(QMouseEvent *event)
{
	QWidget::mousePressEvent(event);

	if (!editing)
		OnMouseStatusChanged(PROPERTY_VALUE_MOUSE_STATUS_PRESSED);
}

void SourceTreeItem::mouseReleaseEvent(QMouseEvent *event)
{
	QWidget::mouseReleaseEvent(event);

	if (!editing)
		OnMouseStatusChanged(PROPERTY_VALUE_MOUSE_STATUS_HOVER);
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
void SourceTreeItem::enterEvent(QEnterEvent *event)
#else
void SourceTreeItem::enterEvent(QEvent *event)
#endif
{
	QWidget::enterEvent(event);

	if (!editing)
		OnMouseStatusChanged(PROPERTY_VALUE_MOUSE_STATUS_HOVER);
	OBSBasicPreview *preview = OBSBasicPreview::Get();

	std::lock_guard<std::mutex> lock(preview->selectMutex);
	preview->hoveredPreviewItems.clear();
	preview->hoveredPreviewItems.push_back(sceneitem);
}

void SourceTreeItem::leaveEvent(QEvent *event)
{
	QWidget::leaveEvent(event);

	OnMouseStatusChanged(PROPERTY_VALUE_MOUSE_STATUS_NORMAL);
	OBSBasicPreview *preview = OBSBasicPreview::Get();

	std::lock_guard<std::mutex> lock(preview->selectMutex);
	preview->hoveredPreviewItems.clear();
}

void SourceTreeItem::resizeEvent(QResizeEvent *event)
{
	QWidget::resizeEvent(event);
}

bool SourceTreeItem::IsEditing() const
{
	return editor != nullptr;
}

OBSSceneItem SourceTreeItem::SceneItem() const
{
	return sceneitem;
}

QString GenerateSourceItemBg(const char *property, const char *color)
{
	return QString::asprintf(
		"SourceTreeItem,SourceTreeItem[status=\"%s\"]{background-color: %s;}",
		property, color);
}

void SourceTreeItem::SetBgColor(SourceItemBgType t, void *param)
{
	QString qss = "";

	do {
		if (t == SourceItemBgType::BgCustom) {
			if (param) {
				auto color = (const char *)param;
				qss += GenerateSourceItemBg(
					PROPERTY_VALUE_MOUSE_STATUS_NORMAL,
					color);
				qss += GenerateSourceItemBg(
					PROPERTY_VALUE_MOUSE_STATUS_HOVER,
					color);
				qss += GenerateSourceItemBg(
					PROPERTY_VALUE_MOUSE_STATUS_PRESSED,
					color);
			}
		} else if (t == SourceItemBgType::BgPreset) {
			auto index = (int)(intptr_t)param;
			if (index < 0 ||
			    index >= presetColorListWithOpacity.size())
				break;
			QString color = presetColorListWithOpacity[index];
			qss += GenerateSourceItemBg(
				PROPERTY_VALUE_MOUSE_STATUS_NORMAL,
				color.toUtf8());
			qss += GenerateSourceItemBg(
				PROPERTY_VALUE_MOUSE_STATUS_HOVER,
				color.toUtf8());
			qss += GenerateSourceItemBg(
				PROPERTY_VALUE_MOUSE_STATUS_PRESSED,
				color.toUtf8());
		}

	} while (false);

	if (qss.isEmpty()) {
		// default style
		qss += GenerateSourceItemBg(PROPERTY_VALUE_MOUSE_STATUS_NORMAL,
					    "#272727");
		qss += GenerateSourceItemBg(PROPERTY_VALUE_MOUSE_STATUS_HOVER,
					    "#444444");
		qss += GenerateSourceItemBg(PROPERTY_VALUE_MOUSE_STATUS_PRESSED,
					    "#2d2d2d");
	}

	this->setStyleSheet(qss);
	OnMouseStatusChanged(PROPERTY_VALUE_MOUSE_STATUS_NORMAL);
}

void SourceTreeItem::EnterEditMode()
{
	editing = true;
	OnMouseStatusChanged(PROPERTY_VALUE_MOUSE_STATUS_NORMAL);

	const obs_source_t *source = obs_sceneitem_get_source(sceneitem);
	const char *name = obs_source_get_name(source);

	setFocusPolicy(Qt::StrongFocus);
	boxLayout->removeWidget(label);
	editor = pls_new<QLineEdit>(name ? name : "");
	editor->selectAll();
	editor->installEventFilter(this);
	editor->setObjectName(OBJECT_NAME_SOURCE_RENAME_EDIT);
	boxLayout->insertWidget(2, editor);
	setFocusProxy(editor);
}

void SourceTreeItem::ExitEditMode(bool save)
{
	editing = false;
	ExitEditModeInternal(save);

	if (tree->undoSceneData) {
		OBSBasic *main = OBSBasic::Get();
		main->undo_s.pop_disabled();

		OBSData redoSceneData = main->BackupScene(GetCurrentScene());

		QString text = QTStr("Undo.GroupItems").arg(newName.c_str());
		main->CreateSceneUndoRedoAction(text, tree->undoSceneData,
						redoSceneData);

		tree->undoSceneData = nullptr;
	}
}

void SourceTreeItem::ExitEditModeInternal(bool save)
{
	if (!editor) {
		return;
	}
	OBSBasic *main = reinterpret_cast<OBSBasic *>(App()->GetMainWindow());
	OBSScene scene = main->GetCurrentScene();

	newName = QT_TO_UTF8(editor->text().simplified());

	setFocusProxy(nullptr);
	int index = boxLayout->indexOf(editor);
	boxLayout->removeWidget(editor);
	pls_delete(editor, nullptr);
	setFocusPolicy(Qt::NoFocus);
	boxLayout->insertWidget(2, label);

	/* ----------------------------------------- */
	/* check for empty string                    */

	if (!save)
		return;

	if (newName.empty()) {
		OBSMessageBox::information(main, QTStr("Alert.Title"),
					   QTStr("NoNameEntered.Text"));
		return;
	}

	/* ----------------------------------------- */
	/* Check for same name                       */

	obs_source_t *source = obs_sceneitem_get_source(sceneitem);
	if (!source)
		return;

	const char *tempName = obs_source_get_name(source);
	if (!tempName)
		return;

	if (strcmp(newName.c_str(), tempName) == 0)
		return;

	/* ----------------------------------------- */
	/* check for existing source                 */

	OBSSourceAutoRelease existingSource =
		obs_get_source_by_name(newName.c_str());
	bool exists = !!existingSource;

	if (exists) {
		OBSMessageBox::information(main, QTStr("Alert.Title"),
					   QTStr("NameExists.Text"));
		return;
	}

	/* ----------------------------------------- */
	/* rename                                    */

	SignalBlocker sourcesSignalBlocker(this);
	std::string prevName(obs_source_get_name(source));
	std::string scene_name =
		obs_source_get_name(main->GetCurrentSceneSource());
	auto undo = [scene_name, prevName, main](const std::string &data) {
		OBSSourceAutoRelease source =
			obs_get_source_by_name(data.c_str());
		obs_source_set_name(source, prevName.c_str());

		OBSSourceAutoRelease scene_source =
			obs_get_source_by_name(scene_name.c_str());
		main->SetCurrentScene(scene_source.Get(), true);
	};

	std::string editedName = newName;

	auto redo = [scene_name, main, editedName](const std::string &data) {
		OBSSourceAutoRelease source =
			obs_get_source_by_name(data.c_str());
		obs_source_set_name(source, editedName.c_str());

		OBSSourceAutoRelease scene_source =
			obs_get_source_by_name(scene_name.c_str());
		main->SetCurrentScene(scene_source.Get(), true);
	};

	main->undo_s.add_action(QTStr("Undo.Rename").arg(newName.c_str()), undo,
				redo, newName, prevName);

	obs_source_set_name(source, newName.c_str());
	OBSDataAutoRelease data = obs_source_get_private_settings(source);
	auto deviceName = obs_data_get_default_string(data, "deviceName");
	label->setText(QT_UTF8(newName.c_str()) + deviceName);
	label->setToolTip(QT_UTF8(newName.c_str()) + deviceName);
}

bool SourceTreeItem::eventFilter(QObject *object, QEvent *event)
{
	if (editor != object)
		return false;

	if (LineEditCanceled(event)) {
		QMetaObject::invokeMethod(this, "ExitEditMode",
					  Qt::QueuedConnection,
					  Q_ARG(bool, false));
		return true;
	}
	if (LineEditChanged(event)) {
		QMetaObject::invokeMethod(this, "ExitEditMode",
					  Qt::QueuedConnection,
					  Q_ARG(bool, true));
		return true;
	}

	return false;
}

void SourceTreeItem::VisibilityChanged(bool visible)
{
	//PRISM/ZengQin/20200818/#4026/for all sources
	UpdateIcon();
	UpdateNameColor(selected, visible);

	//PRISM/ZengQin/20200811/#4026/for media controller
	tree->OnVisibleItemChanged(sceneitem, visible);

	vis->setChecked(visible);
}

void SourceTreeItem::LockedChanged(bool locked)
{
	lock->setChecked(locked);
	OBSBasic::Get()->UpdateEditMenu();
}

void SourceTreeItem::Renamed(const QString &name)
{
	const auto source = obs_sceneitem_get_source(sceneitem);
	if (!source)
		return;
	OBSDataAutoRelease data = obs_source_get_private_settings(source);
	auto deviceName = obs_data_get_default_string(data, "deviceName");
	label->setText(name + deviceName);
	label->setToolTip(name + deviceName);
}

void SourceTreeItem::RenamedExt()
{
	const auto source = obs_sceneitem_get_source(sceneitem);
	if (!source)
		return;

	QString name = obs_source_get_name(source);
	OBSDataAutoRelease data = obs_source_get_private_settings(source);
	auto deviceName = obs_data_get_default_string(data, "deviceName");

	label->setText(name + deviceName);
	label->setToolTip(name + deviceName);
}

void SourceTreeItem::Update(bool force)
{
	OBSScene scene = GetCurrentScene();
	const obs_scene_t *itemScene = obs_sceneitem_get_scene(sceneitem);

	Type newType;

	/* ------------------------------------------------- */
	/* if it's a group item, insert group checkbox       */

	if (obs_sceneitem_is_group(sceneitem)) {
		newType = Type::Group;

		/* ------------------------------------------------- */
		/* if it's a group sub-item                          */

	} else if (itemScene != scene) {
		newType = Type::SubItem;

		/* ------------------------------------------------- */
		/* if it's a regular item                            */

	} else {
		newType = Type::Item;
	}

	/* ------------------------------------------------- */

	if (!force && newType == type) {
		return;
	}

	/* ------------------------------------------------- */

	ReconnectSignals();

	if (spacer) {
		boxLayout->removeItem(spacer);
		delete spacer;
		spacer = nullptr;
	}

	if (type == Type::Group) {
		boxLayout->removeWidget(expand);
		expand->deleteLater();
		expand = nullptr;
	}

	type = newType;

	if (type == Type::SubItem) {
		spacer = new QSpacerItem(16, 1);
		boxLayout->insertItem(0, spacer);

	} else if (type == Type::Group) {
		expand = pls_new<SourceTreeSubItemCheckBox>();
		expand->setSizePolicy(QSizePolicy::Maximum,
				      QSizePolicy::Maximum);
#ifdef __APPLE__
		expand->setAttribute(Qt::WA_LayoutUsesWidgetRect);
#endif
		boxLayout->insertWidget(0, expand);
		boxLayout->setContentsMargins(7, 0, 0, 0);

		OBSDataAutoRelease data =
			obs_sceneitem_get_private_settings(sceneitem);
		expand->blockSignals(true);
		expand->setChecked(obs_data_get_bool(data, "collapsed"));
		expand->blockSignals(false);

		connect(expand, &QPushButton::toggled, this,
			&SourceTreeItem::ExpandClicked);

	} else {
		spacer = pls_new<QSpacerItem>(3, 1);
		boxLayout->insertItem(0, spacer);
	}
}

void SourceTreeItem::OnIconTypeChanged(QString value)
{
	if (!iconLabel) {
		return;
	}
	iconLabel->setProperty(PROPERTY_NAME_ICON_TYPE, value);
	iconLabel->style()->unpolish(iconLabel);
	iconLabel->style()->polish(iconLabel);
}

void SourceTreeItem::OnMouseStatusChanged(const char *s)
{
	isItemNormal =
		(s && 0 == strcmp(s, PROPERTY_VALUE_MOUSE_STATUS_NORMAL));
	if (isItemNormal) {
		vis->hide();
		lock->hide();
	} else {
		vis->show();
		lock->show();
	}
	UpdateRightMargin();

	this->setProperty(PROPERTY_NAME_MOUSE_STATUS, s);
	this->style()->unpolish(this);
	this->style()->polish(this);
}

void SourceTreeItem::UpdateIndicator(IndicatorType t)
{
	QString normalBk = "background: transparent;";
	QString highlightBk =
		"background:" + QString(SCENE_SCROLLCONTENT_LINE_COLOR) +
		QString(";");

	switch (t) {
	case SourceTreeItem::IndicatorType::IndicatorAbove:
		belowIndicator->setStyleSheet(normalBk);
		aboveIndicator->setStyleSheet(highlightBk);
		break;
	case SourceTreeItem::IndicatorType::IndicatorBelow:
		aboveIndicator->setStyleSheet(normalBk);
		belowIndicator->setStyleSheet(highlightBk);
		break;

	case SourceTreeItem::IndicatorType::IndicatorNormal:
		aboveIndicator->setStyleSheet(normalBk);
		belowIndicator->setStyleSheet(normalBk);
		break;
	default:
		break;
	}
}

void SourceTreeItem::OnSelectChanged(bool isSelected)
{
	selected = isSelected;

	UpdateIcon();
	UpdateNameColor(isSelected, obs_sceneitem_visible(sceneitem));
	emit SelectItemChanged(sceneitem, isSelected);
}

void SourceTreeItem::UpdateRightMargin()
{
	if (isItemNormal) {
		spaceBeforeVis->changeSize(0, 0);
		spaceBeforeLock->changeSize(0, 0);
		if (isScrollShowed)
			rightMargin->changeSize(SOURCEITEM_MARGIN_NORMAL_SHORT,
						1);
		else
			rightMargin->changeSize(SOURCEITEM_MARGIN_NORMAL_LONG,
						1);
	} else {
		spaceBeforeVis->changeSize(SOURCEITEM_SPACE_BEFORE_VIS, 1);
		spaceBeforeLock->changeSize(SOURCEITEM_SPACE_BEFORE_LOCK, 1);
		if (isScrollShowed)
			rightMargin->changeSize(
				SOURCEITEM_MARGIN_UNNORMAL_SHORT, 1);
		else
			rightMargin->changeSize(SOURCEITEM_MARGIN_UNNORMAL_LONG,
						1);
	}

	boxLayout->invalidate();
}

void SourceTreeItem::ExpandClicked(bool checked) const
{
	OBSDataAutoRelease data = obs_sceneitem_get_private_settings(sceneitem);

	obs_data_set_bool(data, "collapsed", checked);

	if (!checked)
		tree->GetStm()->ExpandGroup(sceneitem);
	else
		tree->GetStm()->CollapseGroup(sceneitem);
}

void SourceTreeItem::Select()
{
	tree->SelectItem(sceneitem, true);
	OBSBasic::Get()->UpdateContextBarDeferred();
	OBSBasic::Get()->UpdateEditMenu();
}

void SourceTreeItem::Deselect()
{
	tree->SelectItem(sceneitem, false);
	OBSBasic::Get()->UpdateContextBarDeferred();
	OBSBasic::Get()->UpdateEditMenu();
}

// string should match with qss
const QString SOURCE_ICON_VISIBLE = "visible";
const QString SOURCE_ICON_INVIS = "invisible";
const QString SOURCE_ICON_SELECT = "select";
const QString SOURCE_ICON_UNSELECT = "unselect";
const QString SOURCE_ICON_VALID = "valid";
const QString SOURCE_ICON_INVALID = "invalid";
const QString SOURCE_ICON_SCENE = "scene";
const QString SOURCE_ICON_GROUP = "group";
const QString SOURCE_ICON_IMAGE = "image";
const QString SOURCE_ICON_COLOR = "color";
const QString SOURCE_ICON_SLIDESHOW = "slideshow";
const QString SOURCE_ICON_AUDIOINPUT = "audioinput";
const QString SOURCE_ICON_AUDIOOUTPUT = "audiooutput";
const QString SOURCE_ICON_MONITOR = "monitor";
const QString SOURCE_ICON_WINDOW = "window";
const QString SOURCE_ICON_GAME = "game";
const QString SOURCE_ICON_CAMERA = "camera";
const QString SOURCE_ICON_OBS_CAMERA = "obscamera";
const QString SOURCE_ICON_TEXT = "text";
const QString SOURCE_ICON_MEDIA = "media";
const QString SOURCE_ICON_BROWSER = "browser";
const QString SOURCE_ICON_GIPHY = "giphy";
const QString SOURCE_ICON_DEFAULT = "plugin";
const QString SOURCE_ICON_BGM = "bgm";
const QString SOURCE_ICON_NDI = "ndi";
const QString SOURCE_ICON_TEXT_TEMPLATE = "textmotion";
const QString SOURCE_ICON_CHAT = "chat";
const QString SOURCE_ICON_AUDIOV = "audiov";
const QString SOURCE_ICON_VIRTUAL_BACKGROUND = "virtualbackground";
const QString SOURCE_ICON_PRISM_MOBILE = "prismMobile";
const QString SOURCE_ICON_PRISM_STICKER = "prismSticker";
const QString SOURCE_ICON_TIMER = "timer";
const QString SOURCE_ICON_APP_AUDIO = "appAudio";
const QString SOURCE_ICON_VIEWER_COUNT = "viewercount";
const QString SOURCE_ICON_DECKLINK_INPUT = "decklinkinput";
const QString SOURCE_ICON_PRISM_LENS = "prismlens";

QString GetPLSIconKey(pls_icon_type type)
{
	switch (type) {
	case PLS_ICON_TYPE_REGION:
		return SOURCE_ICON_MONITOR;
	case PLS_ICON_TYPE_OBS_CAMERA:
		return SOURCE_ICON_OBS_CAMERA;
	case PLS_ICON_TYPE_BGM:
		return SOURCE_ICON_BGM;
	case PLS_ICON_TYPE_GIPHY:
		return SOURCE_ICON_GIPHY;
	case PLS_ICON_TYPE_NDI:
		return SOURCE_ICON_NDI;
	case PLS_ICON_TYPE_TEXT_TEMPLATE:
		return SOURCE_ICON_TEXT_TEMPLATE;
	case PLS_ICON_TYPE_CHAT:
		return SOURCE_ICON_CHAT;
	case PLS_ICON_TYPE_SPECTRALIZER:
		return SOURCE_ICON_AUDIOV;
	case PLS_ICON_TYPE_VIRTUAL_BACKGROUND:
		return SOURCE_ICON_VIRTUAL_BACKGROUND;
	case PLS_ICON_TYPE_PRISM_MOBILE:
		return SOURCE_ICON_PRISM_MOBILE;
	case PLS_ICON_TYPE_PRISM_STICKER:
		return SOURCE_ICON_PRISM_STICKER;
	case PLS_ICON_TYPE_PRISM_TIMER:
		return SOURCE_ICON_TIMER;
	case PLS_ICON_TYPE_APP_AUDIO:
		return SOURCE_ICON_APP_AUDIO;
	case PLS_ICON_TYPE_VIEWER_COUNT:
		return SOURCE_ICON_VIEWER_COUNT;
	case PLS_ICON_TYPE_DECKLINK_INPUT:
		return SOURCE_ICON_DECKLINK_INPUT;
	case PLS_ICON_TYPE_PRISM_LENS:
		return SOURCE_ICON_PRISM_LENS;
	default:
		return SOURCE_ICON_DEFAULT;
	}
}

QString GetIconKey(obs_icon_type type)
{
	if (static_cast<pls_icon_type>(type) > PLS_ICON_TYPE_BASE)
		return GetPLSIconKey(static_cast<pls_icon_type>(type));

	switch (type) {
	case OBS_ICON_TYPE_IMAGE:
		return SOURCE_ICON_IMAGE;
	case OBS_ICON_TYPE_COLOR:
		return SOURCE_ICON_COLOR;
	case OBS_ICON_TYPE_SLIDESHOW:
		return SOURCE_ICON_SLIDESHOW;
	case OBS_ICON_TYPE_AUDIO_INPUT:
		return SOURCE_ICON_AUDIOINPUT;
	case OBS_ICON_TYPE_AUDIO_OUTPUT:
		return SOURCE_ICON_AUDIOOUTPUT;
	case OBS_ICON_TYPE_DESKTOP_CAPTURE:
		return SOURCE_ICON_MONITOR;
	case OBS_ICON_TYPE_WINDOW_CAPTURE:
		return SOURCE_ICON_WINDOW;
	case OBS_ICON_TYPE_GAME_CAPTURE:
		return SOURCE_ICON_GAME;
	case OBS_ICON_TYPE_CAMERA:
		return SOURCE_ICON_CAMERA;
	case OBS_ICON_TYPE_TEXT:
		return SOURCE_ICON_TEXT;
	case OBS_ICON_TYPE_MEDIA:
		return SOURCE_ICON_MEDIA;
	case OBS_ICON_TYPE_BROWSER:
		return SOURCE_ICON_BROWSER;
	case OBS_ICON_TYPE_PROCESS_AUDIO_OUTPUT:
		return SOURCE_ICON_APP_AUDIO;
	default:
		return SOURCE_ICON_DEFAULT;
	}
}

void SourceTreeItem::UpdateNameColor(bool isSelected, bool isVisible)
{
	QString visibleStr = isVisible ? SOURCE_ICON_VISIBLE
				       : SOURCE_ICON_INVIS;
	QString selectStr = isSelected ? SOURCE_ICON_SELECT
				       : SOURCE_ICON_UNSELECT;

	QString value = visibleStr + QString(".") + selectStr;
	label->setProperty(PROPERTY_NAME_STATUS, value);

	label->style()->unpolish(label);
	label->style()->polish(label);
}

void SourceTreeItem::UpdateIcon()
{
	const obs_source_t *source = obs_sceneitem_get_source(sceneitem);
	if (!source)
		return;

	const char *id = obs_source_get_id(source);
	if (!id)
		return;

	QString visibleStr = obs_sceneitem_visible(sceneitem)
				     ? SOURCE_ICON_VISIBLE
				     : SOURCE_ICON_INVIS;
	QString selectStr = selected ? SOURCE_ICON_SELECT
				     : SOURCE_ICON_UNSELECT;

	if (strcmp(id, "scene") == 0) {
		OnIconTypeChanged(visibleStr + QString(".") + selectStr +
				  QString(".") + QString(SOURCE_ICON_SCENE));
	} else if (strcmp(id, "group") == 0) {
		OnIconTypeChanged(visibleStr + QString(".") + selectStr +
				  QString(".") + QString(SOURCE_ICON_GROUP));
	} else {
		QString value = visibleStr + QString(".") +
				QString(SOURCE_ICON_VALID) + QString(".") +
				selectStr + QString(".") +
				GetIconKey(obs_source_get_icon_type(id));
		OnIconTypeChanged(value);
	}
}

void SourceTreeItem::OnSourceScrollShow(bool isShow)
{
	isScrollShowed = isShow;
	UpdateRightMargin();
}

/* ========================================================================= */

void SourceTreeModel::OBSFrontendEvent(enum obs_frontend_event event, void *ptr)
{
	SourceTreeModel *stm = reinterpret_cast<SourceTreeModel *>(ptr);

	switch ((int)event) {
	case OBS_FRONTEND_EVENT_PREVIEW_SCENE_CHANGED:
		stm->SceneChanged();
		break;
	case OBS_FRONTEND_EVENT_EXIT:
	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CLEANUP:
		stm->Clear();
		break;
	default:
		break;
	}
}

void SourceTreeModel::Clear()
{
	beginResetModel();
	items.clear();
	endResetModel();

	hasGroups = false;
}

static bool enumItem(obs_scene_t *, obs_sceneitem_t *item, void *ptr)
{
	QVector<OBSSceneItem> &items =
		*static_cast<QVector<OBSSceneItem> *>(ptr);

	obs_source_t *src = obs_sceneitem_get_source(item);
	if (obs_source_removed(src)) {
		return true;
	}

	if (obs_sceneitem_is_group(item)) {
		OBSDataAutoRelease data =
			obs_sceneitem_get_private_settings(item);

		bool collapse = obs_data_get_bool(data, "collapsed");
		if (!collapse) {
			obs_scene_t *scene =
				obs_sceneitem_group_get_scene(item);

			obs_scene_enum_items(scene, enumItem, &items);
		}
	}

	items.insert(0, item);
	return true;
}

struct ChildAndParent {
	std::map<obs_sceneitem_t *, std::pair<obs_sceneitem_t *, bool>>
		childAndParent;
	std::pair<obs_sceneitem_t *, bool> parent;
};
static bool enumItemForParent(obs_scene_t *, obs_sceneitem_t *item, void *ptr)
{
	auto ref = (ChildAndParent *)(ptr);
	if (!ref)
		return false;

	ChildAndParent &sourceParents = *ref;
	sourceParents.childAndParent.insert(
		std::make_pair(item, sourceParents.parent));
	if (obs_sceneitem_is_group(item)) {

		std::pair<obs_sceneitem_t *, bool> parentTemp =
			sourceParents.parent;
		sourceParents.parent =
			std::make_pair(item, obs_sceneitem_visible(item));
		obs_sceneitem_group_enum_items(item, enumItemForParent,
					       &sourceParents);
		sourceParents.parent = parentTemp;
	}
	return true;
}

void SourceTreeModel::SceneChanged()
{
	OBSScene scene = GetCurrentScene();

	beginResetModel();
	items.clear();
	obs_scene_enum_items(scene, enumItem, &items);
	endResetModel();

	UpdateGroupState(false);
	st->ResetWidgets();

	for (int i = 0; i < items.count(); i++) {
		bool select = obs_sceneitem_selected(items[i]);
		QModelIndex index = createIndex(i, 0);
		st->selectionModel()->select(
			index, select ? QItemSelectionModel::Select
				      : QItemSelectionModel::Deselect);
	}
}

/* moves a scene item index (blame linux distros for using older Qt builds) */
static inline void MoveItem(QVector<OBSSceneItem> &items, int oldIdx,
			    int newIdx)
{
	OBSSceneItem item = items[oldIdx];
	items.remove(oldIdx);
	items.insert(newIdx, item);
}

/* reorders list optimally with model reorder funcs */
void SourceTreeModel::ReorderItems()
{
	OBSScene scene = GetCurrentScene();

	QVector<OBSSceneItem> newitems;
	obs_scene_enum_items(scene, enumItem, &newitems);

	/* if item list has changed size, do full reset */
	if (newitems.count() != items.count()) {
		SceneChanged();
		return;
	}

	for (;;) {
		int idx1Old = 0;
		int idx1New = 0;
		int count;
		int i;

		/* find first starting changed item index */
		for (i = 0; i < newitems.count(); i++) {
			obs_sceneitem_t *oldItem = items[i];
			obs_sceneitem_t *newItem = newitems[i];
			if (oldItem != newItem) {
				idx1Old = i;
				break;
			}
		}

		/* if everything is the same, break */
		if (i == newitems.count()) {
			break;
		}

		/* find new starting index */
		for (i = idx1Old + 1; i < newitems.count(); i++) {
			obs_sceneitem_t *oldItem = items[idx1Old];
			obs_sceneitem_t *newItem = newitems[i];

			if (oldItem == newItem) {
				idx1New = i;
				break;
			}
		}

		/* if item could not be found, do full reset */
		if (i == newitems.count()) {
			SceneChanged();
			return;
		}

		/* get move count */
		for (count = 1; (idx1New + count) < newitems.count(); count++) {
			int oldIdx = idx1Old + count;
			int newIdx = idx1New + count;

			obs_sceneitem_t *oldItem = items[oldIdx];
			obs_sceneitem_t *newItem = newitems[newIdx];

			if (oldItem != newItem) {
				break;
			}
		}

		/* move items */
		beginMoveRows(QModelIndex(), idx1Old, idx1Old + count - 1,
			      QModelIndex(), idx1New + count);
		for (i = 0; i < count; i++) {
			int to = idx1New + count;
			if (to > idx1Old)
				to--;
			MoveItem(items, idx1Old, to);
		}
		endMoveRows();
		emit itemReorder();
	}
}

int SourceTreeModel::Count() const
{
	return items.count();
}

QVector<OBSSceneItem> SourceTreeModel::GetItems() const
{
	return items;
}

void SourceTreeModel::Add(obs_sceneitem_t *item)
{
	if (obs_sceneitem_is_group(item)) {
		SceneChanged();
	} else {
		beginInsertRows(QModelIndex(), 0, 0);
		items.insert(0, item);
		endInsertRows();

		st->UpdateWidget(createIndex(0, 0, nullptr), item);
	}
}

void SourceTreeModel::Remove(const void *itemPointer)
{
	int idx = -1;
	OBSSceneItem item;
	for (int i = 0; i < items.count(); i++) {
		if (items[i].Get() == itemPointer) {
			item = items[i];
			idx = i;
			break;
		}
	}

	if (idx == -1)
		return;

	int startIdx = idx;
	int endIdx = idx;

	bool is_group = obs_sceneitem_is_group(item);
	if (is_group) {
		const obs_scene_t *scene = obs_sceneitem_group_get_scene(item);

		for (int i = endIdx + 1; i < items.count(); i++) {
			const obs_sceneitem_t *subitem = items[i];
			const obs_scene_t *subscene =
				obs_sceneitem_get_scene(subitem);

			if (subscene == scene)
				endIdx = i;
			else
				break;
		}
	}

	QVector<OBSSceneItem> tmpitems;
	for (int i = startIdx; i <= endIdx; i++) {
		tmpitems.push_back(items[i]);
	}
	emit itemRemoves(tmpitems);

	beginRemoveRows(QModelIndex(), startIdx, endIdx);
	items.remove(idx, endIdx - startIdx + 1);
	endRemoveRows();

	if (is_group)
		UpdateGroupState(true);
}

OBSSceneItem SourceTreeModel::Get(int idx)
{
	if (idx == -1 || idx >= items.count())
		return OBSSceneItem();
	return items[idx];
}

SourceTreeModel::SourceTreeModel(SourceTree *st_)
	: QAbstractListModel(st_), st(st_)
{
	obs_frontend_add_event_callback(OBSFrontendEvent, this);
}

SourceTreeModel::~SourceTreeModel()
{
	obs_frontend_remove_event_callback(OBSFrontendEvent, this);
}

int SourceTreeModel::rowCount(const QModelIndex &parent) const
{
	return parent.isValid() ? 0 : items.count();
}

QVariant SourceTreeModel::data(const QModelIndex &index, int role) const
{
	if (role == Qt::AccessibleTextRole) {
		if (items.isEmpty()) {
			return QVariant();
		}
		OBSSceneItem item = items[index.row()];

		const obs_source_t *source = obs_sceneitem_get_source(item);
		return QVariant(QT_UTF8(obs_source_get_name(source)));
	}

	return QVariant();
}

Qt::ItemFlags SourceTreeModel::flags(const QModelIndex &index) const
{
	if (!index.isValid())
		return QAbstractListModel::flags(index) | Qt::ItemIsDropEnabled;

	if (items.isEmpty()) {
		return QAbstractListModel::flags(index) | Qt::ItemIsDropEnabled;
	}
	obs_sceneitem_t *item = items[index.row()];
	bool is_group = obs_sceneitem_is_group(item);

	return QAbstractListModel::flags(index) | Qt::ItemIsEditable |
	       Qt::ItemIsDragEnabled |
	       (is_group ? Qt::ItemIsDropEnabled : Qt::NoItemFlags);
}

Qt::DropActions SourceTreeModel::supportedDropActions() const
{
	return QAbstractItemModel::supportedDropActions() | Qt::MoveAction;
}

QString SourceTreeModel::GetNewGroupName() const
{
	OBSScene scene = GetCurrentScene();
	QString name = QTStr("Group");

	int i = 2;
	for (;;) {
		OBSSourceAutoRelease group =
			obs_get_source_by_name(QT_TO_UTF8(name));
		if (!group)
			break;
		name = QTStr("Basic.Main.Group").arg(QString::number(i));
		++i;
	}

	return name;
}

void SourceTreeModel::AddGroup()
{
	QString name = GetNewGroupName();
	obs_sceneitem_t *group =
		obs_scene_add_group(GetCurrentScene(), QT_TO_UTF8(name));
	if (!group)
		return;

	beginInsertRows(QModelIndex(), 0, 0);
	items.insert(0, group);
	endInsertRows();

	st->UpdateWidget(createIndex(0, 0, nullptr), group);
	UpdateGroupState(true);

	QMetaObject::invokeMethod(st, "Edit", Qt::QueuedConnection,
				  Q_ARG(int, 0));
}

void SourceTreeModel::GroupSelectedItems(QModelIndexList &indices)
{
	if (indices.count() == 0)
		return;

	OBSScene scene = GetCurrentScene();
	QString name = GetNewGroupName();

	QVector<obs_sceneitem_t *> item_order;

	for (int i = indices.count() - 1; i >= 0; i--) {
		obs_sceneitem_t *item = items[indices[i].row()];
		item_order << item;
	}

	auto main = OBSBasic::Get();
	st->undoSceneData = main->BackupScene(scene);

	obs_sceneitem_t *item = obs_scene_insert_group(
		scene, QT_TO_UTF8(name), item_order.data(), item_order.size());
	if (!item) {
		st->undoSceneData = nullptr;
		return;
	}

	main->undo_s.push_disabled();

	for (obs_sceneitem_t *item : item_order)
		obs_sceneitem_select(item, false);

	int newIdx = indices[0].row();
	/*beginInsertRows(QModelIndex(), newIdx, newIdx);
	items.insert(newIdx, item);
	endInsertRows();

	for (int i = 0, count = indices.size(); i < count; i++) {
		int fromIdx = indices[i].row() + 1;
		int toIdx = newIdx + i + 1;
		if (fromIdx != toIdx) {
			beginMoveRows(QModelIndex(), fromIdx, fromIdx,
				      QModelIndex(), toIdx);
			MoveItem(items, fromIdx, toIdx);
			endMoveRows();
		}
	}*/

	hasGroups = true;
	st->UpdateWidgets(true);

	obs_sceneitem_select(item, true);

	/* ----------------------------------------------------------------- */
	/* obs_scene_insert_group triggers a full refresh of scene items via */
	/* the item_add signal. No need to insert a row, just edit the one   */
	/* that's created automatically.                                     */

	QMetaObject::invokeMethod(st, "NewGroupEdit", Qt::QueuedConnection,
				  Q_ARG(int, newIdx));
}

static bool enumDshowItem(obs_scene_t *, obs_sceneitem_t *item, void *param)
{
	QVector<OBSSceneItem> &items =
		*static_cast<QVector<OBSSceneItem> *>(param);
	const obs_source_t *source = obs_sceneitem_get_source(item);
	if (!source) {
		return true;
	}
	const char *id = obs_source_get_id(source);
	if (!id) {
		return true;
	}

	if (0 != strcmp(id, BGM_SOURCE_ID)) {
		return true;
	}

	items.push_back(item);

	return true;
}

void SourceTreeModel::UngroupSelectedGroups(QModelIndexList &indices)
{
	OBSBasic *main = OBSBasic::Get();
	if (indices.count() == 0)
		return;

	OBSScene scene = main->GetCurrentScene();
	OBSData undoData = main->BackupScene(scene);

	for (int i = indices.count() - 1; i >= 0; i--) {
		obs_sceneitem_t *item = items[indices[i].row()];
		// scene item was released when ungroup
		QVector<OBSSceneItem> itemsList;
		if (obs_sceneitem_is_group(item)) {
			obs_sceneitem_group_enum_items(item, enumDshowItem,
						       &itemsList);
			if (!itemsList.empty()) {
				emit itemRemoves(itemsList);
			}
		}
		pls_sceneitem_group_ungroup(item);
	}

	SceneChanged();

	OBSData redoData = main->BackupScene(scene);
	main->CreateSceneUndoRedoAction(QTStr("Basic.Main.Ungroup"), undoData,
					redoData);
}

void SourceTreeModel::ExpandGroup(obs_sceneitem_t *item)
{
	int itemIdx = items.indexOf(item);
	if (itemIdx == -1)
		return;

	itemIdx++;

	obs_scene_t *scene = obs_sceneitem_group_get_scene(item);

	QVector<OBSSceneItem> subItems;
	obs_scene_enum_items(scene, enumItem, &subItems);

	if (!subItems.size())
		return;

	beginInsertRows(QModelIndex(), itemIdx, itemIdx + subItems.size() - 1);
	for (int i = 0, size = subItems.size(); i < size; i++)
		items.insert(i + itemIdx, subItems[i]);
	endInsertRows();

	st->UpdateWidgets();
}

void SourceTreeModel::CollapseGroup(const obs_sceneitem_t *item)
{
	int startIdx = -1;
	int endIdx = -1;

	const obs_scene_t *scene = obs_sceneitem_group_get_scene(item);

	for (int i = 0; i < items.size(); i++) {
		const obs_scene_t *itemScene =
			obs_sceneitem_get_scene(items[i]);

		if (itemScene == scene) {
			if (startIdx == -1)
				startIdx = i;
			endIdx = i;
		}
	}

	if (startIdx == -1)
		return;

	beginRemoveRows(QModelIndex(), startIdx, endIdx);
	items.remove(startIdx, endIdx - startIdx + 1);
	endRemoveRows();
}

void SourceTreeModel::UpdateGroupState(bool update)
{
	bool nowHasGroups = false;
	for (const auto &item : items) {
		if (obs_sceneitem_is_group(item)) {
			nowHasGroups = true;
			break;
		}
	}

	if (nowHasGroups != hasGroups) {
		hasGroups = nowHasGroups;
		if (update) {
			st->UpdateWidgets(true);
		}
	}
}

/* ========================================================================= */

SourceTree::SourceTree(QWidget *parent_) : QListView(parent_)
{
	SourceTreeModel *stm_ = new SourceTreeModel(this);
	connect(stm_, &SourceTreeModel::itemRemoves, this,
		[this](QVector<OBSSceneItem> items) {
			emit itemsRemove(items);
		});
	connect(stm_, &SourceTreeModel::itemReorder, this,
		[this] { emit itemsReorder(); });
	setModel(stm_);

	pls_add_css(this, {"PLSSource", "VisibilityCheckBox"});

	scrollBar = pls_new<QSourceScrollBar>(this);
	setVerticalScrollBar(scrollBar);
	setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

	setVerticalScrollMode(ScrollPerPixel);
	setMouseTracking(true);

	noSourceTips = pls_new<QLabel>(this);
	noSourceTips->setObjectName(NO_SOURCE_TEXT_LABEL);
	noSourceTips->hide();

	//UpdateNoSourcesMessage();
	/*connect(App(), &OBSApp::StyleChanged, this,
		&SourceTree::UpdateNoSourcesMessage);*/
	connect(App(), &OBSApp::StyleChanged, this, &SourceTree::UpdateIcons);

	setItemDelegate(new SourceTreeDelegate(this));
	setStyle(new SourceTreeProxyStyle);
}

void SourceTree::UpdateIcons() const
{
	SourceTreeModel *stm = GetStm();
	stm->SceneChanged();
}

void SourceTree::SetIconsVisible(bool visible)
{
	SourceTreeModel *stm = GetStm();

	iconsVisible = visible;
	stm->SceneChanged();
}

int SourceTree::Count() const
{
	SourceTreeModel *stm = GetStm();
	return stm->Count();
}

QVector<OBSSceneItem> SourceTree::GetItems() const
{
	SourceTreeModel *stm = GetStm();
	return stm->GetItems();
}

void SourceTree::ResetDragOver()
{
	if (!preDragOver)
		return;

	int cnt = GetStm()->Count();
	for (int i = 0; i < cnt; ++i) {
		const SourceTreeItem *item = GetItemWidget(i);
		if (item == preDragOver) {
			preDragOver->UpdateIndicator(
				SourceTreeItem::IndicatorType::IndicatorNormal);
			break;
		}
	}

	preDragOver = nullptr;
}

bool SourceTree::GetDestGroupItem(QPoint pos,
				  obs_sceneitem_t *&item_output) const
{
	OBSScene scene = GetCurrentScene();
	SourceTreeModel *stm = GetStm();
	auto &items = stm->items;
	QModelIndexList indices = selectedIndexes();

	DropIndicatorPosition indicator = dropIndicatorPosition();
	int row = indexAt(pos).row();
	bool emptyDrop = row == -1;

	if (emptyDrop) {
		if (!items.size()) {
			return false;
		}

		row = items.size() - 1;
		indicator = QAbstractItemView::BelowItem;
	}

	/* --------------------------------------- */
	/* store destination group if moving to a  */
	/* group                                   */

	obs_sceneitem_t *dropItem = items[row]; /* item being dropped on */
	bool itemIsGroup = obs_sceneitem_is_group(dropItem);

	obs_sceneitem_t *dropGroup =
		itemIsGroup ? dropItem
			    : obs_sceneitem_get_group(scene, dropItem);

	/* not a group if moving above the group */
	if (indicator == QAbstractItemView::AboveItem && itemIsGroup) {
		dropGroup = nullptr;
	}
	if (emptyDrop)
		dropGroup = nullptr;

	/* --------------------------------------- */
	/* remember to remove list items if        */
	/* dropping on collapsed group             */

	if (dropGroup) {
		obs_data_t *data =
			obs_sceneitem_get_private_settings(dropGroup);
		obs_data_release(data);
	}

	if (indicator == QAbstractItemView::BelowItem ||
	    indicator == QAbstractItemView::OnItem ||
	    indicator == QAbstractItemView::OnViewport)
		row++;

	if (row < 0 || row > stm->items.count()) {
		return false;
	}

	/* --------------------------------------- */
	/* determine if any base group is selected */

	bool hasGroups = false;
	for (const auto &index : indices) {
		obs_sceneitem_t *item = items[index.row()];
		if (obs_sceneitem_is_group(item)) {
			hasGroups = true;
			break;
		}
	}

	/* --------------------------------------- */
	/* if dropping a group, detect if it's     */
	/* below another group                     */

	obs_sceneitem_t *itemBelow;
	if (row == stm->items.count())
		itemBelow = nullptr;
	else
		itemBelow = stm->items[row];

	if (hasGroups) {
		if (!itemBelow ||
		    obs_sceneitem_get_group(scene, itemBelow) != dropGroup) {
			dropGroup = nullptr;
		}
	}

	item_output = dropGroup;
	return true;
}

struct CheckChildHelper {
	obs_source_t *destChild;
	bool includeChild;
};

static bool enumChildInclude(obs_scene_t *, obs_sceneitem_t *item, void *ptr)
{
	auto helper = (CheckChildHelper *)ptr;

	const obs_source_t *source = obs_sceneitem_get_source(item);
	if (source) {
		if (source == helper->destChild) {
			helper->includeChild = true;
			return false;
		}

		if (obs_sceneitem_is_group(item)) {
			obs_sceneitem_group_enum_items(item, enumChildInclude,
						       ptr);
		} else {
			const char *id = obs_source_get_id(source);
			if (id && 0 == strcmp(id, SCENE_SOURCE_ID)) {
				obs_scene_t *sceneTemp =
					obs_scene_from_source(source);
				obs_scene_enum_items(sceneTemp,
						     enumChildInclude, ptr);
			}
		}
	}

	return true;
}

// return false : invalid drag
bool SourceTree::CheckDragSceneToGroup(
	const obs_sceneitem_t *dragItem,
	const obs_sceneitem_t *destGroupItem) const
{
	assert(destGroupItem);
	obs_source_t *groupSource = obs_sceneitem_get_source(destGroupItem);
	if (!groupSource) {
		return true;
	}

	if (!dragItem) {
		return false;
	}

	const obs_source_t *dragSource = obs_sceneitem_get_source(dragItem);
	if (!dragSource) {
		return false;
	}

	const char *id = obs_source_get_id(dragSource);
	if (!id) {
		return false;
	}

	if (0 != strcmp(id, SCENE_SOURCE_ID)) {
		return true;
	}

	CheckChildHelper helper;
	helper.destChild = groupSource;
	helper.includeChild = false;

	obs_scene_t *dragScene = obs_scene_from_source(dragSource);
	obs_scene_enum_items(dragScene, enumChildInclude, &helper);

	if (helper.includeChild) {
		return false;
	} else {
		return true;
	}
}

bool SourceTree::IsValidDrag(obs_sceneitem_t *destGroupItem,
			     QVector<OBSSceneItem> items) const
{
	std::map<obs_sceneitem_t *, std::pair<obs_sceneitem_t *, bool>>
		selectedSource;

	ChildAndParent childParents;
	childParents.parent = std::make_pair(nullptr, true);
	obs_scene_enum_items(GetCurrentScene(), enumItemForParent,
			     &childParents);

	QModelIndexList indexs = selectedIndexes();
	size_t indexsCount = indexs.size();
	for (unsigned i = 0; i < (unsigned)indexsCount; i++) {
		obs_sceneitem_t *item = items[indexs[i].row()];
		selectedSource.insert(std::make_pair(
			item, childParents.childAndParent[item]));
	}

	bool isDragValid = true;
	bool isDestGroup = obs_sceneitem_is_group(destGroupItem);
	bool isDestVisible = obs_sceneitem_visible(destGroupItem);

	auto iter = selectedSource.begin();
	for (; iter != selectedSource.end(); ++iter) {
		if (iter->second.first == destGroupItem) {
			continue;
		}

		if (iter->second.first != nullptr &&
		    obs_sceneitem_is_group(iter->second.first) &&
		    !iter->second.second) {
			isDragValid =
				false; // cann't drag out of invisible group
			break;
		}

		if (destGroupItem && isDestGroup) {
			if (!isDestVisible) {
				isDragValid =
					false; // cann't drag into invisible group
				break;
			}

			if (!CheckDragSceneToGroup(iter->first,
						   destGroupItem)) {
				isDragValid =
					false; // cann't drag scene into group which is included in scene
				break;
			}
		}
	}

	return isDragValid;
}

void SourceTree::ResetWidgets()
{
	OBSScene scene = GetCurrentScene();

	SourceTreeModel *stm = GetStm();
	stm->UpdateGroupState(false);

	for (int i = 0; i < stm->items.count(); i++) {
		QModelIndex index = stm->createIndex(i, 0, nullptr);
		auto uiItem = pls_new<SourceTreeItem>(this, stm->items[i]);
		connect(uiItem, &SourceTreeItem::SelectItemChanged, this,
			&SourceTree::OnSelectItemChanged, Qt::QueuedConnection);
		connect(uiItem, &SourceTreeItem::VisibleItemChanged, this,
			&SourceTree::OnVisibleItemChanged,
			Qt::QueuedConnection);
		setIndexWidget(index, uiItem);
	}
}

void SourceTree::UpdateWidget(const QModelIndex &idx, obs_sceneitem_t *item)
{
	auto uiItem = pls_new<SourceTreeItem>(this, item);
	connect(uiItem, &SourceTreeItem::SelectItemChanged, this,
		&SourceTree::OnSelectItemChanged, Qt::QueuedConnection);
	connect(uiItem, &SourceTreeItem::VisibleItemChanged, this,
		&SourceTree::OnVisibleItemChanged, Qt::QueuedConnection);
	setIndexWidget(idx, uiItem);
}

void SourceTree::UpdateWidgets(bool force)
{
	SourceTreeModel *stm = GetStm();

	for (int i = 0; i < stm->items.size(); i++) {
		obs_sceneitem_t *item = stm->items[i];
		SourceTreeItem *widget = GetItemWidget(i);

		if (!widget) {
			UpdateWidget(stm->createIndex(i, 0), item);
		} else {
			widget->Update(force);
		}
	}
}

void SourceTree::NotifyItemSelect(obs_sceneitem_t *sceneitem, bool select)
{
	int cnt = GetStm()->Count();
	for (int i = 0; i < cnt; ++i) {
		SourceTreeItem *item = GetItemWidget(i);
		if (item && item->SceneItem() == sceneitem) {
			item->OnSelectChanged(select);
			break;
		}
	}
}

void SourceTree::SelectItem(obs_sceneitem_t *sceneitem, bool select)
{
	SourceTreeModel *stm = GetStm();
	int i = 0;

	for (; i < stm->items.count(); i++) {
		if (stm->items[i] == sceneitem)
			break;
	}

	if (i == stm->items.count())
		return;

	NotifyItemSelect(sceneitem, select);

	QModelIndex index = stm->createIndex(i, 0);
	if (index.isValid())
		selectionModel()->select(
			index, select ? QItemSelectionModel::Select
				      : QItemSelectionModel::Deselect);
}

Q_DECLARE_METATYPE(OBSSceneItem);

void SourceTree::mouseDoubleClickEvent(QMouseEvent *event)
{
	if (event->button() == Qt::LeftButton)
		QListView::mouseDoubleClickEvent(event);
}

bool TravelGroupChilds(obs_scene_t *, obs_sceneitem_t *item, void *val)
{
	const obs_source_t *source = obs_sceneitem_get_source(item);
	const char *id = obs_source_get_id(source);
	QString actionID = action::GetActionSourceID(id);

	if (!actionID.isEmpty()) {
		auto output = (std::vector<QString> *)val;
		output->push_back(actionID);
	}

	return true;
}

void SourceTree::dragEnterEvent(QDragEnterEvent *event)
{
	if (event->source() == this)
		ResetDragOver();

	QListView::dragEnterEvent(event);
}

void SourceTree::dragLeaveEvent(QDragLeaveEvent *event)
{
	ResetDragOver();
	QListView::dragLeaveEvent(event);
}

bool SourceTree::checkDragBreak(const QDragMoveEvent *event, int row,
				const DropIndicatorPosition &indicator)
{
	const SourceTreeItem *item = GetItemWidget(row);
	int itemY = item->mapTo(this, QPoint(0, 0)).y();
	int itemH = item->size().height();
	int cursorY = event->pos().y();

	// Sometimes indicator can't match current position of cursor, which will cause UI refresh

	if (indicator == QAbstractItemView::AboveItem &&
	    (cursorY > itemY + itemH / 2))
		return true;

	if (indicator == QAbstractItemView::BelowItem &&
	    (cursorY < itemY + itemH / 2))
		return true;

	return false;
}

void SourceTree::dragMoveEvent(QDragMoveEvent *event)
{
	do {
		if (event->source() != this)
			break;

		const auto &items = GetStm()->items;
		if (!items.size())
			break;

		obs_sceneitem_t *destGroupItem = nullptr;
		if (!GetDestGroupItem(event->pos(), destGroupItem)) {
			break;
		}

		if (!IsValidDrag(destGroupItem, items)) {
			ResetDragOver();
			QListView::dragMoveEvent(event);
			event->ignore();
			return;
		}

		DropIndicatorPosition indicator = dropIndicatorPosition();
		int row = indexAt(event->pos()).row();
		if (row == -1) {
			row = items.size() - 1;
			indicator = QAbstractItemView::BelowItem;
		} else {
			if (checkDragBreak(event, row, indicator))
				break;
		}

		SourceTreeItem *item = GetItemWidget(row);
		if (item != preDragOver) {
			ResetDragOver();
			preDragOver = item;
		}

		switch (indicator) {
		case QAbstractItemView::AboveItem:
			preDragOver->UpdateIndicator(
				SourceTreeItem::IndicatorType::IndicatorAbove);
			break;

		case QAbstractItemView::OnItem:
		case QAbstractItemView::BelowItem:
			preDragOver->UpdateIndicator(
				SourceTreeItem::IndicatorType::IndicatorBelow);
			break;

		default:
			break;
		}
	} while (false);

	QListView::dragMoveEvent(event);
}

void SourceTree::dropEventHasGroups(const SourceTreeModel *stm,
				    QModelIndexList &indices,
				    const OBSScene &scene,
				    const QVector<OBSSceneItem> &items) const
{ /* remove sub-items if selected */
	for (int i = indices.size() - 1; i >= 0; i--) {
		const obs_sceneitem_t *item = items[indices[i].row()];
		const obs_scene_t *itemScene = obs_sceneitem_get_scene(item);

		if (itemScene != scene) {
			indices.removeAt(i);
		}
	}

	/* add all sub-items of selected groups */
	for (int i = indices.size() - 1; i >= 0; i--) {
		obs_sceneitem_t *item = items[indices[i].row()];

		if (!obs_sceneitem_is_group(item)) {
			continue;
		}

		for (int j = items.size() - 1; j >= 0; j--) {
			obs_sceneitem_t *subitem = items[j];
			const obs_sceneitem_t *subitemGroup =
				obs_sceneitem_get_group(scene, subitem);

			if (subitemGroup == item) {
				QModelIndex idx = stm->createIndex(j, 0);
				indices.insert(i + 1, idx);
			}
		}
	}
}

template<typename TnsertLastGroup>
static void
dropEventUpdateScene(bool hasGroups, const OBSScene &scene,
		     obs_sceneitem_t *&lastGroup,
		     const TnsertLastGroup &insertLastGroup,
		     const QVector<OBSSceneItem> &items,
		     obs_sceneitem_t *dropGroup, int firstIdx, int lastIdx,
		     QVector<struct obs_sceneitem_order_info> &orderList)
{
	struct obs_sceneitem_order_info info;

	for (int i = 0; i < items.size(); i++) {
		obs_sceneitem_t *item = items[i];
		obs_sceneitem_t *group;

		if (obs_sceneitem_is_group(item)) {
			if (lastGroup) {
				insertLastGroup();
			}
			lastGroup = item;
			continue;
		}

		if (!hasGroups && i >= firstIdx && i <= lastIdx)
			group = dropGroup;
		else
			group = obs_sceneitem_get_group(scene, item);

		if (lastGroup && lastGroup != group) {
			insertLastGroup();
		}

		lastGroup = group;

		info.group = group;
		info.item = item;
		orderList.insert(0, info);
	}

	if (lastGroup) {
		insertLastGroup();
	}

	obs_scene_reorder_items2(scene, orderList.data(), orderList.size());
}

void SourceTree::dropEventPersistentIndicesForeachCb(
	int &r, SourceTreeModel *stm, QVector<OBSSceneItem> &items,
	const QPersistentModelIndex &persistentIdx) const
{
	int from = persistentIdx.row();
	int to = r;
	int itemTo = to;

	if (itemTo > from)
		itemTo--;

	if (itemTo != from) {
		stm->beginMoveRows(QModelIndex(), from, from, QModelIndex(),
				   to);
		MoveItem(items, from, itemTo);
		stm->endMoveRows();
	}

	r = persistentIdx.row() + 1;
}

void SourceTree::mouseMoveEvent(QMouseEvent *event)
{
	QPoint pos = event->pos();
	const SourceTreeItem *item =
		qobject_cast<SourceTreeItem *>(childAt(pos));

	OBSBasicPreview *preview = OBSBasicPreview::Get();

	QListView::mouseMoveEvent(event);

	std::lock_guard<std::mutex> lock(preview->selectMutex);
	preview->hoveredPreviewItems.clear();
	if (item)
		preview->hoveredPreviewItems.push_back(item->sceneitem);
}

void SourceTree::dropEvent(QDropEvent *event)
{
	if (event->source() != this) {
		QListView::dropEvent(event);
		return;
	}

	ResetDragOver();

	OBSBasic *main = OBSBasic::Get();

	OBSScene scene = GetCurrentScene();
	obs_source_t *scenesource = obs_scene_get_source(scene);
	SourceTreeModel *stm = GetStm();
	auto &items = stm->items;
	QModelIndexList indices = selectedIndexes();

	DropIndicatorPosition indicator = dropIndicatorPosition();
	int row = indexAt(
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
			  event->position().toPoint()
#else
			  event->pos()
#endif
				  )
			  .row();
	bool emptyDrop = row == -1;

	if (emptyDrop) {
		if (!items.size()) {
			QListView::dropEvent(event);
			return;
		}

		row = items.size() - 1;
		indicator = QAbstractItemView::BelowItem;
	}

	/* --------------------------------------- */
	/* store destination group if moving to a  */
	/* group                                   */

	obs_sceneitem_t *dropItem = items[row]; /* item being dropped on */
	bool itemIsGroup = obs_sceneitem_is_group(dropItem);

	obs_sceneitem_t *dropGroup =
		itemIsGroup ? dropItem
			    : obs_sceneitem_get_group(scene, dropItem);

	std::vector<QString> srcChilds;
	std::vector<QString> destChilds;

	if (dropGroup && obs_sceneitem_is_group(dropGroup))
		obs_sceneitem_group_enum_items(dropGroup, TravelGroupChilds,
					       (void *)&srcChilds);

	/* not a group if moving above the group */
	if (indicator == QAbstractItemView::AboveItem && itemIsGroup)
		dropGroup = nullptr;
	if (emptyDrop)
		dropGroup = nullptr;

	/* --------------------------------------- */
	/* remember to remove list items if        */
	/* dropping on collapsed group             */

	bool dropOnCollapsed = false;
	if (dropGroup) {
		obs_data_t *data =
			obs_sceneitem_get_private_settings(dropGroup);
		dropOnCollapsed = obs_data_get_bool(data, "collapsed");
		obs_data_release(data);
	}

	if (indicator == QAbstractItemView::BelowItem ||
	    indicator == QAbstractItemView::OnItem ||
	    indicator == QAbstractItemView::OnViewport)
		row++;

	if (row < 0 || row > stm->items.count()) {
		QListView::dropEvent(event);
		return;
	}

	/* --------------------------------------- */
	/* determine if any base group is selected */

	bool hasGroups = false;
	for (int i = 0; i < indices.size(); i++) {
		obs_sceneitem_t *item = items[indices[i].row()];
		if (obs_sceneitem_is_group(item)) {
			hasGroups = true;
			break;
		}
	}

	/* --------------------------------------- */
	/* if dropping a group, detect if it's     */
	/* below another group                     */

	obs_sceneitem_t *itemBelow;
	if (row == stm->items.count())
		itemBelow = nullptr;
	else
		itemBelow = stm->items[row];

	if (hasGroups) {
		if (!itemBelow ||
		    obs_sceneitem_get_group(scene, itemBelow) != dropGroup) {
			dropGroup = nullptr;
			dropOnCollapsed = false;
		}
	}

	if (!IsValidDrag(dropGroup, items)) {
		PLS_INFO(SOURCE_MODULE,
			 "There is an invalid drag in dropEvent");
		QListView::dropEvent(event);
		return;
	}

	/* --------------------------------------- */
	/* if dropping groups on other groups,     */
	/* disregard as invalid drag/drop          */

	if (dropGroup && hasGroups) {
		QListView::dropEvent(event);
		return;
	}

	/* --------------------------------------- */
	/* save undo data                          */
	std::vector<obs_source_t *> sources;
	for (int i = 0; i < indices.size(); i++) {
		obs_sceneitem_t *item = items[indices[i].row()];
		if (obs_sceneitem_get_scene(item) != scene)
			sources.push_back(obs_scene_get_source(
				obs_sceneitem_get_scene(item)));
	}
	if (dropGroup)
		sources.push_back(obs_sceneitem_get_source(dropGroup));
	OBSData undo_data = main->BackupScene(scene, &sources);

	/* --------------------------------------- */
	/* if selection includes base group items, */
	/* include all group sub-items and treat   */
	/* them all as one                         */

	if (hasGroups) {
		dropEventHasGroups(stm, indices, scene, items);
	}

	/* --------------------------------------- */
	/* build persistent indices                */

	QList<QPersistentModelIndex> persistentIndices;
	persistentIndices.reserve(indices.count());
	for (QModelIndex &index : indices)
		persistentIndices.append(index);
	std::sort(persistentIndices.begin(), persistentIndices.end());

	/* --------------------------------------- */
	/* move all items to destination index     */

	int r = row;
	std::for_each(persistentIndices.begin(), persistentIndices.end(),
		      [this, &r, stm,
		       &items](const QPersistentModelIndex &persistentIdx) {
			      dropEventPersistentIndicesForeachCb(
				      r, stm, items, persistentIdx);
		      });

	std::sort(persistentIndices.begin(), persistentIndices.end());
	int firstIdx = persistentIndices.front().row();
	int lastIdx = persistentIndices.back().row();

	/* --------------------------------------- */
	/* reorder scene items in back-end         */

	QVector<struct obs_sceneitem_order_info> orderList;
	obs_sceneitem_t *lastGroup = nullptr;
	int insertCollapsedIdx = 0;

	auto insertCollapsed = [&orderList, &insertCollapsedIdx,
				&lastGroup](obs_sceneitem_t *item) {
		struct obs_sceneitem_order_info info;
		info.group = lastGroup;
		info.item = item;

		orderList.insert(insertCollapsedIdx++, info);
	};

	using insertCollapsed_t = decltype(insertCollapsed);

	auto preInsertCollapsed = [](obs_scene_t *, obs_sceneitem_t *item,
				     void *param) {
		(*reinterpret_cast<insertCollapsed_t *>(param))(item);
		return true;
	};

	auto insertLastGroup = [&]() {
		OBSDataAutoRelease data =
			obs_sceneitem_get_private_settings(lastGroup);
		bool collapsed = obs_data_get_bool(data, "collapsed");

		if (collapsed) {
			insertCollapsedIdx = 0;
			obs_sceneitem_group_enum_items(lastGroup,
						       preInsertCollapsed,
						       &insertCollapsed);
		}

		struct obs_sceneitem_order_info info;
		info.group = nullptr;
		info.item = lastGroup;
		orderList.insert(0, info);
	};

	auto updateScene = [&hasGroups, &scene, &lastGroup, &insertLastGroup,
			    &items, &dropGroup, &firstIdx, &lastIdx,
			    &orderList]() {
		dropEventUpdateScene(hasGroups, scene, lastGroup,
				     insertLastGroup, items, dropGroup,
				     firstIdx, lastIdx, orderList);
	};

	using updateScene_t = decltype(updateScene);

	auto preUpdateScene = [](void *data, obs_scene_t *) {
		(*reinterpret_cast<updateScene_t *>(data))();
	};

	ignoreReorder = true;
	obs_scene_atomic_update(scene, preUpdateScene, &updateScene);
	ignoreReorder = false;

	/* --------------------------------------- */
	/* save redo data                          */

	OBSData redo_data = main->BackupScene(scene, &sources);

	/* --------------------------------------- */
	/* add undo/redo action                    */

	const char *scene_name = obs_source_get_name(scenesource);
	QString action_name = QTStr("Undo.ReorderSources").arg(scene_name);
	main->CreateSceneUndoRedoAction(action_name, undo_data, redo_data);

	/* --------------------------------------- */
	/* remove items if dropped in to collapsed */
	/* group                                   */

	if (dropOnCollapsed) {
		stm->beginRemoveRows(QModelIndex(), firstIdx, lastIdx);
		items.remove(firstIdx, lastIdx - firstIdx + 1);
		stm->endRemoveRows();
	}

	/* --------------------------------------- */
	/* update widgets and accept event         */

	UpdateWidgets(true);

	event->accept();
	event->setDropAction(Qt::CopyAction);

	if (pls_chk_ptr_invoke(obs_sceneitem_is_group, dropGroup)) {
		obs_sceneitem_group_enum_items(dropGroup, TravelGroupChilds,
					       (void *)&destChilds);
		if (srcChilds.size() != destChilds.size())
			action::OnGroupChildAdded(destChilds);
	}

	emit itemsReorder();
	QListView::dropEvent(event);
}

void SourceTree::leaveEvent(QEvent *event)
{
	OBSBasicPreview *preview = OBSBasicPreview::Get();

	QListView::leaveEvent(event);

	std::lock_guard<std::mutex> lock(preview->selectMutex);
	preview->hoveredPreviewItems.clear();
}

void SourceTree::selectionChanged(const QItemSelection &selected,
				  const QItemSelection &deselected)
{
	{
		SignalBlocker sourcesSignalBlocker(this);
		SourceTreeModel *stm = GetStm();

		QModelIndexList selectedIdxs = selected.indexes();
		QModelIndexList deselectedIdxs = deselected.indexes();

		for (int i = 0; i < selectedIdxs.count(); i++) {
			int idx = selectedIdxs[i].row();
			obs_sceneitem_select(stm->items[idx], true);
		}

		for (int i = 0; i < deselectedIdxs.count(); i++) {
			int idx = deselectedIdxs[i].row();
			obs_sceneitem_select(stm->items[idx], false);
		}
	}
	QListView::selectionChanged(selected, deselected);
}

void SourceTree::NewGroupEdit(int row)
{
	if (!Edit(row)) {
		OBSBasic *main = OBSBasic::Get();
		main->undo_s.pop_disabled();

		blog(LOG_WARNING, "Uh, somehow the edit didn't process, this "
				  "code should never be reached.\nAnd by "
				  "\"never be reached\", I mean that "
				  "theoretically, it should be\nimpossible "
				  "for this code to be reached. But if this "
				  "code is reached,\nfeel free to laugh at "
				  "Jim, because apparently it is, in fact, "
				  "actually\npossible for this code to be "
				  "reached. But I mean, again, theoretically\n"
				  "it should be impossible. So if you see "
				  "this in your log, just know that\nit's "
				  "really dumb, and depressing. But at least "
				  "the undo/redo action is\nstill covered, so "
				  "in theory things *should* be fine. But "
				  "it's entirely\npossible that they might "
				  "not be exactly. But again, yea. This "
				  "really\nshould not be possible.");

		OBSData redoSceneData = main->BackupScene(GetCurrentScene());

		QString text = QTStr("Undo.GroupItems").arg("Unknown");
		main->CreateSceneUndoRedoAction(text, undoSceneData,
						redoSceneData);

		undoSceneData = nullptr;
	}
}

bool SourceTree::Edit(int row)
{
	SourceTreeModel *stm = GetStm();
	if (row < 0 || row >= stm->items.count())
		return false;

	QModelIndex index = stm->createIndex(row, 0);
	QWidget *widget = indexWidget(index);
	SourceTreeItem *itemWidget = reinterpret_cast<SourceTreeItem *>(widget);
	if (itemWidget->IsEditing()) {
#ifdef __APPLE__
		itemWidget->ExitEditMode(true);
#endif
		return false;
	}

	itemWidget->EnterEditMode();
	edit(index);
	return true;
}

void SourceTree::OnSourceItemRemove(unsigned long long sceneItemPtr) const
{
	GetStm()->Remove((void *)sceneItemPtr);
}

void SourceTree::OnSelectItemChanged(OBSSceneItem item, bool selected)
{
	emit SelectItemChanged(item, selected);
}

void SourceTree::OnVisibleItemChanged(OBSSceneItem item, bool visible)
{
	emit VisibleItemChanged(item, visible);
}

bool SourceTree::MultipleBaseSelected() const
{
	SourceTreeModel *stm = GetStm();
	QModelIndexList selectedIndices = selectedIndexes();

	OBSScene scene = GetCurrentScene();

	if (selectedIndices.size() < 1) {
		return false;
	}

	for (auto &idx : selectedIndices) {
		obs_sceneitem_t *item = stm->items[idx.row()];
		if (obs_sceneitem_is_group(item)) {
			return false;
		}

		obs_scene *itemScene = obs_sceneitem_get_scene(item);
		if (itemScene != scene) {
			return false;
		}
	}

	return true;
}

bool SourceTree::GroupsSelected() const
{
	SourceTreeModel *stm = GetStm();
	QModelIndexList selectedIndices = selectedIndexes();

	OBSScene scene = GetCurrentScene();

	if (selectedIndices.size() < 1) {
		return false;
	}

	for (auto &idx : selectedIndices) {
		obs_sceneitem_t *item = stm->items[idx.row()];
		if (!obs_sceneitem_is_group(item)) {
			return false;
		}
	}

	return true;
}

bool SourceTree::GroupedItemsSelected() const
{
	SourceTreeModel *stm = GetStm();
	QModelIndexList selectedIndices = selectedIndexes();
	OBSScene scene = GetCurrentScene();

	if (!selectedIndices.size()) {
		return false;
	}

	for (auto &idx : selectedIndices) {
		obs_sceneitem_t *item = stm->items[idx.row()];
		obs_scene *itemScene = obs_sceneitem_get_scene(item);

		if (itemScene != scene) {
			return true;
		}
	}

	return false;
}

void SourceTree::Remove(OBSSceneItem item) const
{
	OBSBasic *main = reinterpret_cast<OBSBasic *>(App()->GetMainWindow());
	GetStm()->Remove(item);
	main->SaveProject();

	if (!main->SavingDisabled()) {
		obs_scene_t *scene = obs_sceneitem_get_scene(item);
		obs_source_t *sceneSource = obs_scene_get_source(scene);
		obs_source_t *itemSource = obs_sceneitem_get_source(item);
		main->DeletePropertiesWindow(itemSource);
		main->DeleteFiltersWindow(itemSource);
		main->UpdateContextBarDeferred();
		blog(LOG_INFO, "User Removed source '%s' (%s) from scene '%s'",
		     obs_source_get_name(itemSource),
		     obs_source_get_id(itemSource),
		     obs_source_get_name(sceneSource));
	}
}

void SourceTree::GroupSelectedItems() const
{
	PLS_UI_STEP(SOURCE_MODULE, "Group Selected Items", ACTION_CLICK);

	QModelIndexList indices = selectedIndexes();
	std::sort(indices.begin(), indices.end());
	GetStm()->GroupSelectedItems(indices);
}

void SourceTree::UngroupSelectedGroups() const
{
	QModelIndexList indices = selectedIndexes();
	GetStm()->UngroupSelectedGroups(indices);
}

void SourceTree::AddGroup() const
{
	GetStm()->AddGroup();
}

const auto NO_SOURCE_TIPS_MARGIN = 20;
const auto NO_SOURCE_TIPS_BOTTOM_MARGIN = 50;
const auto NO_SOURCE_TIPS_WORD_SPACING = 4;

#if 0
void SourceTree::UpdateNoSourcesMessage()
{
	std::string darkPath;
	GetDataFilePath("themes/Dark/no_sources.svg", darkPath);

	QString file = !App()->IsThemeDark() ? ":res/images/no_sources.svg"
					     : darkPath.c_str();
	iconNoSources.load(file);

	QTextOption opt(Qt::AlignHCenter);
	opt.setWrapMode(QTextOption::WordWrap);
	textNoSources.setTextOption(opt);
	textNoSources.setText(QTStr("Prism.NoSources.Label").replace("\n", "<br/>"));

	textPrepared = false;
}
#endif
void SourceTree::paintEvent(QPaintEvent *event)
{
	const SourceTreeModel *stm = GetStm();
	if (stm && !stm->items.count()) {
		QTextOption option(Qt::AlignHCenter | Qt::AlignVCenter);
		option.setWrapMode(QTextOption::WordWrap);
		int bottomMargin = NO_SOURCE_TIPS_BOTTOM_MARGIN;
		if (this->height() < 100) {
			bottomMargin = NO_SOURCE_TIPS_MARGIN;
		}
		int height = size().height() - bottomMargin;
		int left = NO_SOURCE_TIPS_MARGIN;
		int width = size().width() - 2 * NO_SOURCE_TIPS_MARGIN;

		QPainter p(viewport());
		p.setPen(QColor(102, 102, 102));
		p.setFont(noSourceTips->font());

		QFontMetrics metrics(noSourceTips->font());
		int maxContentWidth = (height / (metrics.height() +
						 NO_SOURCE_TIPS_WORD_SPACING)) *
				      width;
		p.drawText(QRect(left, 0, width, height),
			   metrics.elidedText(QTStr("Prism.NoSources.Label"),
					      Qt::ElideRight, maxContentWidth),
			   option);

	} else {
		QListView::paintEvent(event);
	}
}

SourceTreeDelegate::SourceTreeDelegate(QObject *parent)
	: QStyledItemDelegate(parent)
{
}

QSize SourceTreeDelegate::sizeHint(const QStyleOptionViewItem &option,
				   const QModelIndex &index) const
{
	SourceTree *tree = qobject_cast<SourceTree *>(parent());
	QWidget *item = tree->indexWidget(index);

	if (!item)
		return (QSize(0, 0));

	return (QSize(option.widget->minimumWidth(), item->height()));
}
