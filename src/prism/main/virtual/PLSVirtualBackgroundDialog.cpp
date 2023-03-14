#include "PLSVirtualBackgroundDialog.h"
#include "ui_PLSVirtualBackgroundDialog.h"
#include "qstringlist.h"
#include "qspinbox.h"
#include <qdir.h>
#include <QResizeEvent>
#include <algorithm>
#include "display-helpers.hpp"
#include "pls-app.hpp"
#include "main-view.hpp"
#include "pls-common-define.hpp"
#include "PLSVirtualBgManager.h"
#include "PLSMotionImageListView.h"
#include "PLSMotionDefine.h"
#include "obs.h"
#include <qtooltip.h>

Q_DECLARE_METATYPE(OBSSceneItem);

static const char *s_noSourceString = "main.beauty.face.nosource";

static const char *GEOMETRY_DATA = "geometryVirturalBg"; //key of beauty window geometry in global ini
static const char *MAXIMIZED_STATE = "isMaxState";       //key if the beauty window is maximized in global ini
static const char *s_isNormalModel = "isNormalModel";
static const char *s_isKo = "isKo";

static const int s_minZoomValue = 0;
static const int s_maxZoomValue = 100;

#define CSTR_VIDEO_DEVICE_ID "video_device_id"
#define CSTR_CAPTURE_STATE "capture_state"

#define CSTR_SOURCE_IMAGE_STATUS "source_image_status"

gs_texture_t *PLSVirtualBackgroundDialog::bg_texture = nullptr;

static void PLSFrontendEvent(enum obs_frontend_event event, void *ptr);

static int zoomValueToSliderValue(double zoomValue)
{
	return int(zoomValue * 1000);
}

static double sliderValueToZoomValue(int sliderValue)
{
	return sliderValue / 1000.0;
}

QRectF calcImageViewport(double &scale, const QRectF &imageRect, const QRectF &viewportRect)
{
	if (imageRect.isEmpty() || viewportRect.isEmpty()) {
		scale = 0;
		return QRectF(0, 0, 1.0, 1.0);
	}

	qreal imageWHR = imageRect.width() / imageRect.height();
	qreal viewportWHR = viewportRect.width() / viewportRect.height();
	if (imageWHR > viewportWHR) {
		qreal width = imageRect.width() * viewportRect.height() / imageRect.height();
		scale = width / imageRect.width();
		return QRectF((width - viewportRect.width()) / 2 / width, 0, viewportRect.width() / width, 1.0);
	} else {
		qreal height = imageRect.height() * viewportRect.width() / imageRect.width();
		scale = height / imageRect.height();
		return QRectF(0, (height - viewportRect.height()) / 2 / height, 1.0, viewportRect.height() / height);
	}
}

static bool isIntersectionValid(const QRect &intersectionRect, const QRect &sourceRect)
{
	if (intersectionRect == sourceRect) {
		return true;
	}
	return false;
}

static QRect calcIntersectionRect(QRect &imageRect, const QRect &sourceRect)
{
	QRect intersectionRect = imageRect & sourceRect;
	for (QRect boundingRect = imageRect | sourceRect;;) {
		if (isIntersectionValid(intersectionRect, sourceRect)) {
			break;
		}

		QRect tmpBoundingRect = imageRect.translated(1, 1) | sourceRect;
		if (intersectionRect.width() < sourceRect.width()) {
			imageRect.translate((tmpBoundingRect.width() < boundingRect.width()) ? 1 : -1, 0);
		}

		if (intersectionRect.height() < sourceRect.height()) {
			imageRect.translate(0, (tmpBoundingRect.height() < boundingRect.height()) ? 1 : -1);
		}

		boundingRect = imageRect | sourceRect;
		intersectionRect = imageRect & sourceRect;
	}
	return intersectionRect;
}

class SliderValueInitHelper {
	bool &sliderValueInit;

public:
	SliderValueInitHelper(bool &sliderValueInit_) : sliderValueInit(sliderValueInit_) { sliderValueInit = true; }
	~SliderValueInitHelper() { sliderValueInit = false; }
};

PLSVirtualBackgroundDialog::PLSVirtualBackgroundDialog(DialogInfo info, QWidget *parent, PLSDpiHelper dpiHelper) : PLSDialogView(info, parent, dpiHelper), ui(new Ui::PLSVirtualBackgroundDialog)
{

	dpiHelper.setCss(this, {
				       PLSCssIndex::PLSVirtualBackgroundDialog,
				       PLSCssIndex::PLSImageTextButton,
				       PLSCssIndex::PLSMotionErrorLayer,
			       });

	ui->setupUi(this->content());
	m_motionViewCreated = true;

	this->setCursor(Qt::ArrowCursor);
	this->setAttribute(Qt::WA_DeleteOnClose, true);
	notifyFirstShow([=]() {
		this->InitGeometry(true);
		QMetaObject::invokeMethod(
			this,
			[=]() {
				adjustChromakeyTipSize(ui->preview->size().width());
				//adjustFPSTipSize(ui->preview->size().width());
			},
			Qt::QueuedConnection);
	});

	setHasMaxResButton(true);
	setCaptionButtonMargin(9);
	this->setWindowTitle(tr("virtual.view.title"));

	obs_frontend_add_event_callback(PLSFrontendEvent, this);

	ui->noSourceLabel->setHidden(true);
	setupSingals();
	setupFirstUI();
	reloadVideoDatas();
	updateUI();

	connect(ui->preview, &PLSVirtualBackgroundDisplay::beginVBkgDrag, this, [this]() {
		const auto &data = PLS_VIRTUAL_BG_MANAGER->getVirtualData();
		if (!data.isImageEditMode) {
			return;
		}

		double width = data.resrcWidth * data.zoomValue;
		double height = data.resrcHeight * data.zoomValue;
		beginDragVBkgImgRect = QRectF(data.srcWidth * data.dstRectX - width * data.srcRectX, data.srcHeight * data.dstRectY - height * data.srcRectY, width, height).toRect();
	});
	connect(ui->preview, &PLSVirtualBackgroundDisplay::dragVBkgMoving, this, [this](int cx, int cy) {
		if ((!cx && !cy) || renderViewportRect.isEmpty()) {
			return;
		}

		auto &data = PLS_VIRTUAL_BG_MANAGER->getVirtualData();
		if (!data.isImageEditMode || (data.srcWidth <= 0) || (data.srcHeight <= 0)) {
			return;
		}

		double scale = data.srcWidth / double(renderViewportRect.width());
		int scx = int(cx * scale);
		int scy = int(cy * scale);

		QRect imageRect = QRect(beginDragVBkgImgRect.topLeft(), QSize(qMax(beginDragVBkgImgRect.width(), data.srcWidth), qMax(beginDragVBkgImgRect.height(), data.srcHeight)));
		imageRect.translate(scx, scy);

		QRect intersectionRect = calcIntersectionRect(imageRect, QRect(0, 0, data.srcWidth, data.srcHeight));

		data.srcRectX = static_cast<long>(intersectionRect.x() - imageRect.x()) / double(imageRect.width());
		data.srcRectY = static_cast<long>(intersectionRect.y() - imageRect.y()) / double(imageRect.height());
		data.srcRectCX = intersectionRect.width() / double(imageRect.width());
		data.srcRectCY = intersectionRect.height() / double(imageRect.height());

		data.dstRectX = intersectionRect.x() / double(data.srcWidth);
		data.dstRectY = intersectionRect.y() / double(data.srcHeight);
		data.dstRectCX = intersectionRect.width() / double(data.srcWidth);
		data.dstRectCY = intersectionRect.height() / double(data.srcHeight);

		PLS_VIRTUAL_BG_MANAGER->updateDataToCore();
	});

	bg_texture = nullptr;
}

PLSVirtualBackgroundDialog::~PLSVirtualBackgroundDialog()
{
	obs_display_remove_draw_callback(ui->preview->GetDisplay(), PLSVirtualBackgroundDialog::drawPreview, this);
	obs_frontend_remove_event_callback(PLSFrontendEvent, this);

	obs_enter_graphics();
	if (bg_texture) {
		gs_texture_destroy(bg_texture);
		bg_texture = nullptr;
	}
	obs_leave_graphics();

	delete ui;
}

void PLSVirtualBackgroundDialog::setupSingals()
{
	connect(ui->videosComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PLSVirtualBackgroundDialog::onSourceComboxIndexChanged);

	connect(ui->blurControlButton, &QPushButton::clicked, this, &PLSVirtualBackgroundDialog::onBlurControlButtonClicked);

	connect(ui->minButton, &QPushButton::clicked, this, &PLSVirtualBackgroundDialog::onMinButtonClicked);
	connect(ui->maxButton, &QPushButton::clicked, this, &PLSVirtualBackgroundDialog::onMaxButtonClicked);
	connect(ui->flipHButton, &QPushButton::clicked, this, &PLSVirtualBackgroundDialog::onFlipHoriButtonClicked);
	connect(ui->flipVButton, &QPushButton::clicked, this, &PLSVirtualBackgroundDialog::onFlipVertiButtonClicked);
	connect(ui->resetButton, &QPushButton::clicked, this, [=]() {
		PLS_UI_STEP(s_virtualBackground, "image reset button click", ACTION_CLICK);
		onResetImageData(true);
	});
	connect(ui->saveButton, &QPushButton::clicked, this, &PLSVirtualBackgroundDialog::onSaveEditButtonClicked);

	connect(ui->closeEditButton, &QPushButton::clicked, this, &PLSVirtualBackgroundDialog::onCloseEditButtonClicked);

	connect(ui->imageEditButton, &QPushButton::clicked, this, &PLSVirtualBackgroundDialog::onImageEidtButtonClicked);

	connect(ui->motionCheckBox, &QCheckBox::stateChanged, this, &PLSVirtualBackgroundDialog::onMotionCheckBoxClicked);

	connect(ui->originalBgButton, &QPushButton::clicked, this, &PLSVirtualBackgroundDialog::onOriginBackgroundClicked);
	connect(ui->clearBgButton, &QPushButton::clicked, this, &PLSVirtualBackgroundDialog::onClearBackgroundButtonClicked);

	connect(ui->blurSlider, SIGNAL(valueChanged(int)), this, SLOT(onBlurValueChanged(int)));
	connect(ui->blurSpinBox, SIGNAL(valueChanged(int)), this, SLOT(onBlurValueChanged(int)));

	connect(ui->zoomSlider, SIGNAL(valueChanged(int)), this, SLOT(onZoomSliderValueChanged(int)));

	connect(ui->motionListView, &PLSMotionImageListView::currentResourceChanged, this, &PLSVirtualBackgroundDialog::onCurrentMotionListResourceChanged);
	connect(
		ui->motionListView, &PLSMotionImageListView::setSelectedItemFailed, this, [=](const QString &itemID) { onCurrentResourceFileIsInvaild(QStringList(itemID)); }, Qt::QueuedConnection);

	connect(ui->preview, &PLSVirtualBackgroundDisplay::mousePressed, this, [=]() { isEnterTempOriginModel(true); });
	connect(ui->preview, &PLSVirtualBackgroundDisplay::mouseReleased, this, [=]() { isEnterTempOriginModel(false); });
}

void PLSVirtualBackgroundDialog::setupFirstUI()
{

	const auto &data = PLS_VIRTUAL_BG_MANAGER->getVirtualData();

	ui->preview->installEventFilter(this);
	auto addDrawCallback = [this]() { obs_display_add_draw_callback(ui->preview->GetDisplay(), PLSVirtualBackgroundDialog::drawPreview, this); };

	connect(ui->preview, &PLSQTDisplay::DisplayCreated, addDrawCallback);
	ui->preview->show();

	initSlider(ui->blurSlider, 0, 100, 1, data.blurValue);

	ui->blurSpinBox->setRange(0, 100);
	ui->blurSpinBox->setValue(ui->blurSlider->value());
	ui->blurSpinBox->setSingleStep(1);

	bool isKo = IS_KR();
	ui->originalBgButton->setProperty(s_isKo, isKo);
	ui->clearBgButton->setProperty(s_isKo, isKo);
}

void PLSVirtualBackgroundDialog::updateUI()
{
	auto &data = PLS_VIRTUAL_BG_MANAGER->getVirtualData();
	bool isInvalid = PLS_VIRTUAL_BG_MANAGER->getCurrentSceneItem() == nullptr;

	QMetaObject::invokeMethod(
		this,
		[=]() {
			ui->bgStackedWidget->setCurrentWidget(isInvalid ? ui->noSourcePage : ui->showDisplayPage);
			ui->noSourceLabel->setHidden(false);
		},
		isInvalid ? Qt::DirectConnection : Qt::QueuedConnection);

	ui->motionListView->setItemSelectedEnabled(!isInvalid);

	SliderValueInitHelper sliderValueInitHelper(sliderValueInit);
	if (!data.isValid) {
		QString &path = data.isMotionEnable ? data.originPath : data.stopPath;
		if (!path.isEmpty()) {
			QSize mediaSize;
			if (pls_get_media_size(mediaSize, path.toUtf8().constData())) {
				data.isValid = true;

				data.resrcWidth = mediaSize.width();
				data.resrcHeight = mediaSize.height();

				calcImageViewport(data.zoomMin, QRectF(0, 0, mediaSize.width(), mediaSize.height()), QRectF(0, 0, data.srcWidth, data.srcHeight));
				data.zoomMax = data.zoomMin * 3;

				int zoomMin = zoomValueToSliderValue(data.zoomMin), zoomMax = zoomValueToSliderValue(data.zoomMax), zoomValue = zoomValueToSliderValue(data.zoomValue);
				setEditSliderRange(zoomMin, zoomMax);
				ui->zoomSlider->setValue(((zoomValue < zoomMin) || (zoomValue > zoomMax)) ? zoomValueToSliderValue(data.zoomValue = data.zoomMin) : zoomValue);
				ui->zoomSlider->setEnabled(true);
			} else {
				data.srcRectX = data.srcRectY = data.srcRectCX = data.srcRectCY = 0;
				ui->zoomSlider->setEnabled(false);
			}
		} else {
			data.srcRectX = data.srcRectY = data.srcRectCX = data.srcRectCY = 0;
			ui->zoomSlider->setEnabled(false);
		}
	} else {
		calcImageViewport(data.zoomMin, QRectF(0, 0, data.resrcWidth, data.resrcHeight), QRectF(0, 0, data.srcWidth, data.srcHeight));
		data.zoomMax = data.zoomMin * 3;

		int zoomMin = zoomValueToSliderValue(data.zoomMin), zoomMax = zoomValueToSliderValue(data.zoomMax), zoomValue = zoomValueToSliderValue(data.zoomValue);
		setEditSliderRange(zoomMin, zoomMax);
		ui->zoomSlider->setValue(((zoomValue < zoomMin) || (zoomValue > zoomMax)) ? zoomValueToSliderValue(data.zoomValue = data.zoomMin) : zoomValue);
		ui->zoomSlider->setEnabled(true);
	}
	pls_flush_style(ui->zoomSlider, STATUS_HANDLE, ui->zoomSlider->isEnabled());

	updateSwitchImageEditModelUI();
	updateBlurPageUI();
	updateImageEditUI();
	updateBackgroundUI();
}

void PLSVirtualBackgroundDialog::updateSwitchImageEditModelUI()
{
	const auto &data = PLS_VIRTUAL_BG_MANAGER->getVirtualData();
	ui->actionStackedWidget->setCurrentWidget(data.isImageEditMode ? ui->editPage : ui->blurPage);
}

void PLSVirtualBackgroundDialog::updateBackgroundUI(bool isFromMotionList, bool isFromNowMethod)
{
	auto &data = PLS_VIRTUAL_BG_MANAGER->getVirtualData();
	pls_flush_style_recursive(ui->originalBgButton->m_boderLabel, s_isNormalModel, data.bgModelType == PLSVrBgModel::Original);
	pls_flush_style_recursive(ui->clearBgButton->m_boderLabel, s_isNormalModel, data.bgModelType == PLSVrBgModel::BgDelete);
	ui->originalBgButton->setEnabled(!PLS_VIRTUAL_BG_MANAGER->isNoSourcePage());
	ui->clearBgButton->setEnabled(!PLS_VIRTUAL_BG_MANAGER->isNoSourcePage());

	if (data.bgModelType != PLSVrBgModel::OtherResource) {
		if (!data.motionItemID.isEmpty()) {
			data.motionItemID = "";
		}
		data.stopPath = data.originPath = data.thumbnailFilePath = data.foregroundPath = data.foregroundStaticPath = QString();
		ui->motionListView->clearSelectedItem();
		return;
	}

	if (!data.motionItemID.isEmpty()) {
		if (!isFromMotionList) {
			ui->motionListView->setSelectedItem(data.motionItemID);
		}
		return;
	}

	data.bgModelType = PLSVrBgModel::Original;
	PLS_VIRTUAL_BG_MANAGER->updateDataToCore();
	ui->motionListView->clearSelectedItem();

	if (isFromNowMethod) {
		return;
	}
	updateBackgroundUI(isFromMotionList, true);
}

void PLSVirtualBackgroundDialog::updateBlurPageUI()
{
	const auto &data = PLS_VIRTUAL_BG_MANAGER->getVirtualData();
	bool uiCanClick = (data.bgModelType == PLSVrBgModel::Original || data.bgModelType == PLSVrBgModel::OtherResource);
	ui->blurSlider->setValue(uiCanClick ? data.blurValue : 0);
	ui->blurSlider->setEnabled(uiCanClick);
	pls_flush_style(ui->blurSlider, STATUS_HANDLE, uiCanClick);
	ui->blurSpinBox->setEnabled(uiCanClick);
	ui->blurControlButton->setEnabled(uiCanClick);
	bool _blurEnable = data.blurValue > 0 && uiCanClick;
	pls_flush_style(ui->blurControlButton, s_isNormalModel, _blurEnable);

	ui->imageEditButton->setEnabled(data.bgModelType == PLSVrBgModel::OtherResource);

	if (data.bgModelType == PLSVrBgModel::OtherResource && data.thidrSourceType == MotionType::MOTION && data.isPrismResource) {
		ui->motionCheckBox->setEnabled(true);
	} else {
		ui->motionCheckBox->setEnabled(false);
	}
	ui->motionCheckBox->setChecked(data.isMotionUIChecked);
}

void PLSVirtualBackgroundDialog::updateImageEditUI()
{
	const auto &data = PLS_VIRTUAL_BG_MANAGER->getVirtualData();
	ui->flipHButton->setProperty(s_isNormalModel, data.isFlipHori);
	ui->flipVButton->setProperty(s_isNormalModel, data.isFlipVerti);
	pls_flush_style_recursive(ui->editPage);
}

static int GetNextVisibleItemIndex(QWidget *widget)
{
	if (!widget) {
		return -1;
	}

	PLSComboBox *combobox = qobject_cast<PLSComboBox *>(widget);
	if (!combobox) {
		return -1;
	}

	QListView *view = qobject_cast<QListView *>(combobox->view());
	if (!view) {
		return -1;
	}

	for (int i = 0; i < combobox->count(); i++) {
		auto itemData = combobox->itemData(i).value<OBSSceneItem>();
		if (view->isRowHidden(i) || itemData == nullptr) {
			continue;
		}
		return i;
	}

	return -1;
}

void PLSVirtualBackgroundDialog::setSourceSelect(const QString &, OBSSceneItem item)
{
	if (item == nullptr) {
		return;
	}

	QListView *view = qobject_cast<QListView *>(ui->videosComboBox->view());
	if (!view) {
		return;
	}
	if (PLS_VIRTUAL_BG_MANAGER->getCurrentSceneItem() == item) {
		return;
	}

	isEnterTempOriginModel(false);

	bool isInCombox = isSourceComboboxVisible(item);
	if (!PLS_VIRTUAL_BG_MANAGER->isDShowSourceAvailable(item)) {
		if (isInCombox) {
			reloadVideoDatas();
		}
		return;
	}

	if (isInCombox) {
		if (setCurrentIndex(item)) {
			return;
		}
	}

	int index = GetNextVisibleItemIndex(ui->videosComboBox);
	if (index >= 0) {
		setCurrentIndex(ui->videosComboBox->itemData(index).value<OBSSceneItem>());
		return;
	}

	if (item != nullptr) {
		reloadVideoDatas();
	}
}

void PLSVirtualBackgroundDialog::reloadVideoComboboxList(const DShowSourceVecType &list, OBSSceneItem forceShowItem)
{
	ui->videosComboBox->blockSignals(true);
	ui->videosComboBox->clear();
	PLS_VIRTUAL_BG_MANAGER->updateSourceListDatas(list);

	bool isNoSource = true;
	for (const auto &data : list) {
		if (!PLS_VIRTUAL_BG_MANAGER->isDShowSourceAvailable(data.second)) {
			if (forceShowItem == nullptr || forceShowItem != data.second) {
				continue;
			}
		}
		isNoSource = false;
		ui->videosComboBox->addItem(data.first, QVariant::fromValue(data.second));
	}

	if (isNoSource) {
		ui->videosComboBox->addItem(QTStr(s_noSourceString), QVariant::fromValue(nullptr));
	}
	ui->videosComboBox->blockSignals(false);

	foundAndSetShouldSetSource();

	reInitDisplay();
}

void PLSVirtualBackgroundDialog::onSourceComboxIndexChanged(int index)
{
	PLS_INFO(s_virtualBackground, __FUNCTION__ " name:%s", ui->videosComboBox->currentText().toStdString().c_str());
	auto itemScene = ui->videosComboBox->currentData().value<OBSSceneItem>();
	bool noSource = itemScene == nullptr || -1 == index;

	bool isSameItem = itemScene == PLS_VIRTUAL_BG_MANAGER->getCurrentSceneItem();

	setCurrentSourceItem(itemScene);
	PLS_VIRTUAL_BG_MANAGER->updateDataFromCore(itemScene);
	if (!isSameItem) {
		PLS_VIRTUAL_BG_MANAGER->leaveNotSavedImageEditMode(itemScene);
	}

	if (!noSource) {
		PLSBasic *basic = reinterpret_cast<PLSBasic *>(App()->GetMainWindow());
		if (!m_ignoreChangeIndex && !basic->isSceneItemOnlySelect(itemScene)) {
			emit currentSourceChanged(ui->videosComboBox->currentText(), itemScene);
		}
		reInitDisplay();
	}

	ui->videosComboBox->setEnabled(!noSource);
	updateUI();
	hideChromakeyToast(); //hide first
	auto &data = PLS_VIRTUAL_BG_MANAGER->getVirtualData();
	if (!data.haveShownChromakeyTip && obs_source_filter_valid(PLS_VIRTUAL_BG_MANAGER->getCurrentSource(), FILTER_TYPE_ID_CHROMAKEY)) {
		data.haveShownChromakeyTip = true;
		showChromakeyToast();
	}

	// 	hideFPSToast();
	// 	if (PLS_VIRTUAL_BG_MANAGER->getCurrentSource()) {
	// 		showFPSToast();
	// 	}

	PLS_VIRTUAL_BG_MANAGER->updateDataToCore();

	if (m_isFirstShow) {
		m_isFirstShow = false;
		if (data.bgModelType == PLSVrBgModel::OtherResource) {
			ui->motionListView->switchToSelectItem(data.motionItemID);
		}
	}
}

void PLSVirtualBackgroundDialog::setSourceVisible(const QString &, OBSSceneItem item, bool)
{
	QListView *view = qobject_cast<QListView *>(ui->videosComboBox->view());
	if (!view) {
		return;
	}

	isEnterTempOriginModel(false);

	PLSBasic *basic = reinterpret_cast<PLSBasic *>(App()->GetMainWindow());
	if (!basic->isDshowSourceBySourceId(item)) {
		return;
	}
	reloadVideoDatas(nullptr);
}

void PLSVirtualBackgroundDialog::setBgFileError(const QString &sourceName)
{
	auto _source = pls_get_source_by_name(sourceName.toStdString().c_str());
	if (_source == PLS_VIRTUAL_BG_MANAGER->getCurrentSource()) {
		ui->motionListView->showApplyErrorToast();
		onOriginBackgroundClicked();
	}
}

void PLSVirtualBackgroundDialog::vrRunderProcessExit(void *source)
{
	if (!source) {
		return;
	}
	if (source == PLS_VIRTUAL_BG_MANAGER->getCurrentSource()) {
		auto &data = PLS_VIRTUAL_BG_MANAGER->getVirtualData();
		data.blurValue = 0;
		onOriginBackgroundClicked();
		return;
	}

	//PLS_VIRTUAL_BG_MANAGER->checkOtherSourceRunderProcessExit(source);
}

void PLSVirtualBackgroundDialog::vrReloadCurrentData(void *source)
{
	if (!source || source != PLS_VIRTUAL_BG_MANAGER->getCurrentSource()) {
		return;
	}
	PLS_DEBUG(s_virtualBackground, __FUNCTION__);
	PLS_VIRTUAL_BG_MANAGER->updateDataFromCore(PLS_VIRTUAL_BG_MANAGER->getCurrentSceneItem());
	updateUI();
}

void PLSVirtualBackgroundDialog::setPreviewCallback(bool set)
{
	set ? obs_display_add_draw_callback(ui->preview->GetDisplay(), PLSVirtualBackgroundDialog::drawPreview, this)
	    : obs_display_remove_draw_callback(ui->preview->GetDisplay(), PLSVirtualBackgroundDialog::drawPreview, this);
}

void PLSVirtualBackgroundDialog::setCurrentSourceItem(OBSSceneItem item)
{
	clearSourceDisplay();

	if (item == nullptr || item != PLS_VIRTUAL_BG_MANAGER->getCurrentSceneItem()) {
		PLS_VIRTUAL_BG_MANAGER->leaveImageEditMode(false);
	}

	PLS_VIRTUAL_BG_MANAGER->setCurrentSceneItem(nullptr);
	if (!PLS_VIRTUAL_BG_MANAGER->isCurrentSourceExisted(item)) {
		return;
	}

	PLS_VIRTUAL_BG_MANAGER->setCurrentSceneItem(item);
}

bool PLSVirtualBackgroundDialog::isSourceComboboxVisible(const OBSSceneItem item)
{
	QListView *view = qobject_cast<QListView *>(ui->videosComboBox->view());
	if (!view) {
		return false;
	}

	int index = findText(item);
	if (-1 == index) {
		return false;
	}

	return !view->isRowHidden(index);
}

int PLSVirtualBackgroundDialog::findText(const OBSSceneItem item)
{
	if (item == nullptr) {
		return -1;
	}
	for (int i = 0; i < ui->videosComboBox->count(); i++) {
		OBSSceneItem itemData = ui->videosComboBox->itemData(i).value<OBSSceneItem>();
		if (itemData == item) {
			return i;
		}
	}
	return -1;
}

bool PLSVirtualBackgroundDialog::setCurrentIndex(OBSSceneItem item)
{
	int index = findText(item);
	if (-1 == index && item != nullptr) {
		reloadVideoDatas();
		return false;
	}
	if (-1 == index) {
		return false;
	}
	m_ignoreChangeIndex = true;

	auto oldSelect = ui->videosComboBox->currentIndex();
	ui->videosComboBox->setCurrentIndex(index);

	if (oldSelect == index) {
		onSourceComboxIndexChanged(index);
	}

	m_ignoreChangeIndex = false;
	return true;
}

void PLSVirtualBackgroundDialog::reInitDisplay()
{
	if (PLS_VIRTUAL_BG_MANAGER->getCurrentSource() == nullptr) {
		return;
	}

	ui->preview->UpdateSourceState(PLS_VIRTUAL_BG_MANAGER->getCurrentSource());
	ui->preview->AttachSource(PLS_VIRTUAL_BG_MANAGER->getCurrentSource());
}

void PLSVirtualBackgroundDialog::clearSourceDisplay()
{
	ui->preview->UpdateSourceState(nullptr);
	ui->preview->AttachSource(nullptr);
}

void PLSVirtualBackgroundDialog::onBlurControlButtonClicked()
{
	PLS_UI_STEP(s_virtualBackground, __FUNCTION__, ACTION_CLICK);
	auto &data = PLS_VIRTUAL_BG_MANAGER->getVirtualData();
	data.blurValue = data.blurValue > 0 ? 0 : 10;
	PLS_VIRTUAL_BG_MANAGER->updateDataToCore();
	updateBlurPageUI();
}

void PLSVirtualBackgroundDialog::onBlurValueChanged(int value)
{
	if (value % 30 == 0 || value == ui->blurSpinBox->maximum()) {
		auto logStr = QString("%1 value:%2").arg(__FUNCTION__).arg(value);
		PLS_UI_STEP(s_virtualBackground, logStr.toStdString().c_str(), ACTION_CLICK);
	}

	ui->blurSpinBox->blockSignals(true);
	ui->blurSpinBox->setValue(value);
	ui->blurSpinBox->blockSignals(false);

	ui->blurSlider->blockSignals(true);
	ui->blurSlider->setValue(value);
	ui->blurSlider->blockSignals(false);

	auto &data = PLS_VIRTUAL_BG_MANAGER->getVirtualData();

	bool uiCanClick = (data.bgModelType == PLSVrBgModel::Original || data.bgModelType == PLSVrBgModel::OtherResource);

	if (data.blurValue != value && uiCanClick) {
		data.blurValue = value;
		updateBlurPageUI();
		PLS_VIRTUAL_BG_MANAGER->updateDataToCore();
	}
}

void PLSVirtualBackgroundDialog::onImageEidtButtonClicked()
{
	PLS_UI_STEP(s_virtualBackground, __FUNCTION__, ACTION_CLICK);
	PLS_VIRTUAL_BG_MANAGER->enterImageEditMode();
	auto &data = PLS_VIRTUAL_BG_MANAGER->getVirtualData();
	data.isImageEditMode = true;
	PLS_VIRTUAL_BG_MANAGER->updateDataToCore();
	updateUI();
}

void PLSVirtualBackgroundDialog::onMotionCheckBoxClicked(int state)
{
	PLS_UI_STEP(s_virtualBackground, __FUNCTION__, ACTION_CLICK);
	auto &data = PLS_VIRTUAL_BG_MANAGER->getVirtualData();
	data.isMotionUIChecked = state == Qt::Checked;
	PLS_VIRTUAL_BG_MANAGER->updateDataToCore();
}

void PLSVirtualBackgroundDialog::onMinButtonClicked()
{
	PLS_UI_STEP(s_virtualBackground, __FUNCTION__, ACTION_CLICK);
	ui->zoomSlider->setValue(ui->zoomSlider->minimum());
}

void PLSVirtualBackgroundDialog::onMaxButtonClicked()
{
	PLS_UI_STEP(s_virtualBackground, __FUNCTION__, ACTION_CLICK);
	ui->zoomSlider->setValue(ui->zoomSlider->maximum());
}

void PLSVirtualBackgroundDialog::onZoomSliderValueChanged(int value)
{
	if (sliderValueInit || renderViewportRect.isEmpty()) {
		return;
	}

	if (value != ui->zoomSlider->minimum()) {
		qreal zoomValue = sliderValueToZoomValue(value);

		auto &data = PLS_VIRTUAL_BG_MANAGER->getVirtualData();
		if (zoomValue == data.zoomValue) {
			return;
		}

		double oldWidth = data.resrcWidth * data.zoomValue;
		double oldHeight = data.resrcHeight * data.zoomValue;
		QPoint imageCenterPt =
			QRectF(data.srcWidth * data.dstRectX - oldWidth * data.srcRectX, data.srcHeight * data.dstRectY - oldHeight * data.srcRectY, oldWidth, oldHeight).center().toPoint();

		data.zoomValue = zoomValue;

		double width = data.resrcWidth * data.zoomValue;
		double height = data.resrcHeight * data.zoomValue;
		QRect imageRect = QRectF(0, 0, qMax((int)width, data.srcWidth), qMax((int)height, data.srcHeight)).toRect();
		imageRect.moveCenter(imageCenterPt);

		QRect intersectionRect = calcIntersectionRect(imageRect, QRect(0, 0, data.srcWidth, data.srcHeight));

		data.srcRectX = static_cast<long>(intersectionRect.x() - imageRect.x()) / double(imageRect.width());
		data.srcRectY = static_cast<long>(intersectionRect.y() - imageRect.y()) / double(imageRect.height());
		data.srcRectCX = intersectionRect.width() / double(imageRect.width());
		data.srcRectCY = intersectionRect.height() / double(imageRect.height());

		data.dstRectX = intersectionRect.x() / double(data.srcWidth);
		data.dstRectY = intersectionRect.y() / double(data.srcHeight);
		data.dstRectCX = intersectionRect.width() / double(data.srcWidth);
		data.dstRectCY = intersectionRect.height() / double(data.srcHeight);
	} else {
		auto &data = PLS_VIRTUAL_BG_MANAGER->getVirtualData();

		QRectF srcRect = calcImageViewport(data.zoomMin, QRectF(0, 0, data.resrcWidth, data.resrcHeight), QRectF(0, 0, data.srcWidth, data.srcHeight));
		data.zoomMax = data.zoomMin * 3;
		data.zoomValue = data.zoomMin;

		data.srcRectX = srcRect.x();
		data.srcRectY = srcRect.y();
		data.srcRectCX = srcRect.width();
		data.srcRectCY = srcRect.height();

		data.dstRectX = 0;
		data.dstRectY = 0;
		data.dstRectCX = 1;
		data.dstRectCY = 1;
	}

	PLS_VIRTUAL_BG_MANAGER->updateDataToCore();
}

#define BOOL_To_STR(x) (x) ? "true" : "false"

void PLSVirtualBackgroundDialog::onFlipVertiButtonClicked()
{
	auto &data = PLS_VIRTUAL_BG_MANAGER->getVirtualData();
	data.isFlipVerti = !data.isFlipVerti;

	auto logStr = QString("%1 isOn:%2").arg(__FUNCTION__).arg(BOOL_To_STR(data.isFlipVerti));
	PLS_UI_STEP(s_virtualBackground, logStr.toStdString().c_str(), ACTION_CLICK);

	pls_flush_style(ui->flipVButton, s_isNormalModel, data.isFlipVerti);
	PLS_VIRTUAL_BG_MANAGER->updateDataToCore();
}

void PLSVirtualBackgroundDialog::onFlipHoriButtonClicked()
{
	auto &data = PLS_VIRTUAL_BG_MANAGER->getVirtualData();
	data.isFlipHori = !data.isFlipHori;

	auto logStr = QString("%1 isOn:%2").arg(__FUNCTION__).arg(BOOL_To_STR(data.isFlipHori));
	PLS_UI_STEP(s_virtualBackground, logStr.toStdString().c_str(), ACTION_CLICK);

	pls_flush_style(ui->flipHButton, s_isNormalModel, data.isFlipHori);

	PLS_VIRTUAL_BG_MANAGER->updateDataToCore();
}

void PLSVirtualBackgroundDialog::onResetImageData(bool needSendToCore)
{
	PLS_INFO(s_virtualBackground, __FUNCTION__);

	SliderValueInitHelper sliderValueInitHelper(sliderValueInit);

	PLS_VIRTUAL_BG_MANAGER->resetImageEditData(PLS_VIRTUAL_BG_MANAGER->getCurrentSceneItem(), needSendToCore);

	updateImageEditUI();

	const auto &data = PLS_VIRTUAL_BG_MANAGER->getVirtualData();
	int zoomMin = zoomValueToSliderValue(data.zoomMin), zoomMax = zoomValueToSliderValue(data.zoomMax);
	setEditSliderRange(zoomMin, zoomMax);

	ui->zoomSlider->setValue(zoomValueToSliderValue(data.zoomValue));
	ui->zoomSlider->setEnabled(true);
	pls_flush_style(ui->zoomSlider, STATUS_HANDLE, ui->zoomSlider->isEnabled());
}

void PLSVirtualBackgroundDialog::setEditSliderRange(int zoomMin, int zoomMax)
{
	ui->zoomSlider->setRange(zoomMin, zoomMax);
	if (zoomMax > zoomMin) {
		int singleStep = (zoomMax - zoomMin) / 100;
		ui->zoomSlider->setSingleStep(singleStep);
	}
}

void PLSVirtualBackgroundDialog::onCloseEditButtonClicked()
{
	PLS_UI_STEP(s_virtualBackground, __FUNCTION__, ACTION_CLICK);
	PLS_VIRTUAL_BG_MANAGER->leaveImageEditMode(false);
	auto &data = PLS_VIRTUAL_BG_MANAGER->getVirtualData();
	data.isImageEditMode = false;
	updateSwitchImageEditModelUI();
	PLS_VIRTUAL_BG_MANAGER->updateDataToCore();
}

void PLSVirtualBackgroundDialog::onSaveEditButtonClicked()
{
	PLS_UI_STEP(s_virtualBackground, __FUNCTION__, ACTION_CLICK);
	PLS_VIRTUAL_BG_MANAGER->leaveImageEditMode(true);
	auto &data = PLS_VIRTUAL_BG_MANAGER->getVirtualData();
	data.isImageEditMode = false;
	updateSwitchImageEditModelUI();
	PLS_VIRTUAL_BG_MANAGER->updateDataToCore();
}

void PLSVirtualBackgroundDialog::onOriginBackgroundClicked()
{
	PLS_UI_STEP(s_virtualBackground, __FUNCTION__, ACTION_CLICK);

	auto &data = PLS_VIRTUAL_BG_MANAGER->getVirtualData();
	if (data.bgModelType == PLSVrBgModel::Original) {
		return;
	}
	PLS_VIRTUAL_BG_MANAGER->leaveImageEditMode(false);
	onResetImageData(false);
	data.bgModelType = PLSVrBgModel::Original;
	data.isImageEditMode = false;

	updateSwitchImageEditModelUI();
	updateBackgroundUI();
	updateBlurPageUI();
	PLS_VIRTUAL_BG_MANAGER->updateDataToCore();
	PLS_VIRTUAL_BG_MANAGER->vrCurrentModelChanged(PLS_VIRTUAL_BG_MANAGER->getCurrentSource());
}

void PLSVirtualBackgroundDialog::onClearBackgroundButtonClicked()
{
	PLS_UI_STEP(s_virtualBackground, __FUNCTION__, ACTION_CLICK);
	auto &data = PLS_VIRTUAL_BG_MANAGER->getVirtualData();
	if (data.bgModelType == PLSVrBgModel::BgDelete) {
		return;
	}
	PLS_VIRTUAL_BG_MANAGER->leaveImageEditMode(false);
	onResetImageData(false);
	data.bgModelType = PLSVrBgModel::BgDelete;
	data.isImageEditMode = false;

	updateSwitchImageEditModelUI();
	updateBackgroundUI();
	updateBlurPageUI();
	PLS_VIRTUAL_BG_MANAGER->updateDataToCore();
	PLS_VIRTUAL_BG_MANAGER->vrCurrentModelChanged(PLS_VIRTUAL_BG_MANAGER->getCurrentSource());
}

void PLSVirtualBackgroundDialog::onCurrentMotionListResourceChanged(const QString &itemId, int type, const QString &resourcePath, const QString &staticImgPath, const QString &thumbnailPath,
								    bool prismResource, const QString &foregroundPath, const QString &foregroundStaticImgPath)
{
	if (itemId.isEmpty()) {
		return;
	}
	auto _type = static_cast<MotionType>(type);
	if (_type == MotionType::UNKOWN) {
		return;
	}
	PLS_VIRTUAL_BG_MANAGER->leaveImageEditMode(false);
	onResetImageData(false);
	auto &data = PLS_VIRTUAL_BG_MANAGER->getVirtualData();
	data.bgModelType = PLSVrBgModel::OtherResource;
	data.thidrSourceType = _type;
	data.stopPath = staticImgPath;
	data.originPath = resourcePath;
	data.thumbnailFilePath = thumbnailPath;
	data.foregroundStaticPath = foregroundStaticImgPath;
	data.foregroundPath = foregroundPath;
	data.motionItemID = itemId;
	data.isPrismResource = prismResource;
	data.isImageEditMode = false;

	SliderValueInitHelper sliderValueInitHelper(sliderValueInit);
	QSize mediaSize;
	if (pls_get_media_size(mediaSize, _type == MotionType::STATIC || data.isMotionEnable ? data.originPath.toUtf8().constData() : data.stopPath.toUtf8().constData())) {
		data.isValid = true;

		data.resrcWidth = mediaSize.width();
		data.resrcHeight = mediaSize.height();

		QRectF srcRect = calcImageViewport(data.zoomMin, QRectF(0, 0, mediaSize.width(), mediaSize.height()), QRectF(0, 0, data.srcWidth, data.srcHeight));
		data.zoomMax = data.zoomMin * 3;
		data.zoomValue = data.zoomMin;

		data.srcRectX = srcRect.x();
		data.srcRectY = srcRect.y();
		data.srcRectCX = srcRect.width();
		data.srcRectCY = srcRect.height();

		int zoomMin = zoomValueToSliderValue(data.zoomMin), zoomMax = zoomValueToSliderValue(data.zoomMax);
		setEditSliderRange(zoomMin, zoomMax);
		ui->zoomSlider->setValue(zoomValueToSliderValue(data.zoomValue));
		ui->zoomSlider->setEnabled(true);
	} else {
		data.isValid = false;
		data.srcRectX = data.srcRectY = data.srcRectCX = data.srcRectCY = 0;
		ui->zoomSlider->setEnabled(false);
	}

	data.dstRectX = 0;
	data.dstRectY = 0;
	data.dstRectCX = 1;
	data.dstRectCY = 1;
	pls_flush_style(ui->zoomSlider, STATUS_HANDLE, ui->zoomSlider->isEnabled());

	updateSwitchImageEditModelUI();
	updateBackgroundUI(true);
	updateBlurPageUI();
	PLS_VIRTUAL_BG_MANAGER->updateDataToCore();
	PLS_VIRTUAL_BG_MANAGER->vrCurrentModelChanged(PLS_VIRTUAL_BG_MANAGER->getCurrentSource());
}

void PLSVirtualBackgroundDialog::onCurrentResourceFileIsInvaild(const QStringList &list)
{
	const auto &data = PLS_VIRTUAL_BG_MANAGER->getVirtualData();
	if (data.bgModelType == PLSVrBgModel::OtherResource && !data.motionItemID.isEmpty() && list.contains((data.motionItemID))) {
		PLS_INFO(s_virtualBackground, __FUNCTION__);
		onOriginBackgroundClicked();
	}
	PLSVirtualBgManager::checkResourceInvalid(list, "");
}

static void DrawLine(float x1, float y1, float x2, float y2, float thickness, vec2 scale)
{
	float ySide = (y1 == y2) ? (y1 < 0.5f ? 1.0f : -1.0f) : 0.0f;
	float xSide = (x1 == x2) ? (x1 < 0.5f ? 1.0f : -1.0f) : 0.0f;

	gs_render_start(true);

	gs_vertex2f(x1, y1);
	gs_vertex2f(x1 + (xSide * (thickness / scale.x)), y1 + (ySide * (thickness / scale.y)));
	gs_vertex2f(x2 + (xSide * (thickness / scale.x)), y2 + (ySide * (thickness / scale.y)));
	gs_vertex2f(x2, y2);
	gs_vertex2f(x1, y1);

	gs_vertbuffer_t *line = gs_render_save();

	gs_load_vertexbuffer(line);
	gs_draw(GS_TRISTRIP, 0, 0);
	gs_vertexbuffer_destroy(line);
}

void PLSVirtualBackgroundDialog::drawBorder(void *, uint32_t cx, uint32_t cy, float)
{
	gs_projection_push();
	gs_ortho(0.0f, float(cx), 0.0f, float(cy), -100.0f, 100.0f);

	gs_matrix_push();
	gs_matrix_identity();

	gs_effect_t *solid = obs_get_base_effect(OBS_EFFECT_SOLID);
	gs_technique_t *tech = gs_effect_get_technique(solid, "Solid");

	vec4 color;
	vec4_set(&color, 1.0f, 1.0f, 0.0f, 1.0f);
	gs_effect_set_vec4(gs_effect_get_param_by_name(solid, "color"), &color);

	gs_technique_begin(tech);
	gs_technique_begin_pass(tech, 0);

	struct vec2 scale2 = {1.0, 1.0};
	DrawLine(0.0, 0.0, cx, 0.0, 2.0, scale2);
	DrawLine(0.0, 0.0, 0.0, cy, 2.0, scale2);
	DrawLine(0.0, cy, cx, cy, 2.0, scale2);
	DrawLine(cx, 0.0, cx, cy, 2.0, scale2);
	gs_load_vertexbuffer(nullptr);

	gs_technique_end_pass(tech);
	gs_technique_end(tech);

	gs_matrix_pop();
	gs_projection_pop();
}

void PLSVirtualBackgroundDialog::drawBackground()
{
	if (!bg_texture) {
		QDir appDir(qApp->applicationDirPath());
		QString path = appDir.absoluteFilePath(QString("data/prism-studio/images/img-vbg-bgdelete-large.png"));
		std::string file_path = path.toStdString();
		bg_texture = gs_texture_create_from_file(file_path.c_str());
	}

	if (!bg_texture) {
		return;
	}

	gs_projection_push();
	gs_matrix_push();
	gs_matrix_identity();

	gs_rect rt = {0};
	gs_get_viewport(&rt);
	gs_ortho(0.0f, float(rt.cx), 0.0f, float(rt.cy), -100.0f, 100.0f);

	gs_effect_t *effect = obs_get_base_effect(OBS_EFFECT_REPEAT);
	gs_technique_t *tech = gs_effect_get_technique(effect, "Draw");

	gs_technique_begin(tech);
	gs_technique_begin_pass(tech, 0);

	uint32_t tex_w = gs_texture_get_width(bg_texture);
	uint32_t tex_h = gs_texture_get_height(bg_texture);

	struct vec2 tex_scale = {1.0};
	tex_scale.x = (float)rt.cx / (float)tex_w;
	tex_scale.y = (float)rt.cy / (float)tex_h;

	gs_effect_set_texture(gs_effect_get_param_by_name(effect, "image"), bg_texture);
	gs_effect_set_vec2(gs_effect_get_param_by_name(effect, "scale"), &tex_scale);
	gs_draw_sprite(bg_texture, 0, rt.cx, rt.cy);

	gs_technique_end_pass(tech);
	gs_technique_end(tech);

	gs_projection_pop();
	gs_matrix_pop();
}

void PLSVirtualBackgroundDialog::drawPreview(void *data, uint32_t cx, uint32_t cy)
{
	PLSVirtualBackgroundDialog *window = static_cast<PLSVirtualBackgroundDialog *>(data);
	if (!window)
		return;

	auto source = PLS_VIRTUAL_BG_MANAGER->getCurrentSource();
	if (!source)
		return;

	uint32_t sourceCX = max(obs_source_get_width(source), 1u);
	uint32_t sourceCY = max(obs_source_get_height(source), 1u);

	auto &vdata = PLS_VIRTUAL_BG_MANAGER->getVirtualData();
	if ((uint32_t(vdata.srcWidth) != sourceCX) || (uint32_t(vdata.srcHeight) != sourceCY)) {
		vdata.srcWidth = sourceCX;
		vdata.srcHeight = sourceCY;
	}

	int x, y;
	int newCX, newCY;
	float scale;

	GetScaleAndCenterPos(sourceCX, sourceCY, cx, cy, x, y, scale);

	newCX = int(scale * float(sourceCX));
	newCY = int(scale * float(sourceCY));

	window->renderViewportRect = QRect(0, 0, newCX, newCY);

	gs_viewport_push();
	gs_projection_push();
	gs_set_viewport(x, y, newCX, newCY);
	gs_ortho(0.0f, float(sourceCX), 0.0f, float(sourceCY), -100.0f, 100.0f);

	if (vdata.bgModelType == PLSVrBgModel::BgDelete) {
		drawBackground();
	}

	obs_source_video_render(source);
	gs_projection_pop();

	if (vdata.isImageEditMode) {
		// image edit mode, render yellow border
		drawBorder(data, newCX, newCY, scale);
	}

	gs_viewport_pop();
}

void PLSFrontendEvent(enum obs_frontend_event event, void *ptr)
{
	PLSVirtualBackgroundDialog *view = static_cast<PLSVirtualBackgroundDialog *>(ptr);
	if (!view) {
		return;
	}
	switch ((int)event) {
	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED:
	case OBS_FRONTEND_EVENT_SCENE_COPY:
		QMetaObject::invokeMethod(view, "onSceneChanged", Qt::QueuedConnection);
		break;
	case OBS_FRONTEND_EVENT_SCENE_LIST_CHANGED:
		//when rename source, not need set item to nullptr;
		QMetaObject::invokeMethod(view, "onSourceRenamed", Qt::QueuedConnection);
		break;
	}
}

void PLSVirtualBackgroundDialog::onSceneChanged()
{
	PLS_DEBUG(s_virtualBackground, __FUNCTION__);
	setCurrentSourceItem(nullptr);
	reloadVideoDatas();
}

void PLSVirtualBackgroundDialog::onSourceRenamed()
{
	PLS_DEBUG(s_virtualBackground, __FUNCTION__);
	reloadVideoDatas();
}

int PLSVirtualBackgroundDialog::getSourceComboboxVisibleCount()
{
	QListView *view = qobject_cast<QListView *>(ui->videosComboBox->view());
	if (!view) {
		return -1;
	}

	int count = 0;
	for (int i = 0; i < ui->videosComboBox->count(); i++) {
		if (view->isRowHidden(i)) {
			continue;
		}
		count++;
	}
	return count;
}

void PLSVirtualBackgroundDialog::closeDialogSave()
{
	emit virtualViewVisibleChanged(false);
	PLS_VIRTUAL_BG_MANAGER->leaveImageEditMode(false);
	isEnterTempOriginModel(false);
	clearSourceDisplay();
	PLS_VIRTUAL_BG_MANAGER->clearVecData();
}

void PLSVirtualBackgroundDialog::isEnterTempOriginModel(bool isTempModel)
{

	auto &data = PLS_VIRTUAL_BG_MANAGER->getVirtualData();
	if (data.isTempOriginModel == isTempModel || data.isImageEditMode) {
		return;
	}

	PLS_INFO(s_virtualBackground, __FUNCTION__ ":%s", BOOL_To_STR(isTempModel));

	data.isTempOriginModel = isTempModel;
	updateUI();
	PLS_VIRTUAL_BG_MANAGER->updateDataToCore();
}

void PLSVirtualBackgroundDialog::foundAndSetShouldSetSource()
{
	PLSBasic *main = reinterpret_cast<PLSBasic *>(App()->GetMainWindow());
	if (!main) {
		return;
	}
	//have select
	auto topSelect = main->getTopSelectDShowSceneItem();
	if (topSelect) {
		setCurrentIndex(topSelect);
		return;
	}

	////now select
	//auto nowSelect = PLS_VIRTUAL_BG_MANAGER->getCurrentSceneItem();
	//if (nowSelect && findText(nowSelect) >= 0) {
	//	setCurrentIndex(nowSelect);
	//	return;
	//}

	//top no selct
	auto topItem = main->getTopDShowSceneItem();
	if (topItem) {
		setCurrentIndex(topItem);
		return;
	}

	//no source
	ui->videosComboBox->setCurrentIndex(0);
	onSourceComboxIndexChanged(0);
}

void PLSVirtualBackgroundDialog::showEvent(QShowEvent *event)
{
	__super::showEvent(event);
	App()->getMainView()->updateSideBarButtonStyle(ConfigId::VirtualbackgroundConfig, true);
}

void PLSVirtualBackgroundDialog::hideEvent(QHideEvent *event)
{
	App()->getMainView()->updateSideBarButtonStyle(ConfigId::VirtualbackgroundConfig, false);
	closeDialogSave();
	__super::hideEvent(event);
}

void PLSVirtualBackgroundDialog::closeEvent(QCloseEvent *event)
{
	__super::closeEvent(event);
}

void PLSVirtualBackgroundDialog::reloadVideoDatas(OBSSceneItem forceShowItem)
{
	isEnterTempOriginModel(false);

	QString name{};
	OBSSceneItem item;
	DShowSourceVecType sourceList;
	PLSBasic *main = reinterpret_cast<PLSBasic *>(App()->GetMainWindow());
	if (!main) {
		return;
	}
	main->GetSelectDshowSourceAndList(name, item, sourceList);

	for (auto iter = sourceList.begin(); iter != sourceList.end();) {
		bool isValid = PLS_VIRTUAL_BG_MANAGER->isDShowSourceAvailable(iter->second);
		if (isValid) {
			++iter;
			continue;
		}
		if (forceShowItem != nullptr && iter->second == forceShowItem) {
			++iter;
			continue;
		}
		iter = sourceList.erase(iter);
	}
	reloadVideoComboboxList(sourceList, forceShowItem);
}

template<typename T> void PLSVirtualBackgroundDialog::initSlider(QSlider *slider, const T &min, const T &max, const T &step, const T &currentValue, Qt::Orientation ori)
{
	if (!slider) {
		return;
	}
	slider->setMinimum(min);
	slider->setMaximum(max);
	slider->setPageStep(step);
	slider->setOrientation(ori);
	slider->setValue(currentValue);
}

void PLSVirtualBackgroundDialog::showChromakeyToast()
{
	if (m_chromakeyToast == nullptr) {
		m_chromakeyToast = new PLSMotionErrorLayer(ui->preview, 3000);
		m_chromakeyToast->setToastText(tr("virtual.chromakey.used.tip"));
	}
	m_chromakeyToast->showView();
	adjustChromakeyTipSize();
}
void PLSVirtualBackgroundDialog::hideChromakeyToast()
{
	if (m_chromakeyToast && !m_chromakeyToast->isHidden()) {
		m_chromakeyToast->hiddenView();
	}
}
void PLSVirtualBackgroundDialog::adjustChromakeyTipSize(int superWidth)
{
	if (m_chromakeyToast == nullptr || m_chromakeyToast->isHidden() || m_chromakeyToast->parentWidget() == nullptr) {
		return;
	}
	if (superWidth <= 0) {
		superWidth = ui->preview->size().width();
	}
	auto gap = PLSDpiHelper::calculate(this, 15);
	const static int s_chromakeyToastHeight = 38;
	const static int s_chromakeyToastWeight = 280;
	int tipWidth = PLSDpiHelper::calculate(this, s_chromakeyToastWeight);
	int leftP = (superWidth - tipWidth) * 0.5;
	QPoint pt = ui->preview->mapToGlobal(QPoint(leftP, gap));
	m_chromakeyToast->setGeometry(pt.x(), pt.y(), tipWidth, PLSDpiHelper::calculate(this, s_chromakeyToastHeight));
}

void PLSVirtualBackgroundDialog::showFPSToast()
{
	if (m_fpsToast == nullptr) {
		m_fpsToast = new PLSMotionErrorLayer(ui->preview, 3000);
		m_fpsToast->setToastWordWrap(true);
		m_fpsToast->setToastText(tr("virtual.fps.adjust.tip"));
	}
	if (adjustFPSTipSize()) {
		m_fpsToast->showView();
	} else {
		delayShowFpsToast = true;
	}
}

void PLSVirtualBackgroundDialog::hideFPSToast()
{
	if (m_fpsToast && !m_fpsToast->isHidden()) {
		m_fpsToast->hiddenView();
	}
}

bool PLSVirtualBackgroundDialog::adjustFPSTipSize(int superWidth /*= 0*/)
{
	if (m_fpsToast == nullptr /*|| m_fpsToast->isHidden()*/ || m_fpsToast->parentWidget() == nullptr) {
		return false;
	}
	if (superWidth <= 0) {
		superWidth = ui->preview->size().width();
	}
	auto gap = PLSDpiHelper::calculate(this, 15);
	const static int s_fpsToastHeight = 38;
	const static int s_fpsToastWeight = 300;

	int tipWidth = PLSDpiHelper::calculate(this, s_fpsToastWeight);
	if (superWidth < tipWidth) {
		return false;
	}
	int leftP = (superWidth - tipWidth) * 0.5;
	QPoint pt = ui->preview->mapToGlobal(QPoint(leftP, gap));
	m_fpsToast->setGeometry(pt.x(), pt.y(), tipWidth, PLSDpiHelper::calculate(this, s_fpsToastHeight));

	if (delayShowFpsToast) {
		m_fpsToast->showView();
		delayShowFpsToast = false;
	}
	return true;
}

bool PLSVirtualBackgroundDialog::eventFilter(QObject *obj, QEvent *event)
{
	if (obj == ui->preview) {
		if (event->type() == QEvent::Resize) {
			adjustChromakeyTipSize(static_cast<QResizeEvent *>(event)->size().width());
			//adjustFPSTipSize(static_cast<QResizeEvent *>(event)->size().width());
		} else if (event->type() == QEvent::Move) {
			adjustChromakeyTipSize();
			//adjustFPSTipSize();
		}
	}
	return PLSDialogView::eventFilter(obj, event);
}

void PLSVirtualBackgroundDialog::moveEvent(QMoveEvent *event)
{
	PLSDialogView::moveEvent(event);

	if (m_motionViewCreated) {
		adjustChromakeyTipSize();
		//adjustFPSTipSize();
		//ui->motionListView->adjustErrTipSize();
	} else {
		QMetaObject::invokeMethod(
			this,
			[this]() {
				adjustChromakeyTipSize();
				//adjustFPSTipSize();
				//ui->motionListView->adjustErrTipSize();
			},
			Qt::QueuedConnection);
	}
}
