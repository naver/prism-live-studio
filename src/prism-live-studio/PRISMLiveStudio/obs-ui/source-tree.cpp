#include "window-basic-main.hpp"
#include "obs-app.hpp"
#include "source-tree.hpp"
#include "platform.hpp"
#include "pls-common-define.hpp"
#include "PLSAction.h"
#include "action.h"
#include "liblog.h"
#include "log/module_names.h"
#include "PLSBasic.h"
#include "ChannelCommonFunctions.h"
#include "source-label.hpp"
#include <qt-wrappers.hpp>
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
#include <QSet>
#include "pls/pls-source.h"
#include "pls/pls-dual-output.h"
#include "PLSSceneitemMapManager.h"
#include "libutils-api.h"
#if defined(Q_OS_MACOS)
#include "mac/PLSPermissionHelper.h"
#endif

using namespace common;

static inline OBSScene GetCurrentScene()
{
	OBSBasic *main = reinterpret_cast<OBSBasic *>(App()->GetMainWindow());
	return main->GetCurrentScene();
}

const auto SOURCEITEM_MARGIN_UNNORMAL_LONG = 30;  // while scroll is hiden and mouse status is unnormal
const auto SOURCEITEM_MARGIN_UNNORMAL_SHORT = 15; // while scroll is shown and mouse status is unnormal

const auto SOURCEITEM_MARGIN_NORMAL_LONG = 20;  // while scroll is hiden and mouse status is normal
const auto SOURCEITEM_MARGIN_NORMAL_SHORT = 10; // while scroll is shown and mouse status is normal

const auto SOURCEITEM_SPACE_BEFORE_VIS = 5;
const auto SOURCEITEM_SPACE_BEFORE_LOCK = 10;
const auto SOURCEITEM_SPACE_AFTER_HOR_VIS = 10;
const auto SOURCEITEM_SPACE_AFTER_VER_VIS = 10;

/* ========================================================================= */
SourceTreeItem::SourceTreeItem(SourceTree *tree_, OBSSceneItem sceneitem_)
	: tree(tree_),
	  sceneitem(sceneitem_),
	  isScrollShowed(tree_->scrollBar->isVisible())
{
	setAttribute(Qt::WA_TranslucentBackground);
	setMouseTracking(true);
	setProperty("showHandCursor", true);

	connect(tree_->scrollBar, &QSourceScrollBar::SourceScrollShow, this, &SourceTreeItem::OnSourceScrollShow);
	obs_source_t *source = obs_sceneitem_get_source(sceneitem);
	const char *name = obs_source_get_name(source);

	OBSBasic *main = reinterpret_cast<OBSBasic *>(App()->GetMainWindow());
	const char *id = obs_source_get_id(source);

	bool sourceVisible = obs_sceneitem_visible(sceneitem);
	bool allVisible = sourceVisible;
	bool verVisible = false;
	if (pls_is_dual_output_on()) {
		auto verItem = PLSSceneitemMapMgrInstance->getVerticalSceneitem(sceneitem);
		verVisible = obs_sceneitem_visible(verItem);
		allVisible = sourceVisible || verVisible;
	}

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
		iconLabel->setEnabled(allVisible);
		iconLabel->setStyleSheet("background: none");
		iconLabel->setProperty("TH_Source_Icon", true);
	}

	// horizontal visible icon
	horVisBtn = new QCheckBox(this);
	horVisBtn->setObjectName("horVisBtnViewCheckbox");
	horVisBtn->setChecked(sourceVisible);
	horVisBtn->setVisible(false);

	// vertical visible icon
	verVisBtn = new QCheckBox(this);
	verVisBtn->setObjectName("verVisBtnViewCheckbox");
	verVisBtn->setChecked(verVisible);
	verVisBtn->setVisible(false);

	vis = new QCheckBox();
	vis->setProperty("visibilityCheckBox", true);
	vis->setObjectName("sourceIconViewCheckbox");
	vis->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	vis->setChecked(allVisible);
	vis->setAccessibleName(QTStr("Basic.Main.Sources.Visibility"));
	vis->setAccessibleDescription(QTStr("Basic.Main.Sources.VisibilityDescription").arg(name));

	lock = new QCheckBox();
	lock->setProperty("lockCheckBox", true);
	lock->setObjectName("sourceLockCheckbox");
	lock->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	lock->setChecked(obs_sceneitem_locked(sceneitem));
	lock->setAccessibleName(QTStr("Basic.Main.Sources.Lock"));
	lock->setAccessibleDescription(QTStr("Basic.Main.Sources.LockDescription").arg(name));

	deleteBtn = new QPushButton();
	deleteBtn->setObjectName("sourceIconDeleteBtn");

	label = new OBSSourceLabel(source);
	connect(label, &OBSSourceLabel::Renamed, this, [=](const char *name) {
		OBSDataAutoRelease data = obs_source_get_private_settings(source);
		auto deviceName = obs_data_get_default_string(data, "deviceName");
		label->appendDeviceName(name, deviceName);
	});
	OBSDataAutoRelease data = obs_source_get_private_settings(source);
	auto deviceName = obs_data_get_default_string(data, "deviceName");
	label->appendDeviceName(name, deviceName);
	label->setObjectName("sourceNameLabel");
	label->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
	label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	label->setAttribute(Qt::WA_TranslucentBackground);
	label->setEnabled(allVisible);

	aboveIndicator = pls_new<QLabel>(this);
	belowIndicator = pls_new<QLabel>(this);

	aboveIndicator->setFixedHeight(1);
	belowIndicator->setFixedHeight(1);
	UpdateIndicator(IndicatorType::IndicatorNormal);

#ifdef __APPLE__
	horVisBtn->setAttribute(Qt::WA_LayoutUsesWidgetRect);
	verVisBtn->setAttribute(Qt::WA_LayoutUsesWidgetRect);
	vis->setAttribute(Qt::WA_LayoutUsesWidgetRect);
	lock->setAttribute(Qt::WA_LayoutUsesWidgetRect);
#endif

	spaceBeforeVis = pls_new<QSpacerItem>(SOURCEITEM_SPACE_BEFORE_VIS, 1);
	spaceBeforeLock = pls_new<QSpacerItem>(SOURCEITEM_SPACE_BEFORE_LOCK, 1);
	rightMargin = pls_new<QSpacerItem>(SOURCEITEM_MARGIN_NORMAL_LONG, 1);
	spaceAfterHorVis = pls_new<QSpacerItem>(SOURCEITEM_SPACE_AFTER_HOR_VIS, 1);
	spaceAfterVerVis = pls_new<QSpacerItem>(SOURCEITEM_SPACE_AFTER_VER_VIS, 1);
	boxLayout = new QHBoxLayout();
	boxLayout->setContentsMargins(18, 0, 0, 0);
	boxLayout->setSpacing(0);
	if (iconLabel) {
		boxLayout->addWidget(iconLabel);
	}
	boxLayout->addWidget(label);
	boxLayout->addItem(spaceBeforeVis);
	boxLayout->addWidget(horVisBtn);
	boxLayout->addItem(spaceAfterHorVis);
	boxLayout->addWidget(verVisBtn);
	boxLayout->addItem(spaceAfterVerVis);
	boxLayout->addWidget(vis);
	boxLayout->addItem(spaceBeforeLock);
	boxLayout->addWidget(lock);
	boxLayout->addSpacing(10);
	boxLayout->addWidget(deleteBtn);
	boxLayout->addItem(rightMargin);

	auto vLayout = pls_new<QVBoxLayout>(this);
	vLayout->setContentsMargins(0, 0, 0, 0);
	vLayout->setSpacing(0);
	vLayout->addWidget(aboveIndicator);
	vLayout->addLayout(boxLayout);
	vLayout->addWidget(belowIndicator);

	OBSDataAutoRelease privData = obs_sceneitem_get_private_settings(sceneitem);
	auto preset = obs_data_get_int(privData, "color-preset");

	if (preset == 1) {
		const char *color = obs_data_get_string(privData, "color");
		SetBgColor(SourceItemBgType::BgCustom,
			   (void *)color); // format of color is like "#5555aa7f"
	} else if (preset > 1) {
		long long presetIndex = (preset - 2); // the index should start with 0
		SetBgColor(SourceItemBgType::BgPreset, (void *)presetIndex);
	} else {
		SetBgColor(SourceItemBgType::BgDefault, nullptr);
	}

	UpdateIcon(allVisible);
	Update(false);

	auto undo_redo = [](const std::string &uuid, int64_t id, bool val, bool all) {
		OBSSourceAutoRelease s = obs_get_source_by_uuid(uuid.c_str());
		obs_scene_t *sc = obs_group_or_scene_from_source(s);
		obs_sceneitem_t *si = obs_scene_find_sceneitem_by_id(sc, id);
		if (si)
			obs_sceneitem_set_visible(si, val);
		if (!all) {
			return;
		}
		if (auto verItem = PLSSceneitemMapMgrInstance->getVerticalSceneitem(si); verItem) {
			obs_sceneitem_set_visible(verItem, val);
		}
	};

	auto addUndoRedo = [undo_redo](obs_scene_item *sceneitem_, bool val, bool all) {
		QString str = QTStr(val ? "Undo.ShowSceneItem" : "Undo.HideSceneItem");
		obs_scene_t *scene = obs_sceneitem_get_scene(sceneitem_);
		obs_source_t *scenesource = obs_scene_get_source(scene);
		int64_t id = obs_sceneitem_get_id(sceneitem_);
		const char *name = obs_source_get_name(scenesource);
		const char *uuid = obs_source_get_uuid(scenesource);
		obs_source_t *source = obs_sceneitem_get_source(sceneitem_);

		//the vertical group name need check when dual output open
		OBSDataAutoRelease priSettings = obs_sceneitem_get_private_settings(sceneitem_);
		auto sourceName = obs_data_get_string(priSettings, SCENE_ITEM_REFERENCE_GROUP_NAME);
		if (pls_is_empty(sourceName)) {
			sourceName = obs_source_get_name(source);
		}
		auto sceneName = obs_data_get_string(priSettings, SCENE_ITEM_MAP_SAVE_SCENE_NAME_KEY);
		OBSBasic *main = OBSBasic::Get();
		main->undo_s.add_action(str.arg(sourceName, !pls_is_empty(sceneName) ? sceneName : name),
					std::bind(undo_redo, std::placeholders::_1, id, !val, all),
					std::bind(undo_redo, std::placeholders::_1, id, val, all), uuid, uuid);
	};

	/* --------------------------------------------------------- */
	auto setItemVisible = [this](obs_scene_item *sceneitem_, bool val) {
		QSignalBlocker sourcesSignalBlocker(this);
		obs_sceneitem_set_visible(sceneitem_, val);
	};
	auto setHorItemVisible = [this, setItemVisible, addUndoRedo](bool val) {
		setItemVisible(sceneitem, val);
		addUndoRedo(sceneitem, val, false);
	};
	auto setVerItemVisible = [this, setItemVisible, addUndoRedo](bool val) {
		auto verItem = PLSSceneitemMapMgrInstance->getVerticalSceneitem(sceneitem);
		if (!verItem) {
			return;
		}
		setItemVisible(verItem, val);
		addUndoRedo(verItem, val, false);
	};
	auto setAllItemVisible = [this, setHorItemVisible, setItemVisible, addUndoRedo](bool val) {
		setItemVisible(sceneitem, val);
		if (auto verItem = PLSSceneitemMapMgrInstance->getVerticalSceneitem(sceneitem); verItem) {
			setItemVisible(verItem, val);
		}
		addUndoRedo(sceneitem, val, true);
	};

	auto setItemLocked = [this](bool checked) {
		QSignalBlocker sourcesSignalBlocker(this);
		obs_sceneitem_set_locked(sceneitem, checked);

		auto item = PLSSceneitemMapMgrInstance->getVerticalSceneitem(sceneitem);
		if (nullptr == item) {
			return;
		}
		obs_sceneitem_set_locked(item, checked);
	};

	auto deleteItem = [this]() {
		OBSBasic *main = OBSBasic::Get();
		if (!main) {
			return;
		}
		main->queryRemoveSourceItem(sceneitem);
	};

	connect(vis, &QAbstractButton::clicked, setAllItemVisible);
	connect(horVisBtn, &QAbstractButton::clicked, setHorItemVisible);
	connect(verVisBtn, &QAbstractButton::clicked, setVerItemVisible);
	connect(lock, &QAbstractButton::clicked, setItemLocked);
	connect(deleteBtn, &QAbstractButton::clicked, deleteItem);
	OnSelectChanged(checkItemSelected(obs_sceneitem_selected(sceneitem_)), sceneitem_);
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
	sigs.clear();
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
		obs_scene_t *curScene = (obs_scene_t *)calldata_ptr(cd, "scene");
		if (curItem == this_->sceneitem) {
			QMetaObject::invokeMethod(this_->tree, "Remove", Qt::QueuedConnection,
						  Q_ARG(OBSSceneItem, curItem), Q_ARG(OBSScene, curScene));
			curItem = nullptr;
			this_->label->clearSignals();
		}
		if (!curItem)
			QMetaObject::invokeMethod(this_, "Clear", Qt::QueuedConnection);
	};

	auto itemVisible = [](void *data, calldata_t *cd) {
		pls_unused(cd);
		auto this_ = static_cast<SourceTreeItem *>(data);
		auto curItem = (obs_sceneitem_t *)calldata_ptr(cd, "item");
		bool visible = calldata_bool(cd, "visible");

		if (curItem == this_->sceneitem) {
			QMetaObject::invokeMethod(this_, "HorVisibilityChanged", Qt::QueuedConnection,
						  Q_ARG(bool, visible));
		} else {
			auto verItem = PLSSceneitemMapMgrInstance->getVerticalSceneitem(this_->sceneitem);
			if (curItem == verItem) {
				QMetaObject::invokeMethod(this_, "VerVisibilityChanged", Qt::QueuedConnection,
							  Q_ARG(bool, visible));
			}
		}
	};

	auto itemLocked = [](void *data, calldata_t *cd) {
		pls_unused(cd);
		auto this_ = static_cast<SourceTreeItem *>(data);
		auto curItem = (obs_sceneitem_t *)calldata_ptr(cd, "item");
		bool locked = calldata_bool(cd, "locked");

		if (curItem == this_->sceneitem)
			QMetaObject::invokeMethod(this_, "LockedChanged", Qt::QueuedConnection, Q_ARG(bool, locked));
	};

	auto itemSelect = [](void *data, calldata_t *cd) {
		SourceTreeItem *this_ = reinterpret_cast<SourceTreeItem *>(data);
		obs_sceneitem_t *curItem = (obs_sceneitem_t *)calldata_ptr(cd, "item");

		if (curItem == this_->sceneitem)
			QMetaObject::invokeMethod(this_, "Select", Qt::QueuedConnection);
		if (pls_is_vertical_sceneitem(curItem))
			QMetaObject::invokeMethod(this_, "verItemSelect", Qt::QueuedConnection,
						  Q_ARG(OBSSceneItem, curItem), Q_ARG(bool, true));
	};

	auto itemDeselect = [](void *data, calldata_t *cd) {
		pls_unused(cd);
		auto this_ = static_cast<SourceTreeItem *>(data);
		auto curItem = (obs_sceneitem_t *)calldata_ptr(cd, "item");

		if (curItem == this_->sceneitem)
			QMetaObject::invokeMethod(this_, "Deselect", Qt::QueuedConnection);
		if (pls_is_vertical_sceneitem(curItem))
			QMetaObject::invokeMethod(this_, "verItemSelect", Qt::QueuedConnection,
						  Q_ARG(OBSSceneItem, curItem), Q_ARG(bool, false));
	};

	auto reorderGroup = [](void *data, calldata_t *cd) {
		pls_unused(cd);
		auto this_ = static_cast<SourceTreeItem *>(data);
		QMetaObject::invokeMethod(this_->tree, "ReorderItems", Qt::QueuedConnection);
	};

	const obs_scene_t *scene = obs_sceneitem_get_scene(sceneitem);
	const obs_source_t *sceneSource = obs_scene_get_source(scene);
	signal_handler_t *signal = obs_source_get_signal_handler(sceneSource);

	sigs.emplace_back(signal, "remove", removeItem, this);
	sigs.emplace_back(signal, "item_remove", removeItem, this);
	sigs.emplace_back(signal, "item_visible", itemVisible, this);
	sigs.emplace_back(signal, "item_locked", itemLocked, this);
	sigs.emplace_back(signal, "item_select", itemSelect, this);
	sigs.emplace_back(signal, "item_deselect", itemDeselect, this);

	if (obs_sceneitem_is_group(sceneitem)) {
		const obs_source_t *source = obs_sceneitem_get_source(sceneitem);
		signal = obs_source_get_signal_handler(source);

		sigs.emplace_back(signal, "reorder", reorderGroup, this);
	}

	/* --------------------------------------------------------- */
	auto removeSource = [](void *data, calldata_t *) {
		SourceTreeItem *this_ = reinterpret_cast<SourceTreeItem *>(data);
		this_->DisconnectSignals();
		this_->sceneitem = nullptr;
		QMetaObject::invokeMethod(this_->tree, "RefreshItems", Qt::QueuedConnection);
	};

	const obs_source_t *source = obs_sceneitem_get_source(sceneitem);
	signal = obs_source_get_signal_handler(source);
	//sigs.emplace_back(signal, "remove", removeSource, this);
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
				auto permissionStatus = PLSPermissionHelper::checkPermissionWithSource(source, avType);

				QMetaObject::invokeMethod(
					this,
					[avType, permissionStatus, this]() {
						PLSPermissionHelper::showPermissionAlertIfNeeded(avType,
												 permissionStatus);
					},
					Qt::QueuedConnection);
#endif
				return;
			}
			main->CreatePropertiesWindow(source, OPERATION_NONE /*, main*/);
		}
	}
}

void SourceTreeItem::mousePressEvent(QMouseEvent *event)
{
	QWidget::mousePressEvent(event);
	mousePressed = true;
	if (!editing)
		OnMouseStatusChanged(PROPERTY_VALUE_MOUSE_STATUS_PRESSED);
}

void SourceTreeItem::mouseReleaseEvent(QMouseEvent *event)
{
	QWidget::mouseReleaseEvent(event);
	if (!editing)
		OnMouseStatusChanged(PROPERTY_VALUE_MOUSE_STATUS_HOVER);
}

void SourceTreeItem::enterEvent(QEnterEvent *event)
{
	QWidget::enterEvent(event);

	if (!editing)
		OnMouseStatusChanged(PROPERTY_VALUE_MOUSE_STATUS_HOVER);

	auto pushHoverPreviewItems = [](OBSBasicPreview *preview, OBSSceneItem sceneitem) {
		if (!preview) {
			return;
		}

		std::lock_guard<std::mutex> lock(preview->selectMutex);
		preview->hoveredPreviewItems.clear();
		preview->hoveredPreviewItems.push_back(sceneitem);
	};

	if (!pls_is_dual_output_on()) {
		pushHoverPreviewItems(OBSBasicPreview::Get(), sceneitem);
		return;
	}

	// deal vertical preview when dual output on, ignore group and mapped vertical scene item
	if (obs_sceneitem_is_group(sceneitem)) {
		return;
	}
	pushHoverPreviewItems(OBSBasicPreview::Get(), sceneitem);

	auto verItem = PLSSceneitemMapMgrInstance->getVerticalSceneitem(sceneitem);
	if (PLSSceneitemMapMgrInstance->isMappedVerticalSceneItem(verItem)) {
		return;
	}
	pushHoverPreviewItems(OBSBasic::Get()->getVerticalDisplay(), verItem);
}

void SourceTreeItem::leaveEvent(QEvent *event)
{
	QWidget::leaveEvent(event);

	OnMouseStatusChanged(PROPERTY_VALUE_MOUSE_STATUS_NORMAL);
	OBSBasicPreview *preview = OBSBasicPreview::Get();

	{
		std::lock_guard<std::mutex> lock(preview->selectMutex);
		preview->hoveredPreviewItems.clear();
	}

	auto secondDisplay = OBSBasic::Get()->getVerticalDisplay();
	if (secondDisplay) {
		std::lock_guard<std::mutex> lock(secondDisplay->selectMutex);
		secondDisplay->hoveredPreviewItems.clear();
	}
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
	return QString::asprintf("SourceTreeItem,SourceTreeItem[status=\"%s\"]{background-color: %s;}", property,
				 color);
}

void SourceTreeItem::SetBgColor(SourceItemBgType t, void *param)
{
	QString qss = "";

	do {
		if (t == SourceItemBgType::BgCustom) {
			if (param) {
				auto color = (const char *)param;
				qss += GenerateSourceItemBg(PROPERTY_VALUE_MOUSE_STATUS_NORMAL, color);
				qss += GenerateSourceItemBg(PROPERTY_VALUE_MOUSE_STATUS_HOVER, color);
				qss += GenerateSourceItemBg(PROPERTY_VALUE_MOUSE_STATUS_PRESSED, color);
			}
		} else if (t == SourceItemBgType::BgPreset) {
			auto index = (int)(intptr_t)param;
			if (index < 0 || index >= presetColorListWithOpacity.size())
				break;
			QString color = presetColorListWithOpacity[index];
			qss += GenerateSourceItemBg(PROPERTY_VALUE_MOUSE_STATUS_NORMAL, color.toUtf8());
			qss += GenerateSourceItemBg(PROPERTY_VALUE_MOUSE_STATUS_HOVER, color.toUtf8());
			qss += GenerateSourceItemBg(PROPERTY_VALUE_MOUSE_STATUS_PRESSED, color.toUtf8());
		}

	} while (false);

	if (qss.isEmpty()) {
		// default style
		qss += GenerateSourceItemBg(PROPERTY_VALUE_MOUSE_STATUS_NORMAL, "#272727");
		qss += GenerateSourceItemBg(PROPERTY_VALUE_MOUSE_STATUS_HOVER, "#444444");
		qss += GenerateSourceItemBg(PROPERTY_VALUE_MOUSE_STATUS_PRESSED, "#2d2d2d");
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
		obs_data_set_string(tree->undoSceneData, "Undo.GroupItems", newName.c_str());
		if (auto verItem = PLSSceneitemMapMgrInstance->getVerticalSceneitem(sceneitem); verItem) {
			OBSDataAutoRelease priSettings = obs_sceneitem_get_private_settings(verItem);
			auto groupRefName = obs_data_get_string(priSettings, SCENE_ITEM_REFERENCE_SCENE_NAME);
			obs_data_set_string(tree->undoSceneData, SCENE_ITEM_REFERENCE_SCENE_NAME, groupRefName);
		}

		main->CreateSceneUndoRedoAction(text, tree->undoSceneData, redoSceneData);

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
		OBSMessageBox::information(main, QTStr("Alert.Title"), QTStr("NoNameEntered.Text"));
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

	OBSSourceAutoRelease existingSource = obs_get_source_by_name(newName.c_str());
	bool exists = !!existingSource;

	if (exists) {
		OBSMessageBox::information(main, QTStr("Alert.Title"), QTStr("NameExists.Text"));
		return;
	}

	/* ----------------------------------------- */
	/* rename                                    */

	QSignalBlocker sourcesSignalBlocker(this);
	std::string prevName(obs_source_get_name(source));
	std::string scene_uuid = obs_source_get_uuid(main->GetCurrentSceneSource());
	auto undo = [scene_uuid, prevName, main](const std::string &data) {
		OBSSourceAutoRelease source = obs_get_source_by_uuid(data.c_str());
		obs_source_set_name(source, prevName.c_str());

		OBSSourceAutoRelease scene_source = obs_get_source_by_uuid(scene_uuid.c_str());
		main->SetCurrentScene(scene_source.Get(), true);
	};

	std::string editedName = newName;

	auto redo = [scene_uuid, main, editedName](const std::string &data) {
		OBSSourceAutoRelease source = obs_get_source_by_uuid(data.c_str());
		obs_source_set_name(source, editedName.c_str());

		OBSSourceAutoRelease scene_source = obs_get_source_by_uuid(scene_uuid.c_str());
		main->SetCurrentScene(scene_source.Get(), true);
	};

	const char *uuid = obs_source_get_uuid(source);
	main->undo_s.add_action(QTStr("Undo.Rename").arg(newName.c_str()), undo, redo, uuid, uuid);

	obs_source_set_name(source, newName.c_str());
}

bool SourceTreeItem::eventFilter(QObject *object, QEvent *event)
{
	if (editor != object)
		return false;

	if (LineEditCanceled(event)) {
		QMetaObject::invokeMethod(this, "ExitEditMode", Qt::QueuedConnection, Q_ARG(bool, false));
		return true;
	}
	if (LineEditChanged(event)) {
		QMetaObject::invokeMethod(this, "ExitEditMode", Qt::QueuedConnection, Q_ARG(bool, true));
		return true;
	}

	return false;
}

void SourceTreeItem::HorVisibilityChanged(bool visible)
{
	//PRISM/ZengQin/20200811/#4026/for media controller
	tree->OnVisibleItemChanged(sceneitem, visible);
	horVisBtn->setChecked(visible);

	if (pls_is_dual_output_on()) {
		if (auto verItem = PLSSceneitemMapMgrInstance->getVerticalSceneitem(sceneitem); verItem) {
			visible = visible || obs_sceneitem_visible(verItem);
		}
	}
	vis->setChecked(visible);

	//PRISM/ZengQin/20200818/#4026/for all sources
	UpdateIcon(visible);
	UpdateNameColor(selected, visible);
}

void SourceTreeItem::VerVisibilityChanged(bool visible)
{
	verVisBtn->setChecked(visible);
	auto horItemVis = obs_sceneitem_visible(sceneitem);

	if (pls_is_dual_output_on()) {
		visible = visible || horItemVis;
	}

	vis->setChecked(visible);
	UpdateIcon(visible);
	UpdateNameColor(selected, visible);
}

void SourceTreeItem::LockedChanged(bool locked)
{
	lock->setChecked(locked);
	OBSBasic::Get()->UpdateEditMenu();
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
		expand = new QCheckBox();
		expand->setProperty("sourceTreeSubItem", true);
		expand->setObjectName("expandSubItem");

#ifdef __APPLE__
		expand->setAttribute(Qt::WA_LayoutUsesWidgetRect);
#endif
		boxLayout->insertWidget(0, expand);
		boxLayout->setContentsMargins(7, 0, 0, 0);

		OBSDataAutoRelease data = obs_sceneitem_get_private_settings(sceneitem);
		expand->blockSignals(true);
		expand->setChecked(obs_data_get_bool(data, "collapsed"));
		expand->blockSignals(false);

		connect(expand, &QPushButton::toggled, this, &SourceTreeItem::ExpandClicked);

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
	isItemNormal = (s && 0 == strcmp(s, PROPERTY_VALUE_MOUSE_STATUS_NORMAL));
	if (isItemNormal) {
		horVisBtn->hide();
		verVisBtn->hide();
		deleteBtn->hide();
		vis->hide();
		lock->hide();
	} else {
		horVisBtn->setVisible(pls_is_dual_output_on());
		verVisBtn->setVisible(pls_is_dual_output_on());
		deleteBtn->show();
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
	QString highlightBk = "background:" + QString(SCENE_SCROLLCONTENT_LINE_COLOR) + QString(";");

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

void SourceTreeItem::SelectGroupItem(bool selected)
{
	int cnt = tree->GetStm()->Count();
	for (int i = 0; i < cnt; ++i) {
		SourceTreeItem *item = tree->GetItemWidget(i);
		if (!item) {
			continue;
		}
		if (item->SceneItem() != sceneitem) {
			continue;
		}
		OBSDataAutoRelease settings = obs_sceneitem_get_private_settings(sceneitem);
		obs_data_set_bool(settings, "groupSelectedWithDualOutput", selected);
		item->OnSelectChanged(selected, sceneitem);
		tree->triggeredSelection(i, selected);
	}
}

void SourceTreeItem::OnSelectChanged(bool isSelected, OBSSceneItem sceneitem_, bool sendSignal)
{
	selected = isSelected;

	bool visible = vis->isChecked();
	UpdateIcon(visible);
	UpdateNameColor(isSelected, visible);

	if (sendSignal) {
		emit SelectItemChanged(sceneitem_, isSelected);
	}
}

void SourceTreeItem::UpdateRightMargin()
{
	if (isItemNormal) {
		spaceBeforeVis->changeSize(0, 0);
		spaceBeforeLock->changeSize(0, 0);
		if (isScrollShowed)
			rightMargin->changeSize(SOURCEITEM_MARGIN_NORMAL_SHORT, 1);
		else
			rightMargin->changeSize(SOURCEITEM_MARGIN_NORMAL_LONG, 1);
	} else {
		if (pls_is_dual_output_on()) {
			spaceAfterHorVis->changeSize(SOURCEITEM_SPACE_AFTER_HOR_VIS, 1);
			spaceAfterVerVis->changeSize(SOURCEITEM_SPACE_AFTER_VER_VIS, 1);
			spaceBeforeVis->changeSize(0, 0);

		} else {
			spaceAfterHorVis->changeSize(0, 0);
			spaceAfterVerVis->changeSize(0, 0);
			spaceBeforeVis->changeSize(SOURCEITEM_SPACE_BEFORE_VIS, 1);
		}

		spaceBeforeLock->changeSize(SOURCEITEM_SPACE_BEFORE_LOCK, 1);
		if (isScrollShowed || (pls_is_dual_output_on() && width() < 250)) {
			rightMargin->changeSize(SOURCEITEM_MARGIN_UNNORMAL_SHORT, 1);
		} else {
			rightMargin->changeSize(SOURCEITEM_MARGIN_UNNORMAL_LONG, 1);
		}
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
	tree->SelectItem(sceneitem, true, true);

	pls_async_call(this, []() {
		OBSBasic::Get()->UpdateContextBarDeferred();
		OBSBasic::Get()->UpdateEditMenu();
	});
}

void SourceTreeItem::Deselect()
{
	tree->SelectItem(sceneitem, false, false);

	pls_async_call(this, []() {
		OBSBasic::Get()->UpdateContextBarDeferred();
		OBSBasic::Get()->UpdateEditMenu();
	});
}

void SourceTreeItem::verItemSelect(OBSSceneItem verItem, bool select)
{
	tree->SelectItem(verItem, select, false);

	auto horItem = PLSSceneitemMapMgrInstance->getHorizontalSceneitem(verItem);
	auto stm = tree->GetStm();
	for (int i = 0; i < stm->items.count(); i++) {
		if (stm->items[i] == horItem) {
			tree->NotifyVerticalItemSelect(i, verItem, select);
			break;
		}
	}
	pls_async_call(this, []() {
		OBSBasic::Get()->UpdateContextBarDeferred();
		OBSBasic::Get()->UpdateEditMenu();
	});
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
const QString SOURCE_ICON_CAPTURE_CARD = "capturecard";
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
const QString SOURCE_ICON_SPOUT2 = "spout2";
const QString SOURCE_ICON_CHAT_TEMPLATE = "chat";
const QString SOURCE_ICON_CHZZK_SPONSOR = "chzzksponsor";

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
	case PLS_ICON_TYPE_SPOUT2:
		return SOURCE_ICON_SPOUT2;
	case PLS_ICON_TYPE_CHAT_TEMPLATE:
		return SOURCE_ICON_CHAT_TEMPLATE;
	case PLS_ICON_TYPE_CHZZK_SPONSOR:
		return SOURCE_ICON_CHZZK_SPONSOR;
	case PLS_ICON_TYPE_CAPTURE_CARD:
		return SOURCE_ICON_CAPTURE_CARD;
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
	QString visibleStr = isVisible ? SOURCE_ICON_VISIBLE : SOURCE_ICON_INVIS;
	QString selectStr = isSelected ? SOURCE_ICON_SELECT : SOURCE_ICON_UNSELECT;

	QString value = visibleStr + QString(".") + selectStr;
	label->setProperty(PROPERTY_NAME_STATUS, value);

	label->style()->unpolish(label);
	label->style()->polish(label);
}

void SourceTreeItem::UpdateIcon(bool visible)
{
	const obs_source_t *source = obs_sceneitem_get_source(sceneitem);
	if (!source)
		return;

	const char *id = obs_source_get_id(source);
	if (!id)
		return;

	QString visibleStr = visible ? SOURCE_ICON_VISIBLE : SOURCE_ICON_INVIS;
	QString selectStr = selected ? SOURCE_ICON_SELECT : SOURCE_ICON_UNSELECT;

	if (strcmp(id, "scene") == 0) {
		OnIconTypeChanged(visibleStr + QString(".") + selectStr + QString(".") + QString(SOURCE_ICON_SCENE));
	} else if (strcmp(id, "group") == 0) {
		OnIconTypeChanged(visibleStr + QString(".") + selectStr + QString(".") + QString(SOURCE_ICON_GROUP));
	} else {
		QString value = visibleStr + QString(".") + QString(SOURCE_ICON_VALID) + QString(".") + selectStr +
				QString(".") + GetIconKey(obs_source_get_icon_type(id));
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

	switch (event) {
	case OBS_FRONTEND_EVENT_PREVIEW_SCENE_CHANGED:
		stm->SceneChanged();
		break;
	case OBS_FRONTEND_EVENT_EXIT:
		stm->Clear();
		obs_frontend_remove_event_callback(OBSFrontendEvent, stm);
		break;
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
	QVector<OBSSceneItem> &items = *static_cast<QVector<OBSSceneItem> *>(ptr);

	obs_source_t *src = obs_sceneitem_get_source(item);
	if (obs_source_removed(src)) {
		return true;
	}

	if (obs_sceneitem_is_group(item)) {
		OBSDataAutoRelease data = obs_sceneitem_get_private_settings(item);

		bool collapse = obs_data_get_bool(data, "collapsed");
		if (!collapse) {
			obs_scene_t *scene = obs_sceneitem_group_get_scene(item);

			pls_scene_enum_items_all(scene, enumItem, &items);
		}
	} else if (pls_is_vertical_sceneitem(item)) {
		return true;
	}

	items.insert(0, item);
	return true;
}

struct ChildAndParent {
	std::map<obs_sceneitem_t *, std::pair<obs_sceneitem_t *, bool>> childAndParent;
	std::pair<obs_sceneitem_t *, bool> parent;
};
static bool enumItemForParent(obs_scene_t *, obs_sceneitem_t *item, void *ptr)
{
	auto ref = (ChildAndParent *)(ptr);
	if (!ref)
		return false;

	ChildAndParent &sourceParents = *ref;
	sourceParents.childAndParent.insert(std::make_pair(item, sourceParents.parent));
	if (obs_sceneitem_is_group(item)) {

		std::pair<obs_sceneitem_t *, bool> parentTemp = sourceParents.parent;
		sourceParents.parent = std::make_pair(item, obs_sceneitem_visible(item));
		pls_sceneitem_group_enum_items_all(item, enumItemForParent, &sourceParents);
		sourceParents.parent = parentTemp;
	}
	return true;
}

void SourceTreeModel::SceneChanged()
{
	OBSScene scene = GetCurrentScene();

	beginResetModel();
	items.clear();
	pls_scene_enum_items_all(scene, enumItem, &items);
	endResetModel();

	UpdateGroupState(false);
	st->ResetWidgets();

	for (int i = 0; i < items.count(); i++) {
		bool select = obs_sceneitem_selected(items[i]);
		if (pls_is_dual_output_on()) {
			if (auto item = st->GetItemWidget(i); item)
				select = item->checkItemSelected(select);
			if (obs_sceneitem_is_group(items[i])) {
				select = st->CheckGroupAllItemSelected(items[i]);
				st->NotifyVerticalItemSelect(i, items[i], select);
			}
		}

		QModelIndex index = createIndex(i, 0);
		pls_async_call(this, [this, index, select]() {
			st->selectionModel()->select(index, select ? QItemSelectionModel::Select
								   : QItemSelectionModel::Deselect);
		});
	}
}

/* moves a scene item index (blame linux distros for using older Qt builds) */
static inline void MoveItem(QVector<OBSSceneItem> &items, int oldIdx, int newIdx)
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
	pls_scene_enum_items_all(scene, enumItem, &newitems);

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
		beginMoveRows(QModelIndex(), idx1Old, idx1Old + count - 1, QModelIndex(), idx1New + count);
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
		if (pls_is_dual_output_on()) {
			OBSBasic::Get()->resetGroupTransforms(item);
		}
	} else {
		if (Existed(item)) {
			return;
		}

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
			const obs_scene_t *subscene = obs_sceneitem_get_scene(subitem);

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
	OBSBasic::Get()->UpdateContextBarDeferred();
}

OBSSceneItem SourceTreeModel::Get(int idx)
{
	if (idx == -1 || idx >= items.count())
		return OBSSceneItem();
	return items[idx];
}

SourceTreeModel::SourceTreeModel(SourceTree *st_) : QAbstractListModel(st_), st(st_)
{
	obs_frontend_add_event_callback(OBSFrontendEvent, this);
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
	if (index.row() >= items.size()) {
		return Qt::NoItemFlags;
	}

	obs_sceneitem_t *item = items[index.row()];
	if (!pls_is_alive(item)) {
		assert(false && "There was invalid sceneitem existed.");
		PLS_WARN(SOURCE_MODULE, "There was invalid sceneitem existed.");
		return Qt::NoItemFlags;
	}
	bool is_group = obs_sceneitem_is_group(item);

	return QAbstractListModel::flags(index) | Qt::ItemIsEditable | Qt::ItemIsDragEnabled |
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
		OBSSourceAutoRelease group = obs_get_source_by_name(QT_TO_UTF8(name));
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
	obs_sceneitem_t *group = obs_scene_add_group(GetCurrentScene(), QT_TO_UTF8(name));
	if (!group)
		return;

	beginInsertRows(QModelIndex(), 0, 0);
	items.insert(0, group);
	endInsertRows();

	st->UpdateWidget(createIndex(0, 0, nullptr), group);
	UpdateGroupState(true);

	QMetaObject::invokeMethod(st, "Edit", Qt::QueuedConnection, Q_ARG(int, 0));
}

bool SourceTreeModel::Existed(OBSSceneItem item)
{
	for (int i = 0; i < items.count(); i++) {
		if (item == items[i]) {
			return true;
		}
	}
	return false;
}

void SourceTreeModel::GroupSelectedItems(QModelIndexList &indices)
{
	if (indices.count() == 0)
		return;

	OBSScene scene = GetCurrentScene();
	QString name = GetNewGroupName();

	QVector<obs_sceneitem_t *> item_order;

	auto id = 0;
	bool isDualOutput = pls_is_dual_output_on();
	for (int i = indices.count() - 1; i >= 0; i--) {
		obs_sceneitem_t *item = items[indices[i].row()];
		item_order << item;
		if (isDualOutput) {
			auto verItem = PLSSceneitemMapMgrInstance->getVerticalSceneitem(item);
			if (verItem) {
				id = obs_sceneitem_get_id(verItem);
				item_order << verItem;
			}
		}
	}

	auto main = OBSBasic::Get();
	st->undoSceneData = main->BackupScene(scene);

	obs_sceneitem_t *item = obs_scene_insert_group(scene, QT_TO_UTF8(name), item_order.data(), item_order.size());
	if (!item) {
		st->undoSceneData = nullptr;
		return;
	}
	for (auto item_ : item_order) {
		pls_obs_sceneitem_move_hotkeys(obs_group_from_source(obs_sceneitem_get_source(item)), item_);
	}

	main->undo_s.push_disabled();

	if (isDualOutput) {
		OBSDataAutoRelease settings = obs_sceneitem_get_private_settings(item);
		obs_data_set_bool(settings, "groupSelectedWithDualOutput", true);
		PLSSceneitemMapMgrInstance->groupItems(obs_source_get_name(obs_scene_get_source(scene)),
						       QT_TO_UTF8(name), item_order);
		for (obs_sceneitem_t *item : item_order)
			obs_sceneitem_select(item, true);
		hasGroups = true;
		st->UpdateWidgets(true);

		obs_sceneitem_select(item, false);
		PLSSceneitemMapMgrInstance->switchToDualOutputMode();
	} else {
		for (obs_sceneitem_t *item : item_order)
			obs_sceneitem_select(item, false);
		hasGroups = true;
		st->UpdateWidgets(true);

		obs_sceneitem_select(item, true);
	}

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

	/* ----------------------------------------------------------------- */
	/* obs_scene_insert_group triggers a full refresh of scene items via */
	/* the item_add signal. No need to insert a row, just edit the one   */
	/* that's created automatically.                                     */

	QMetaObject::invokeMethod(st, "NewGroupEdit", Qt::QueuedConnection, Q_ARG(int, newIdx));
}

static bool enumDshowItem(obs_scene_t *, obs_sceneitem_t *item, void *param)
{
	QVector<OBSSceneItem> &items = *static_cast<QVector<OBSSceneItem> *>(param);
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
			pls_sceneitem_group_enum_items_all(item, enumDshowItem, &itemsList);
			if (!itemsList.empty()) {
				emit itemRemoves(itemsList);
			}
		}
		PLSSceneitemMapMgrInstance->removeItem(item, true);
		pls_sceneitem_group_ungroup(item);
	}

	SceneChanged();

	OBSData redoData = main->BackupScene(scene);
	main->CreateSceneUndoRedoAction(QTStr("Basic.Main.Ungroup"), undoData, redoData);
}

void SourceTreeModel::ExpandGroup(obs_sceneitem_t *item)
{
	int itemIdx = items.indexOf(item);
	if (itemIdx == -1)
		return;

	itemIdx++;

	obs_scene_t *scene = obs_sceneitem_group_get_scene(item);

	QVector<OBSSceneItem> subItems;
	pls_scene_enum_items_all(scene, enumItem, &subItems);

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
		const obs_scene_t *itemScene = obs_sceneitem_get_scene(items[i]);

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
		[this](QVector<OBSSceneItem> items) { emit itemsRemove(items); });
	connect(stm_, &SourceTreeModel::itemReorder, this, [this] { emit itemsReorder(); });
	setModel(stm_);

	connect(PLSSceneitemMapMgrInstance, &PLSSceneitemMapManager::duplicateItemSuccess, this,
		[this](OBSSceneItem horItem, OBSSceneItem verItem) { updateVerItemIconVisible(horItem, verItem); });

	pls_add_css(this, {"PLSSource"});

	scrollBar = pls_new<QSourceScrollBar>(this);
	setVerticalScrollBar(scrollBar);
	setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

	setVerticalScrollMode(ScrollPerPixel);
	setMouseTracking(true);

	noSourceTips = pls_new<QLabel>(this);
	noSourceTips->setObjectName(NO_SOURCE_TEXT_LABEL);
	noSourceTips->hide();

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
			preDragOver->UpdateIndicator(SourceTreeItem::IndicatorType::IndicatorNormal);
			break;
		}
	}

	preDragOver = nullptr;
}

bool SourceTree::GetDestGroupItem(QPoint pos, obs_sceneitem_t *&item_output) const
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

	obs_sceneitem_t *dropGroup = itemIsGroup ? dropItem : obs_sceneitem_get_group(scene, dropItem);

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
		obs_data_t *data = obs_sceneitem_get_private_settings(dropGroup);
		obs_data_release(data);
	}

	if (indicator == QAbstractItemView::BelowItem || indicator == QAbstractItemView::OnItem ||
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
		if (!itemBelow || obs_sceneitem_get_group(scene, itemBelow) != dropGroup) {
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
			pls_sceneitem_group_enum_items_all(item, enumChildInclude, ptr);
		} else {
			const char *id = obs_source_get_id(source);
			if (id && 0 == strcmp(id, SCENE_SOURCE_ID)) {
				obs_scene_t *sceneTemp = obs_scene_from_source(source);
				pls_scene_enum_items_all(sceneTemp, enumChildInclude, ptr);
			}
		}
	}

	return true;
}

// return false : invalid drag
bool SourceTree::CheckDragSceneToGroup(const obs_sceneitem_t *dragItem, const obs_sceneitem_t *destGroupItem) const
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
	pls_scene_enum_items_all(dragScene, enumChildInclude, &helper);

	if (helper.includeChild) {
		return false;
	} else {
		return true;
	}
}

bool SourceTree::IsValidDrag(obs_sceneitem_t *destGroupItem, QVector<OBSSceneItem> items) const
{
	std::map<obs_sceneitem_t *, std::pair<obs_sceneitem_t *, bool>> selectedSource;

	ChildAndParent childParents;
	childParents.parent = std::make_pair(nullptr, true);
	pls_scene_enum_items_all(GetCurrentScene(), enumItemForParent, &childParents);

	QModelIndexList indexs = selectedIndexes();
	size_t indexsCount = indexs.size();
	for (unsigned i = 0; i < (unsigned)indexsCount; i++) {
		obs_sceneitem_t *item = items[indexs[i].row()];
		selectedSource.insert(std::make_pair(item, childParents.childAndParent[item]));
	}

	bool isDragValid = true;
	bool isDestGroup = obs_sceneitem_is_group(destGroupItem);
	bool isDestVisible = obs_sceneitem_visible(destGroupItem);

	auto iter = selectedSource.begin();
	for (; iter != selectedSource.end(); ++iter) {
		if (iter->second.first == destGroupItem) {
			continue;
		}
		// Since the source reference count problem has been solved,
		// we will temporarily remove the restriction on dragging in and out of the group and verify whether there is any problem.
		if (iter->second.first != nullptr && obs_sceneitem_is_group(iter->second.first) &&
		    !iter->second.second) {
			isDragValid = false; // cann't drag out of invisible group
			break;
		}
		if (iter->second.first != nullptr && obs_sceneitem_is_group(iter->second.first) &&
		    pls_is_dual_output_on()) {
			isDragValid = false; // cann't drag out of when dual output on
			break;
		}

		if (destGroupItem && isDestGroup) {
			if (!isDestVisible) {
				isDragValid = false; // cann't drag into invisible group
				break;
			}

			if (pls_is_dual_output_on()) {
				isDragValid = false; // cann't drag into group when dual output on
				break;
			}

			if (!CheckDragSceneToGroup(iter->first, destGroupItem)) {
				isDragValid = false; // cann't drag scene into group which is included in scene
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
		connect(uiItem, &SourceTreeItem::SelectItemChanged, this, &SourceTree::OnSelectItemChanged,
			Qt::QueuedConnection);
		connect(uiItem, &SourceTreeItem::VisibleItemChanged, this, &SourceTree::OnVisibleItemChanged,
			Qt::QueuedConnection);
		setIndexWidget(index, uiItem);
	}
}

void SourceTree::UpdateWidget(const QModelIndex &idx, obs_sceneitem_t *item)
{
	auto uiItem = pls_new<SourceTreeItem>(this, item);
	connect(uiItem, &SourceTreeItem::SelectItemChanged, this, &SourceTree::OnSelectItemChanged,
		Qt::QueuedConnection);
	connect(uiItem, &SourceTreeItem::VisibleItemChanged, this, &SourceTree::OnVisibleItemChanged,
		Qt::QueuedConnection);
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
			item->OnSelectChanged(item->checkItemSelected(select), sceneitem);
			break;
		}
	}
}

void SourceTree::NotifyVerticalItemSelect(int index, obs_sceneitem_t *sceneitem_, bool select)
{
	SourceTreeItem *item = GetItemWidget(index);
	if (item) {
		select = select || obs_sceneitem_selected(item->sceneitem);
		item->OnSelectChanged(select, sceneitem_, true);
	}
}

void SourceTree::SelectGroupItem(obs_sceneitem_t *groupItem, bool selected, bool forceSelect)
{
	int cnt = GetStm()->Count();
	for (int i = 0; i < cnt; ++i) {
		SourceTreeItem *item = GetItemWidget(i);
		if (!item) {
			continue;
		}
		if (item->SceneItem() != groupItem) {
			continue;
		}
		OBSDataAutoRelease settings = obs_sceneitem_get_private_settings(groupItem);
		obs_data_set_bool(settings, "groupSelectedWithDualOutput", selected);
		item->OnSelectChanged(selected, groupItem);

		QModelIndex index = GetStm()->createIndex(i, 0);
		if (index.isValid() && selected != selectionModel()->isSelected(index) || forceSelect) {
			ignoreSelectionChanged = !forceSelect;
			selectionModel()->select(index, selected ? QItemSelectionModel::Select
								 : QItemSelectionModel::Deselect);
		}
	}

	OBSBasic::Get()->UpdateContextBarDeferred();
	pls_async_call(this, []() { OBSBasic::Get()->UpdateEditMenu(); });
}

void SourceTree::triggeredSelection(int row, bool select)
{
	QModelIndex index = GetStm()->createIndex(row, 0);
	if (index.isValid() && select != selectionModel()->isSelected(index)) {
		ignoreSelectionChanged = true;
		selectionModel()->select(index, select ? QItemSelectionModel::Select : QItemSelectionModel::Deselect);
	}
}

void SourceTree::SelectItemWithDualOutput(obs_sceneitem_t *sceneitem_, bool select, bool fromSourceList)
{
	SourceTreeModel *stm = GetStm();
	int i = 0;

	isVerticalItemSignal = pls_is_vertical_sceneitem(sceneitem_);
	for (; i < stm->items.count(); i++) {
		if (stm->items[i] == sceneitem_)
			break;
		auto verItem = PLSSceneitemMapMgrInstance->getVerticalSceneitem(stm->items[i]);
		if (sceneitem_ == verItem) {
			isVerticalItemSignal = true;
			break;
		}
	}

	if (i == stm->items.count())
		return;
	bool mousePress = false;
	auto widget = GetItemWidget(i);
	if (!widget) {
		return;
	}
	mousePress = widget->mousePressed;

	if (isVerticalItemSignal) {
		NotifyVerticalItemSelect(i, sceneitem_, select);
		if (mousePress) {
			obs_sceneitem_select(stm->items[i], select);
		}
	} else {
		NotifyItemSelect(sceneitem_, select);
		if (shiftPressed || this->mousePressed || mousePress || (needHandleClick && !select)) {
			if (auto verItem = PLSSceneitemMapMgrInstance->getVerticalSceneitem(stm->items[i]); verItem) {
				obs_sceneitem_select(verItem, select);
			}
		}
	}

	auto checkItemFunc = [this](OBSSceneItem item) {
		auto select = obs_sceneitem_selected(item);
		if (!select) {
			CheckGroupItemUnselectStatus(item);
		}
	};

	bool selectStatus;
	if (isVerticalItemSignal) {
		selectStatus = widget->checkItemSelected(obs_sceneitem_selected(stm->items[i]));
		checkItemFunc(sceneitem_);
	} else {
		selectStatus = widget->checkItemSelected(obs_sceneitem_selected(sceneitem_));
		checkItemFunc(stm->items[i]);
	}

	QModelIndex index = stm->createIndex(i, 0);
	if (index.isValid() && selectStatus != selectionModel()->isSelected(index)) {
		ignoreSelectionChanged = true;
		selectionModel()->select(index,
					 selectStatus ? QItemSelectionModel::Select : QItemSelectionModel::Deselect);
		NotifyItemSelect(isVerticalItemSignal ? stm->items[i].Get() : sceneitem_, selectStatus);
	}
}

void SourceTree::SelectItem(obs_sceneitem_t *sceneitem_, bool select, bool fromSourceList)
{
	if (pls_is_dual_output_on()) {
		SelectItemWithDualOutput(sceneitem_, select, fromSourceList);
		return;
	}

	SourceTreeModel *stm = GetStm();
	int i = 0;

	for (; i < stm->items.count(); i++) {
		if (stm->items[i] == sceneitem_)
			break;
	}

	if (i == stm->items.count())
		return;

	NotifyItemSelect(sceneitem_, select);

	QModelIndex index = stm->createIndex(i, 0);
	if (index.isValid() && select != selectionModel()->isSelected(index)) {
		pls_async_call(this, [this, index, select]() {
			selectionModel()->select(index,
						 select ? QItemSelectionModel::Select : QItemSelectionModel::Deselect);
		});
	}
}

Q_DECLARE_METATYPE(OBSSceneItem);

void SourceTree::mouseDoubleClickEvent(QMouseEvent *event)
{
	if (event->button() == Qt::LeftButton)
		QListView::mouseDoubleClickEvent(event);
}

void SourceTree::mousePressEvent(QMouseEvent *event)
{
	if (event->button() == Qt::LeftButton) {
		mousePressed = true;
	}
	int row = indexAt(event->pos()).row();
	int cnt = GetStm()->Count();
	for (int i = 0; i < cnt; ++i) {
		SourceTreeItem *item = GetItemWidget(i);
		if (row == i) {
			needHandleClick = true;
			item->resetMousePressed(true);
			continue;
		}
		item->resetMousePressed(false);
		auto sceneitem = item->SceneItem().Get();
		if (obs_sceneitem_is_group(sceneitem)) {
			obs_sceneitem_select(sceneitem, false);
			SelectGroupItem(sceneitem, false, true);
		}
	}
	needHandleClick = needHandleClick || (row == -1);
	noValidClick = row == -1;

	if (!pls_is_dual_output_on() || noValidClick) {
		QListView::mousePressEvent(event);
		return;
	}

	pls_async_call(this, [this, row]() {
		QModelIndex index = GetStm()->createIndex(row, 0);
		if (index.isValid() && selectionModel()->isSelected(index)) {
			auto item = GetStm()->items[row];
			// ignore group selected : obs will set group status select false again
			if (obs_sceneitem_is_group(item)) {
				SelectGroupItem(item, true, true);
				SetGroupAllItemSelected(item);
				return;
			}
			obs_sceneitem_select(item, true);
			if (auto verItem = PLSSceneitemMapMgrInstance->getVerticalSceneitem(item); verItem) {
				obs_sceneitem_select(verItem, true);
			}
		}
	});

	QListView::mousePressEvent(event);
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

bool SourceTree::checkDragBreak(const QDragMoveEvent *event, int row, const DropIndicatorPosition &indicator)
{
	const SourceTreeItem *item = GetItemWidget(row);
	int itemY = item->mapTo(this, QPoint(0, 0)).y();
	int itemH = item->size().height();
	int cursorY = event->pos().y();

	// Sometimes indicator can't match current position of cursor, which will cause UI refresh

	if (indicator == QAbstractItemView::AboveItem && (cursorY > itemY + itemH / 2))
		return true;

	if (indicator == QAbstractItemView::BelowItem && (cursorY < itemY + itemH / 2))
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
			preDragOver->UpdateIndicator(SourceTreeItem::IndicatorType::IndicatorAbove);
			break;

		case QAbstractItemView::OnItem:
		case QAbstractItemView::BelowItem:
			preDragOver->UpdateIndicator(SourceTreeItem::IndicatorType::IndicatorBelow);
			break;

		default:
			break;
		}
	} while (false);

	QListView::dragMoveEvent(event);
}

void SourceTree::dropEventHasGroups(const SourceTreeModel *stm, QModelIndexList &indices, const OBSScene &scene,
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
			const obs_sceneitem_t *subitemGroup = obs_sceneitem_get_group(scene, subitem);

			if (subitemGroup == item) {
				QModelIndex idx = stm->createIndex(j, 0);
				indices.insert(i + 1, idx);
			}
		}
	}
}

template<typename TnsertLastGroup>
static void dropEventUpdateScene(bool hasGroups, const OBSScene &scene, obs_sceneitem_t *&lastGroup,
				 const TnsertLastGroup &insertLastGroup, const QVector<OBSSceneItem> &items,
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
	auto newOrder = PLSSceneitemMapMgrInstance->reorderItems(scene, orderList);
	obs_scene_reorder_items2(scene, newOrder.data(), newOrder.size());
}

void SourceTree::dropEventPersistentIndicesForeachCb(int &r, SourceTreeModel *stm, QVector<OBSSceneItem> &items,
						     const QPersistentModelIndex &persistentIdx) const
{
	int from = persistentIdx.row();
	int to = r;
	int itemTo = to;

	if (itemTo > from)
		itemTo--;

	if (itemTo != from) {
		stm->beginMoveRows(QModelIndex(), from, from, QModelIndex(), to);
		MoveItem(items, from, itemTo);
		stm->endMoveRows();
	}

	r = persistentIdx.row() + 1;
}

void SourceTree::resetMousePressed()
{
	int cnt = GetStm()->Count();
	for (int i = 0; i < cnt; ++i) {
		SourceTreeItem *item = GetItemWidget(i);
		if (item) {
			item->resetMousePressed(false);
		}
	}
	mousePressed = false;
	needHandleClick = false;
}

void SourceTree::mouseMoveEvent(QMouseEvent *event)
{
	QPoint pos = event->pos();
	const SourceTreeItem *item = qobject_cast<SourceTreeItem *>(childAt(pos));

	OBSBasicPreview *preview = OBSBasicPreview::Get();

	QListView::mouseMoveEvent(event);

	auto pushHoverPreviewItems = [](OBSBasicPreview *preview, OBSSceneItem sceneitem) {
		if (!preview) {
			return;
		}

		std::lock_guard<std::mutex> lock(preview->selectMutex);
		preview->hoveredPreviewItems.clear();
		preview->hoveredPreviewItems.push_back(sceneitem);
	};

	if (!pls_is_dual_output_on() && item) {
		pushHoverPreviewItems(preview, item->sceneitem);
		return;
	}
	// deal vertical preview when dual output on, ignore group and mapped vertical scene item
	if (!item || obs_sceneitem_is_group(item->sceneitem)) {
		return;
	}
	pushHoverPreviewItems(preview, item->sceneitem);

	auto verItem = PLSSceneitemMapMgrInstance->getVerticalSceneitem(item->sceneitem);
	if (PLSSceneitemMapMgrInstance->isMappedVerticalSceneItem(verItem)) {
		return;
	}
	pushHoverPreviewItems(OBSBasic::Get()->getVerticalDisplay(), verItem);
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
	int row = indexAt(event->position().toPoint()).row();
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

	obs_sceneitem_t *dropGroup = itemIsGroup ? dropItem : obs_sceneitem_get_group(scene, dropItem);

	std::vector<QString> srcChilds;
	std::vector<QString> destChilds;

	if (dropGroup && obs_sceneitem_is_group(dropGroup))
		pls_sceneitem_group_enum_items_all(dropGroup, TravelGroupChilds, (void *)&srcChilds);

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
		obs_data_t *data = obs_sceneitem_get_private_settings(dropGroup);
		dropOnCollapsed = obs_data_get_bool(data, "collapsed");
		obs_data_release(data);
	}

	if (indicator == QAbstractItemView::BelowItem || indicator == QAbstractItemView::OnItem ||
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
		if (!itemBelow || obs_sceneitem_get_group(scene, itemBelow) != dropGroup) {
			dropGroup = nullptr;
			dropOnCollapsed = false;
		}
	}

	if (!IsValidDrag(dropGroup, items)) {
		PLS_INFO(SOURCE_MODULE, "There is an invalid drag in dropEvent");
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
			sources.push_back(obs_scene_get_source(obs_sceneitem_get_scene(item)));
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
		      [this, &r, stm, &items](const QPersistentModelIndex &persistentIdx) {
			      dropEventPersistentIndicesForeachCb(r, stm, items, persistentIdx);
		      });

	std::sort(persistentIndices.begin(), persistentIndices.end());
	int firstIdx = persistentIndices.front().row();
	int lastIdx = persistentIndices.back().row();

	/* --------------------------------------- */
	/* reorder scene items in back-end         */

	QVector<struct obs_sceneitem_order_info> orderList;
	obs_sceneitem_t *lastGroup = nullptr;
	int insertCollapsedIdx = 0;

	auto insertCollapsed = [&orderList, &insertCollapsedIdx, &lastGroup](obs_sceneitem_t *item) {
		struct obs_sceneitem_order_info info;
		info.group = lastGroup;
		info.item = item;

		orderList.insert(insertCollapsedIdx++, info);
	};

	using insertCollapsed_t = decltype(insertCollapsed);

	auto preInsertCollapsed = [](obs_scene_t *, obs_sceneitem_t *item, void *param) {
		(*reinterpret_cast<insertCollapsed_t *>(param))(item);
		return true;
	};

	auto insertLastGroup = [&]() {
		OBSDataAutoRelease data = obs_sceneitem_get_private_settings(lastGroup);
		bool collapsed = obs_data_get_bool(data, "collapsed");

		if (collapsed) {
			insertCollapsedIdx = 0;
			obs_sceneitem_group_enum_items(lastGroup, preInsertCollapsed, &insertCollapsed);
		}

		struct obs_sceneitem_order_info info;
		info.group = nullptr;
		info.item = lastGroup;
		orderList.insert(0, info);
	};

	auto updateScene = [&hasGroups, &scene, &lastGroup, &insertLastGroup, &items, &dropGroup, &firstIdx, &lastIdx,
			    &orderList]() {
		dropEventUpdateScene(hasGroups, scene, lastGroup, insertLastGroup, items, dropGroup, firstIdx, lastIdx,
				     orderList);
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
		pls_sceneitem_group_enum_items_all(dropGroup, TravelGroupChilds, (void *)&destChilds);
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

	{
		std::lock_guard<std::mutex> lock(preview->selectMutex);
		preview->hoveredPreviewItems.clear();
	}
	if (!pls_is_dual_output_on()) {
		return;
	}
	auto secondDisplay = OBSBasic::Get()->getVerticalDisplay();
	if (!secondDisplay) {
		return;
	}
	std::lock_guard<std::mutex> lock(secondDisplay->selectMutex);
	secondDisplay->hoveredPreviewItems.clear();
}

void SourceTree::selectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
	if (pls_is_dual_output_on() && ignoreSelectionChanged) {
		ignoreSelectionChanged = false;
		return;
	}

	{
		QSignalBlocker sourcesSignalBlocker(this);
		SourceTreeModel *stm = GetStm();

		QModelIndexList selectedIdxs = selected.indexes();
		QModelIndexList deselectedIdxs = deselected.indexes();

		QSet<int64_t> groupSelectedItems;
		for (int i = 0; i < selectedIdxs.count(); i++) {
			int idx = selectedIdxs[i].row();
			if (idx >= stm->items.size()) {
				return;
			}

			if (pls_is_dual_output_on() && obs_sceneitem_is_group(stm->items[idx])) {

				if (CheckGroupAllItemSelected(stm->items[idx])) {
					continue;
				}

				auto selectGroupChilds = [](obs_scene_t *, obs_sceneitem_t *item, void *val) {
					QSet<int64_t> &selectedItems = *static_cast<QSet<int64_t> *>(val);
					obs_sceneitem_select(item, true);
					selectedItems.insert((int64_t)item);
					return true;
				};
				pls_sceneitem_group_enum_items_all(stm->items[idx], selectGroupChilds,
								   &groupSelectedItems);
				auto widget = GetItemWidget(idx);
				if (widget) {
					widget->SelectGroupItem(true);
				}

			} else {
				obs_sceneitem_select(stm->items[idx], true);
			}
		}

		for (int i = 0; i < deselectedIdxs.count(); i++) {
			int idx = deselectedIdxs[i].row();
			if (idx >= stm->items.size()) {
				return;
			}
			if (pls_is_dual_output_on() && obs_sceneitem_is_group(stm->items[idx])) {
				auto unselectGroupChilds = [](obs_scene_t *, obs_sceneitem_t *item, void *val) {
					obs_sceneitem_select(item, false);
					return true;
				};
				auto widget = GetItemWidget(idx);
				if (!widget) {
					continue;
				}
				widget->SelectGroupItem(false);
				if (widget->getMousePressed() || noValidClick) {
					pls_sceneitem_group_enum_items_all(stm->items[idx], unselectGroupChilds,
									   nullptr);
				}

			} else {
				if (pls_is_dual_output_on()) {
					auto iter = groupSelectedItems.find((int64_t)stm->items[idx].Get());
					if (iter != groupSelectedItems.end()) {
						triggeredSelection(idx, true);
						continue;
					}

					if (CheckItemInSelectedGroup(stm->items[idx].Get())) {
						triggeredSelection(idx, true);
						continue;
					}
				}

				// check item->group was clicked, if ture ignore
				obs_sceneitem_select(stm->items[idx], false);
				if (auto verItem = PLSSceneitemMapMgrInstance->getVerticalSceneitem(stm->items[idx]);
				    verItem) {
					obs_sceneitem_select(verItem, false);
				}
			}
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
				  "Lain, because apparently it is, in fact, "
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
		main->CreateSceneUndoRedoAction(text, undoSceneData, redoSceneData);

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

bool SourceTree::GroupsSelectedWithDualOutput(const QModelIndexList &selectedIndices) const
{
	obs_scene_t *groupScene = nullptr;
	bool haveGroup = false;
	SourceTreeModel *stm = GetStm();
	for (auto &idx : selectedIndices) {
		obs_sceneitem_t *item = stm->items[idx.row()];
		if (!obs_sceneitem_is_group(item)) {
			auto itemScene = obs_sceneitem_get_scene(item);
			if (!groupScene) {
				groupScene = itemScene;
			} else {
				if (itemScene != groupScene) {
					return false;
				}
			}
		} else {
			haveGroup = true;
			auto groupItemScene = obs_sceneitem_group_get_scene(item);

			if (!groupScene) {
				groupScene = groupItemScene;
			} else {
				if (groupItemScene != groupScene) {
					return false;
				}
			}
		}
	}

	if (selectedIndices.size() > 1) {
		return true;
	}
	return haveGroup;
}

bool SourceTree::GroupsSelected() const
{
	QModelIndexList selectedIndices = selectedIndexes();

	OBSScene scene = GetCurrentScene();

	if (selectedIndices.size() < 1) {
		return false;
	}

	if (pls_is_dual_output_on()) {
		return false;
	}

	SourceTreeModel *stm = GetStm();
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

bool SourceTree::CheckItemInSelectedGroup(obs_sceneitem_t *sceneitem)
{
	auto scene = obs_sceneitem_get_scene(sceneitem);
	if (!obs_scene_is_group(scene)) {
		return false;
	}
	auto groupSource = obs_scene_get_source(scene);

	int cnt = GetStm()->Count();
	for (int i = 0; i < cnt; ++i) {
		SourceTreeItem *item = GetItemWidget(i);
		if (!item) {
			continue;
		}
		if (obs_sceneitem_get_source(item->SceneItem()) == groupSource) {
			return item->getMousePressed();
		}
	}
	return false;
}

bool SourceTree::CheckGroupItemSelected(obs_sceneitem_t *sceneitem)
{
	if (!obs_sceneitem_is_group(sceneitem)) {
		return false;
	}
	OBSDataAutoRelease settings = obs_sceneitem_get_private_settings(sceneitem);
	return obs_data_get_bool(settings, "groupSelectedWithDualOutput");
}

bool SourceTree::CheckGroupAllItemSelected(obs_sceneitem_t *sceneitem)
{
	if (!CheckGroupItemSelected(sceneitem)) {
		return false;
	}
	auto selectGroupChilds = [](obs_scene_t *, obs_sceneitem_t *item, void *val) {
		bool &isSlected = *static_cast<bool *>(val);
		isSlected = obs_sceneitem_selected(item);
		if (!isSlected) {
			return false;
		}
		return true;
	};
	bool isSlected = false;
	pls_sceneitem_group_enum_items_all(sceneitem, selectGroupChilds, &isSlected);

	return isSlected;
}

void SourceTree::SetGroupAllItemSelected(obs_sceneitem_t *sceneitem)
{
	auto selectGroupChilds = [](obs_scene_t *, obs_sceneitem_t *item, void *) {
		obs_sceneitem_select(item, true);
		return true;
	};
	pls_sceneitem_group_enum_items_all(sceneitem, selectGroupChilds, nullptr);
}

void SourceTree::CheckGroupItemUnselectStatus(obs_sceneitem_t *sceneitem)
{
	if (!sceneitem || obs_sceneitem_selected(sceneitem)) {
		return;
	}
	auto scene = obs_sceneitem_get_scene(sceneitem);
	if (!obs_scene_is_group(scene)) {
		return;
	}

	auto source = obs_sceneitem_get_source(sceneitem);
	auto groupItem = obs_sceneitem_get_group(GetCurrentScene(), sceneitem);

	QVector<OBSSceneItem> items;
	obs_sceneitem_group_enum_items(groupItem, enumItem, &items);

	if (items.count() < 1) {
		return;
	}
	SelectGroupItem(groupItem, false);
}

void SourceTree::Remove(OBSSceneItem item, OBSScene scene) const
{
	OBSBasic *main = reinterpret_cast<OBSBasic *>(App()->GetMainWindow());
	GetStm()->Remove(item);
	main->SaveProject();

	if (!main->SavingDisabled()) {
		obs_source_t *sceneSource = obs_scene_get_source(scene);
		obs_source_t *itemSource = obs_sceneitem_get_source(item);
		main->DeletePropertiesWindow(itemSource);
		main->DeleteFiltersWindow(itemSource);
		main->UpdateContextBarDeferred();
		blog(LOG_INFO, "User Removed source '%s' (%s) from scene '%s'", obs_source_get_name(itemSource),
		     obs_source_get_id(itemSource), obs_source_get_name(sceneSource));
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
	QString file = !App()->IsThemeDark() ? ":res/images/no_sources.svg"
					     : "theme:Dark/no_sources.svg";
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
		int maxContentWidth = (height / (metrics.height() + NO_SOURCE_TIPS_WORD_SPACING)) * width;
		p.drawText(QRect(left, 0, width, height),
			   metrics.elidedText(QTStr("Prism.NoSources.Label"), Qt::ElideRight, maxContentWidth), option);

	} else {
		QListView::paintEvent(event);
	}
}

SourceTreeDelegate::SourceTreeDelegate(QObject *parent) : QStyledItemDelegate(parent) {}

QSize SourceTreeDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
	SourceTree *tree = qobject_cast<SourceTree *>(parent());
	QWidget *item = tree->indexWidget(index);

	if (!item)
		return (QSize(0, 0));

	return (QSize(option.widget->minimumWidth(), item->height()));
}

static bool enum_group_item(obs_scene_t *, obs_sceneitem_t *item, void *param)
{
	if (!param || !item)
		return true;
	QVector<OBSSceneItem> &items = *static_cast<QVector<OBSSceneItem> *>(param);
	items.push_back(item);
	return true;
}

void SourceTree::updateVerItemIconVisible(OBSSceneItem horItem, OBSSceneItem verItem)
{
	auto idx = -1;
	SourceTreeModel *stm = GetStm();
	for (int i = 0; i < stm->items.count(); i++) {
		if (horItem == stm->items[i]) {
			idx = i;
			break;
		}
	}
	if (idx != -1) {
		updateVisibleIcon(idx, horItem, verItem);
	}
}

void SourceTree::updateVisIconVisibleWhenDualoutput()
{
	SourceTreeModel *stm = GetStm();
	for (int i = 0; i < stm->items.count(); i++) {
		auto item = stm->items[i];
		auto verItem = PLSSceneitemMapMgrInstance->getVerticalSceneitem(item);
		if (!verItem) {
			continue;
		}
		updateVisibleIcon(i, item, verItem);
	}
}

void SourceTree::updateVisibleIcon(int index, OBSSceneItem horItem, OBSSceneItem verItem)
{
	SourceTreeItem *itemUi = GetItemWidget(index);
	if (!itemUi) {
		return;
	}
	bool verItemVis = obs_sceneitem_visible(verItem);
	bool horItemVis = obs_sceneitem_visible(horItem);
	itemUi->horVisBtn->setChecked(horItemVis);
	itemUi->verVisBtn->setChecked(verItemVis);
	if (pls_is_dual_output_on()) {
		itemUi->vis->setChecked(verItemVis || horItemVis);
		itemUi->UpdateIcon(verItemVis || horItemVis);
		itemUi->UpdateNameColor(itemUi->selected, verItemVis || horItemVis);
	} else {
		if (!horItemVis) {
			obs_sceneitem_set_visible(verItem, false);
			itemUi->verVisBtn->setChecked(false);
		}
		itemUi->vis->setChecked(horItemVis);
		itemUi->UpdateIcon(horItemVis);
		itemUi->UpdateNameColor(itemUi->selected, horItemVis);
	}
}

void SourceTreeItem::resetMousePressed(bool mousePressed_)
{
	mousePressed = mousePressed_;
}

bool SourceTreeItem::getMousePressed()
{
	return mousePressed;
}

bool SourceTreeItem::checkItemSelected(bool horSelected)
{
	if (pls_is_dual_output_on()) {

		//check group first
		OBSDataAutoRelease settings = obs_sceneitem_get_private_settings(sceneitem);
		if (obs_data_get_bool(settings, "groupSelectedWithDualOutput")) {
			return true;
		}

		// check veritem selected
		auto verItem = PLSSceneitemMapMgrInstance->getVerticalSceneitem(sceneitem);
		if (verItem) {
			horSelected = horSelected || obs_sceneitem_selected(verItem);
		}
		return horSelected;
	}

	return horSelected;
}

void SourceTree::keyPressEvent(QKeyEvent *event)
{
	Qt::KeyboardModifiers modifiers = event->modifiers();
	if (modifiers == Qt::ControlModifier && event->key() == Qt::Key_A) {
		shiftPressed = true;
	}
	switch (event->key()) {
	case Qt::Key_Shift:
		shiftPressed = true;
		break;
	}
	QListView::keyPressEvent(event);
}

void SourceTree::keyReleaseEvent(QKeyEvent *event)
{
	shiftPressed = false;
	QListView::keyReleaseEvent(event);
}

void SourceTree::mouseReleaseEvent(QMouseEvent *event)
{
	mousePressed = false;
	QListView::mouseReleaseEvent(event);
}
