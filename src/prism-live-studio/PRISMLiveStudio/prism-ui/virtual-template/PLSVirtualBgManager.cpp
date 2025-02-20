#include "PLSVirtualBgManager.h"
#include "pls-common-define.hpp"
#include "window-basic-main.hpp"
#include "liblog.h"
#include "PLSBasic.h"

using namespace std;
using namespace common;
const char *const s_virtualBackground = "virtualBackground";

static const char *const s_method = "method";
static const char *const s_resrc_width = "_resrc_width";
static const char *const s_resrc_height = "_resrc_height";
static const char *const s_blur = "blur";
static const char *const s_blur_enable = "blur_enable";
static const char *const s_zoom_value = "_zoom_value";
static const char *const s_src_rect_x = "src_rect_x";
static const char *const s_src_rect_y = "src_rect_y";
static const char *const s_src_rect_cx = "src_rect_cx";
static const char *const s_src_rect_cy = "src_rect_cy";
static const char *const s_dst_rect_x = "dst_rect_x";
static const char *const s_dst_rect_y = "dst_rect_y";
static const char *const s_dst_rect_cx = "dst_rect_cx";
static const char *const s_dst_rect_cy = "dst_rect_cy";
static const char *const s_bg_type = "bg_type";
static const char *const s_path = "path";                                     //video path || image
static const char *const s_stop_motion_file_path = "stop_motion_file_path";   //image path
static const char *const s_thumbnail_file_path = "thumbnail_file_path";       //thumbnail path
static const char *const s_foreground_path = "foreground_path";               //video path || image
static const char *const s_foreground_static_path = "foreground_static_path"; //image path
static const char *const s_motion = "motion";
static const char *const s_ui_motion_checked = "ui_motion_checked";
static const char *const s_horizontal_flip = "horizontal_flip";
static const char *const s_vertical_flip = "vertical_flip";
static const char *const s_background_select_type = "background_select_type";
static const char *const s_is_temp_origin = "is_temp_origin";

static const char *const s_ui_image_edit_mode = "ui_is_image_edit_mode";
static const char *const s_ck_capture = "ck_capture";

static const char *const s_ui_motion_id = "ui_motion_id";
static const char *const s_ui_is_prism_resource = "ui_is_prism_resource";
static const char *const s_ui_have_shown_chromakey_tip = "ui_have_shown_chromakey_tip";

static const char *const s_segmentation = "segmentation";
static const char *const s_type_video = "video";
static const char *const s_type_image = "image";

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

PLSVirtualBgManager *PLSVirtualBgManager::instance()
{
	static PLSVirtualBgManager _instance;
	return &_instance;
}

void PLSVirtualBgManager::updateSourceListDatas(const DShowSourceVecType &list)
{

	for (const auto &data : list) {
		if (isContainItem(data.second)) {
			continue;
		}

		auto da = std::pair(data.second, getVrDataByItem(data.second));
		m_vecDatas.emplace_back(da);
	}

	for (auto iter = m_vecDatas.begin(); iter != m_vecDatas.end();) {
		bool isFound = false;
		for (const auto &data : list) {
			if (data.second == iter->first) {
				isFound = true;
			}
		}
		if (isFound) {
			++iter;
			continue;
		}
		iter->first = nullptr;
		iter = m_vecDatas.erase(iter);
	}
}

PLSVirtualData &PLSVirtualBgManager::getVirtualData(OBSSceneItem sceneData)
{
	bool isContain = false;
	return getVirtualDataWithErrCode(&isContain, sceneData);
}

PLSVirtualData &PLSVirtualBgManager::getVirtualDataWithErrCode(bool *isContaain, OBSSceneItem sceneData)
{
	if (m_vecDatas.empty()) {
		*isContaain = false;
		return m_tempData;
	}

	OBSSceneItem _item = sceneData;
	if (_item == nullptr) {
		_item = getCurrentSceneItem();
	}
	if (_item == nullptr) {
		*isContaain = false;
		return m_tempData;
	}

	for (auto &pairData : m_vecDatas) {
		if (pairData.first == _item) {
			*isContaain = true;
			return pairData.second;
		}
	}

	*isContaain = false;
	return m_tempData;
}

OBSSceneItem PLSVirtualBgManager::getCurrentSceneItem()
{
	return pls_get_sceneitem_by_pointer_address(m_currentSceneItem);
}

OBSSource PLSVirtualBgManager::getCurrentSource()
{
	if (getCurrentSceneItem() == nullptr) {
		return nullptr;
	}
	OBSSource _sour = obs_sceneitem_get_source(getCurrentSceneItem());
	return _sour;
}

void PLSVirtualBgManager::setCurrentSceneItem(OBSSceneItem item)
{
	m_currentSceneItem = item;
}

void PLSVirtualBgManager::resetImageEditData(OBSSceneItem item, bool needSendToCore)
{
	auto &data = getVirtualData(item);

	calcImageImageReset(data);
	if (needSendToCore) {
		updateDataToCore();
	}
}

bool PLSVirtualBgManager::isNoSourcePage()
{
	bool isNoSource = getCurrentSceneItem() == nullptr;
	return isNoSource;
}

void PLSVirtualBgManager::clearVecData()
{
	m_vecDatas.clear();
}

void PLSVirtualBgManager::clearData()
{
	m_vecDatas.clear();
	m_currentSceneItem = nullptr;
}

bool PLSVirtualBgManager::isDShowSourceAvailable(OBSSceneItem item)
{
	return isDShowSourceVisible(item);
}

bool PLSVirtualBgManager::isDShowSourceVisible(OBSSceneItem item)
{
	OBSSource source = obs_sceneitem_get_source(item);
	if (!source) {
		return false;
	}

	if (!item) {
		return false;
	}

	return obs_sceneitem_visible(item);
}

void PLSVirtualBgManager::calcImageImageReset(PLSVirtualData &data)
{
	data.isFlipHori = data.isFlipVerti = false;

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

void PLSVirtualBgManager::copyImageEditData(const PLSVirtualData &fromData, PLSVirtualData &toData)
{

	toData.isFlipHori = fromData.isFlipHori;
	toData.isFlipVerti = fromData.isFlipVerti;

	toData.zoomMin = fromData.zoomMin;
	toData.zoomMax = fromData.zoomMax;
	toData.zoomValue = fromData.zoomValue;

	toData.srcRectX = fromData.srcRectX;
	toData.srcRectY = fromData.srcRectY;
	toData.srcRectCX = fromData.srcRectCX;
	toData.srcRectCY = fromData.srcRectCY;

	toData.dstRectX = fromData.dstRectX;
	toData.dstRectY = fromData.dstRectX;
	toData.dstRectCX = fromData.dstRectX;
	toData.dstRectCY = fromData.dstRectX;
}

bool PLSVirtualBgManager::isCurrentSourceExisted(const OBSSceneItem item) const
{
	return item != nullptr;
}

void PLSVirtualBgManager::enterImageEditMode()
{
	auto nowSceneItem = getCurrentSceneItem();
	if (nowSceneItem == nullptr) {
		return;
	}

	PLSVirtualData data = getVirtualData();
	m_enterimageEditData = make_pair(nowSceneItem, data);
}

void PLSVirtualBgManager::leaveImageEditMode(bool isSaved)
{
#define BOOL_To_STR(x) (x) ? "true" : "false"
	if (isSaved) {
		//PLS_INFO(s_virtualBackground, __FUNCTION__ "  willSave:%s", BOOL_To_STR(isSaved));
		m_enterimageEditData = {};
		updateResolutionDataWhenApplyImageEdit(getCurrentSource(), getVirtualData());
		if (getVirtualData().isImageEditMode == true) {
			getVirtualData().isImageEditMode = false;
			updateDataToCore();
		}
		return;
	}
	auto nowSceneItem = getCurrentSceneItem();
	if (nowSceneItem == nullptr) {
		return;
	}

	auto oldSceneItem = pls_get_sceneitem_by_pointer_address(m_enterimageEditData.first);
	if (oldSceneItem == nullptr) {
		return;
	}

	bool isContainData = false;
	const PLSVirtualData &vrData = getVirtualDataWithErrCode(&isContainData, nowSceneItem);

	if (vrData.isImageEditMode == false && isContainData) {
		return;
	}
	//PLS_INFO(s_virtualBackground, __FUNCTION__ "  willSave:%s", BOOL_To_STR(isSaved));

	PLSVirtualData _tempData = m_enterimageEditData.second;
	m_enterimageEditData = {};
	_tempData.isImageEditMode = false;
	updateDataToCore(oldSceneItem, true, _tempData);
	updateDataFromCore(oldSceneItem);
};

void PLSVirtualBgManager::leaveNotSavedImageEditMode(const OBSSceneItem item)
{
	if (item == nullptr) {
		return;
	}
	auto &data = getVirtualData(item);

	if (data.isImageEditMode == false) {
		return;
	}
	data.isImageEditMode = false;
	PLS_INFO(s_virtualBackground, __FUNCTION__);
	updateDataToCore(pls_get_sceneitem_by_pointer_address(item));
	updateDataFromCore(item);
}

void PLSVirtualBgManager::sourceIsDeleted(const QString &itemId, const QStringList &list)
{
	QStringList _strList = list;
	if (!itemId.isEmpty()) {
		_strList << itemId;
	}
	if (_strList.isEmpty()) {
		return;
	}

	auto main = PLSBasic::instance();
	if (!main) {
		return;
	}

	PLSVirtualBgManager::checkResourceInvalid(_strList, "");
}

bool PLSVirtualBgManager::isContainItem(const OBSSceneItem item) const
{

	if (item == nullptr) {
		return false;
	}
	for (auto &data : m_vecDatas) {
		if (data.first == item) {
			return true;
		}
	}

	return false;
}

void PLSVirtualBgManager::updateDataToCore(OBSSceneItem sceneData, bool isSendData, const PLSVirtualData &sendData)
{

	OBSSceneItem _item = sceneData;
	if (_item == nullptr) {
		_item = getCurrentSceneItem();
	}
	if (_item == nullptr) {
		return;
	}
	const auto &vrData = isSendData ? sendData : getVirtualData(_item);
	OBSSource _sour = obs_sceneitem_get_source(_item);
	updateDataToCore(_sour, vrData);
}

static QString processResourcePath(bool prismResource, const QString &resourcePath)
{
	if (prismResource) {
		auto x = pls_get_relative_config_path(resourcePath);
		return x;
	}
	return resourcePath;
}

void PLSVirtualBgManager::updateDataToCore(const OBSSource _source, const PLSVirtualData &vrData) const
{
	OBSData data = pls_get_source_private_setting(_source);

	obs_data_set_bool(data, s_ui_image_edit_mode, vrData.isImageEditMode);

	obs_data_set_int(data, s_resrc_width, vrData.resrcWidth);
	obs_data_set_int(data, s_resrc_height, vrData.resrcHeight);

	obs_data_set_string(data, s_method, s_segmentation);

	obs_data_set_double(data, s_zoom_value, vrData.zoomValue);
	obs_data_set_double(data, s_src_rect_x, vrData.srcRectX);
	obs_data_set_double(data, s_src_rect_y, vrData.srcRectY);
	obs_data_set_double(data, s_src_rect_cx, vrData.srcRectCX);
	obs_data_set_double(data, s_src_rect_cy, vrData.srcRectCY);
	obs_data_set_double(data, s_dst_rect_x, vrData.dstRectX);
	obs_data_set_double(data, s_dst_rect_y, vrData.dstRectY);
	obs_data_set_double(data, s_dst_rect_cx, vrData.dstRectCX);
	obs_data_set_double(data, s_dst_rect_cy, vrData.dstRectCY);

	obs_data_set_string(data, s_bg_type, vrData.thidrSourceType == MotionType::MOTION ? s_type_video : s_type_image);

	bool motionEnable = false;
	if (vrData.bgModelType == PLSVrBgModel::OtherResource) {
		if (vrData.isPrismResource) {
			motionEnable = !vrData.isMotionUIChecked;
		} else {
			motionEnable = vrData.thidrSourceType == MotionType::MOTION;
		}
	}

	obs_data_set_bool(data, s_motion, motionEnable);
	obs_data_set_bool(data, s_ui_is_prism_resource, vrData.isPrismResource);
	obs_data_set_bool(data, s_ui_motion_checked, vrData.isMotionUIChecked);

	obs_data_set_bool(data, s_horizontal_flip, vrData.isFlipHori);
	obs_data_set_bool(data, s_vertical_flip, vrData.isFlipVerti);

	if (vrData.isTempOriginModel) {
		obs_data_set_int(data, s_background_select_type, static_cast<int>(PLSVrBgModel::Original));
		obs_data_set_bool(data, s_blur_enable, false);
		obs_data_set_int(data, s_blur, 0);
	} else {
		obs_data_set_int(data, s_background_select_type, static_cast<int>(vrData.bgModelType));
		bool uiCanClick = (vrData.bgModelType == PLSVrBgModel::Original || vrData.bgModelType == PLSVrBgModel::OtherResource);
		bool _blurEnable = vrData.blurValue > 0 && uiCanClick;
		obs_data_set_bool(data, s_blur_enable, _blurEnable);
		obs_data_set_int(data, s_blur, vrData.blurValue);
	}

	obs_data_set_string(data, s_ui_motion_id, vrData.motionItemID.toStdString().c_str());
	obs_data_set_string(data, s_path, processResourcePath(vrData.isPrismResource, vrData.originPath).toStdString().c_str());
	obs_data_set_string(data, s_stop_motion_file_path, processResourcePath(vrData.isPrismResource, vrData.stopPath).toStdString().c_str());
	obs_data_set_string(data, s_thumbnail_file_path, processResourcePath(vrData.isPrismResource, vrData.thumbnailFilePath).toStdString().c_str());
	obs_data_set_string(data, s_foreground_path, processResourcePath(vrData.isPrismResource, vrData.foregroundPath).toStdString().c_str());
	obs_data_set_string(data, s_foreground_static_path, processResourcePath(vrData.isPrismResource, vrData.foregroundStaticPath).toStdString().c_str());
	obs_data_set_int(data, s_is_temp_origin, vrData.isTempOriginModel);
	obs_data_set_bool(data, s_ui_have_shown_chromakey_tip, vrData.haveShownChromakeyTip);

	//obs_source_private_update(_source, data);
}

void PLSVirtualBgManager::updateDataFromCore(OBSSceneItem sceneData)
{
	if (sceneData == nullptr) {
		return;
	}

	for (auto &data : m_vecDatas) {
		if (data.first == sceneData) {
			data.second = getVrDataByItem(sceneData);
			break;
		}
	}
}

PLSVirtualData PLSVirtualBgManager::getVrDataByItem(OBSSceneItem sceneData) const
{
	if (sceneData == nullptr) {
		return PLSVirtualData();
	}
	OBSSource _source = obs_sceneitem_get_source(sceneData);
	return getVrDataByItem(_source);
}

PLSVirtualData PLSVirtualBgManager::getVrDataByItem(OBSSource _source) const
{
	auto data = PLSVirtualData();
	if (!_source) {
		return data;
	}

	OBSData settingData = pls_get_source_private_setting(_source);

	data.isImageEditMode = obs_data_get_bool(settingData, s_ui_image_edit_mode);

	data.resrcWidth = (int)obs_data_get_int(settingData, s_resrc_width);
	data.resrcHeight = (int)obs_data_get_int(settingData, s_resrc_height);
	data.srcWidth = obs_source_get_width(_source);
	data.srcHeight = obs_source_get_height(_source);

	data.isValid = (data.resrcWidth > 0) && (data.resrcHeight > 0);

	data.blurValue = static_cast<int>(obs_data_get_int(settingData, s_blur));

	data.zoomValue = obs_data_get_double(settingData, s_zoom_value);
	data.srcRectX = obs_data_get_double(settingData, s_src_rect_x);
	data.srcRectY = obs_data_get_double(settingData, s_src_rect_y);
	data.srcRectCX = obs_data_get_double(settingData, s_src_rect_cx);
	data.srcRectCY = obs_data_get_double(settingData, s_src_rect_cy);
	data.dstRectX = obs_data_get_double(settingData, s_dst_rect_x);
	data.dstRectY = obs_data_get_double(settingData, s_dst_rect_y);
	data.dstRectCX = obs_data_get_double(settingData, s_dst_rect_cx);
	data.dstRectCY = obs_data_get_double(settingData, s_dst_rect_cy);

	auto otherType = obs_data_get_string(settingData, s_bg_type);
	data.thidrSourceType = strcmp(otherType, s_type_video) == 0 ? MotionType::MOTION : MotionType::STATIC;
	data.originPath = obs_data_get_string(settingData, s_path);
	data.stopPath = obs_data_get_string(settingData, s_stop_motion_file_path);
	data.thumbnailFilePath = obs_data_get_string(settingData, s_thumbnail_file_path);
	data.foregroundPath = obs_data_get_string(settingData, s_foreground_path);
	data.foregroundStaticPath = obs_data_get_string(settingData, s_foreground_static_path);

	data.isMotionEnable = obs_data_get_bool(settingData, s_motion);
	data.isMotionUIChecked = obs_data_get_bool(settingData, s_ui_motion_checked);

	data.isFlipHori = obs_data_get_bool(settingData, s_horizontal_flip);
	data.isFlipVerti = obs_data_get_bool(settingData, s_vertical_flip);
	auto bgType = static_cast<int>(obs_data_get_int(settingData, s_background_select_type));
	data.bgModelType = static_cast<PLSVrBgModel>(bgType);
	data.motionItemID = obs_data_get_string(settingData, s_ui_motion_id);
	data.isPrismResource = obs_data_get_bool(settingData, s_ui_is_prism_resource);
	data.haveShownChromakeyTip = obs_data_get_bool(settingData, s_ui_have_shown_chromakey_tip);
	return data;
}

void PLSVirtualBgManager::checkResourceInvalid(const QStringList &itemIdList, const QString &sourceName)
{
	if (itemIdList.isEmpty() && sourceName.isEmpty()) {
		return;
	}

	auto main = PLSBasic::instance();
	if (!main) {
		return;
	}
	std::vector<OBSSource> vecSources;

	pls_get_all_source(vecSources);
	auto sourceNameUTF = sourceName.toUtf8();

	for (OBSSource source : vecSources) {
		const char *id = obs_source_get_id(source);
		if (id == nullptr || *id == '\0' /* || 0 != strcmp(id, DSHOW_SOURCE_ID)*/) {
			continue;
		}

		OBSData settingData = pls_get_source_private_setting(source);

		auto bgType = static_cast<int>(obs_data_get_int(settingData, s_background_select_type));
		if (static_cast<PLSVrBgModel>(bgType) != PLSVrBgModel::OtherResource) {
			continue;
		}

		auto ui_motion_id = obs_data_get_string(settingData, s_ui_motion_id);
		bool isIdNull = ui_motion_id == nullptr || *ui_motion_id == '\0';
		bool isItemIdInvaild = !isIdNull && itemIdList.contains(QString::fromUtf8(ui_motion_id));
		bool isSameSource = strcmp(obs_source_get_name(source), sourceNameUTF.constData()) == 0;

		if (isItemIdInvaild || isSameSource) {
			obs_data_set_string(settingData, s_ui_motion_id, "");
			obs_data_set_int(settingData, s_background_select_type, static_cast<int>(PLSVrBgModel::Original));
			//obs_source_private_update(source, settingData);
			//PLS_INFO(s_virtualBackground, __FUNCTION__ "  sourName:%s\t have invaild image , so change to origin background", obs_source_get_name(source));
		}
	}
	PLS_VIRTUAL_BG_MANAGER->vrCurrentModelChanged(pls_get_source_by_name(sourceNameUTF.constData()));
}

void PLSVirtualBgManager::checkResourceIsUsed(const QString &itemId, bool &vrBgContain, bool &bgSourceContain)
{
	if (itemId.isEmpty()) {
		return;
	}
	std::vector<OBSSource> vecSources;
	pls_get_all_source(vecSources);

	QByteArray itemIdUtf8 = itemId.toUtf8();

	for (OBSSource source : vecSources) {
		const char *id = obs_source_get_id(source);
		if (id == nullptr || *id == '\0') {
			continue;
		}

		if (!bgSourceContain && !strcmp(id, PRISM_BACKGROUND_TEMPLATE_SOURCE_ID)) {
			OBSData settingData = pls_get_source_setting(source);
			auto ui_motion_id = obs_data_get_string(settingData, "item_id");
			if (ui_motion_id && ui_motion_id[0]) {
				bgSourceContain = strcmp(ui_motion_id, itemIdUtf8.constData()) == 0;
			}
		}

		if (vrBgContain && bgSourceContain) {
			break;
		}
	}
}

void PLSVirtualBgManager::checkOtherSourceRunderProcessExit(const void *source)
{
	for (auto &data : getVecDatas()) {
		auto sceneItem = pls_get_sceneitem_by_pointer_address(data.first);
		if (!sceneItem) {
			continue;
		}
		auto _source = obs_sceneitem_get_source(sceneItem);
		if (!_source) {
			continue;
		}
		if (_source == source) {
			updateDataFromCore(sceneItem);
			auto vrData = getVrDataByItem(sceneItem);
			vrData.bgModelType = PLSVrBgModel::Original;
			vrData.isImageEditMode = false;
			updateDataToCore(sceneItem, true, vrData);
		}
	}
	PLS_VIRTUAL_BG_MANAGER->vrCurrentModelChanged(pls_get_source_by_pointer_address(source));
}

void PLSVirtualBgManager::vrSourcePropertyDialogOpen(const OBSSource source, const QSize &originResolution)
{
	if (!source) {
		return;
	}
	PLSVirtualData data;
	if (m_enterimageEditData.first && obs_sceneitem_get_source(pls_get_sceneitem_by_pointer_address(m_enterimageEditData.first)) == source) {
		//this is image edit mode to open property dialog, so use temp data.
		data = m_enterimageEditData.second;
	} else {
		data = getVrDataByItem(source);
	}
	if (data.bgModelType != PLSVrBgModel::OtherResource) {
		m_enterResolutionData = {};
		return;
	}
	data.enterResolutionSize = data.currentResolutionSize = originResolution;
	m_enterResolutionData = make_pair(source, data);
}

void PLSVirtualBgManager::vrSourceResolutionChanged(const OBSSource source, const QSize &currentResolution, const QSize &)
{
	if (!source) {
		return;
	}
	auto data = getVrDataByItem(source);
	if (data.bgModelType != PLSVrBgModel::OtherResource) {
		//now is not image, so ignore.
		m_enterResolutionData = {};
		return;
	}

	if (!m_enterResolutionData.first || m_enterResolutionData.first != source || m_enterResolutionData.second.motionItemID != data.motionItemID) {
		m_enterResolutionData = {};
	}

	if (m_enterResolutionData.first) {
		m_enterResolutionData.second.currentResolutionSize = currentResolution;
		if (m_enterResolutionData.second.enterResolutionSize == m_enterResolutionData.second.currentResolutionSize) {
			//reback to old resolution
			copyImageEditData(m_enterResolutionData.second, data);
		} else {
			calcImageImageReset(data);
		}
	} else {
		calcImageImageReset(data);
	}

	updateDataToCore(source, data);
	PLS_INFO(s_virtualBackground, __FUNCTION__);
}

void PLSVirtualBgManager::vrSourcePropertyDialogButtonClick(const OBSSource source, bool isOkClick, const QSize &originalResolution, const QSize &currentResolution)
{
	if (!source || isOkClick) {
		m_enterResolutionData = {};
		return;
	}

	if (originalResolution == currentResolution) {
		m_enterResolutionData = {};
		return;
	}

	auto data = getVrDataByItem(source);

	if (data.bgModelType != PLSVrBgModel::OtherResource) {
		m_enterResolutionData = {};
		return;
	}

	if (m_enterResolutionData.first && m_enterResolutionData.second.motionItemID == data.motionItemID) {
		PLSVirtualBgManager::copyImageEditData(m_enterResolutionData.second, data);
	} else {
		calcImageImageReset(data);
	}

	updateDataToCore(source, data);
	m_enterResolutionData = {};
	PLS_INFO(s_virtualBackground, __FUNCTION__);
}

void PLSVirtualBgManager::vrCurrentModelChanged(const OBSSource source)
{
	if (!source || !m_enterResolutionData.first || m_enterResolutionData.first != source) {
		return;
	}
	auto data = getVrDataByItem(source);

	if (data.motionItemID != m_enterResolutionData.second.motionItemID) {
		m_enterResolutionData = {};
	}
}

void PLSVirtualBgManager::updateResolutionDataWhenApplyImageEdit(const OBSSource source, PLSVirtualData sendData)
{
	//when image edit click apply, should update data to resolution temp data.
	if (!source || !m_enterResolutionData.first || m_enterResolutionData.first != source) {
		return;
	}

	if (m_enterResolutionData.second.enterResolutionSize != m_enterResolutionData.second.currentResolutionSize) {
		return;
	}
	//only same resolution will update enter data
	if (sendData.bgModelType != PLSVrBgModel::OtherResource || m_enterResolutionData.second.motionItemID != sendData.motionItemID) {
		m_enterResolutionData = {};
		return;
	}

	sendData.currentResolutionSize = m_enterResolutionData.second.currentResolutionSize;
	sendData.enterResolutionSize = m_enterResolutionData.second.enterResolutionSize;
	m_enterResolutionData.second = sendData;
	PLS_INFO(s_virtualBackground, __FUNCTION__);
}
