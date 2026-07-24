#include "properties-view.hpp"
#include "libutils-api.h"
#include "pls/pls-lens-info.h"
#include "pls/pls-lens-event.h"
#include "frontend-api.h"
#include "obs-app.hpp"
#include "pls-common-define.hpp"
#include "PLSLoadingButton.h"
#include <QComboBox>
#if defined(Q_OS_WIN)
#include "util/windows/win-version.h"
#endif

using namespace common;

//------------------------------------------ windows util start -------------------------------------------
#if defined(Q_OS_WIN)
void RenameItemText(OBSSource source, QComboBox *combo, obs_property_t *property, int lens, bool is_camera)
{
	if (!source || !combo || !property)
		return;

	if (lens < 0 || lens >= MAX_LENS_COUNT)
		return;

	for (size_t i = 0; i < combo->count(); ++i) {
		QVariant var = combo->itemData(i);
		std::string name = "";

		if (is_camera) {
			QString str = var.toString();
			auto index = OBSPropertiesView::GetLensIndexFromDeviceString(str);
			if (index != lens)
				continue;
		} else {
			int val = var.toInt();
			if (val != lens)
				continue;
		}

		name = OBSPropertiesView::GetDisplayDeviceName(property, i, source, true);
		combo->setItemText(i, QT_UTF8(name.c_str()));
		break;
	}
}

void RenameComboxItem(OBSSource source, QComboBox *combo, obs_property_t *property, int lens, bool actived)
{
	actived; // unused

	if (!source || !combo || !property)
		return;

	if (lens < 0 || lens >= MAX_LENS_COUNT)
		return;

	auto id = obs_source_get_id(source);
	if (!id)
		return;

	obs_combo_format format = obs_property_list_format(property);
	if (pls_is_equal(id, PRISM_LENS_SOURCE_ID) || pls_is_equal(id, PRISM_LENS_MOBILE_SOURCE_ID)) {
		assert(format == OBS_COMBO_FORMAT_INT);
		RenameItemText(source, combo, property, lens, false);
		return;
	}

	if (pls_is_equal(id, OBS_DSHOW_SOURCE_ID)) {
		assert(format == OBS_COMBO_FORMAT_STRING);
		RenameItemText(source, combo, property, lens, true);
		return;
	}
}

void SelectBestLens(QComboBox *combo)
{
	if (!combo)
		return;

	bool lens_exist = false;
	for (size_t i = 0; i < combo->count(); ++i) {
		QVariant var = combo->itemData(i);
		QString str = var.toString();
		int index = OBSPropertiesView::GetLensIndexFromDeviceString(str.toUtf8().constData());
		if (index < 0)
			break; // since lens device is always at the top of the list, so break when not lens device

		lens_exist = true;
		if (pls_is_lens_active(index)) {
			combo->setCurrentIndex(i); // select first active lens
			return;
		}
	}

	if (lens_exist && combo->count() > 0) {
		combo->setCurrentIndex(0); // select first lens if no active lens
	} else {
		// there is no lens device, do nothing
	}
}
// ----------------------------------------- MAC util start ------------------------------------------
#elif defined(Q_OS_MACOS)
static bool RenameLensIndexItem(OBSSource source, QComboBox *combo, obs_property_t *property, int lens)
{
	for (int i = 0; i < combo->count(); ++i) {
		QVariant data = combo->itemData(i);
		if (!data.canConvert<int>())
			continue;

		if (data.toInt() != lens)
			continue;

		std::string name = OBSPropertiesView::GetDisplayDeviceName(property, size_t(i), source, true);
		combo->setItemText(i, QT_UTF8(name.c_str()));
		return true;
	}

	return false;
}

static bool RenameCameraItem(OBSSource source, QComboBox *combo, obs_property_t *property, int lens)
{
	for (int i = 0; i < combo->count(); ++i) {
		QString deviceString = combo->itemData(i).toString();
		if (deviceString.isEmpty())
			continue;

		int index = OBSPropertiesView::GetLensIndexFromDeviceString(deviceString);
		if (index != lens)
			continue;

		std::string name = OBSPropertiesView::GetDisplayDeviceName(property, size_t(i), source, true);
		combo->setItemText(i, QT_UTF8(name.c_str()));
		return true;
	}

	return false;
}

void RenameComboxItem(OBSSource source, QComboBox *combo, obs_property_t *property, int lens, bool actived)
{
	Q_UNUSED(actived);

	if (!source || !combo || !property)
		return;

	if (lens < 0 || lens >= MAX_LENS_COUNT)
		return;

	const char *id = obs_source_get_id(source);
	if (!id)
		return;

	if (pls_is_equal(id, PRISM_LENS_SOURCE_ID) || pls_is_equal(id, PRISM_LENS_MOBILE_SOURCE_ID) ||
	    pls_is_equal(id, OBS_MACOS_VIDEO_CAPTURE_SOURCE_ID) || pls_is_equal(id, OBS_MACOS_CAPTURE_CARD_SOURCE_ID)) {
		obs_combo_format format = obs_property_list_format(property);
		assert(format == OBS_COMBO_FORMAT_STRING);
		RenameCameraItem(source, combo, property, lens);
		return;
	}
}

void SelectBestLens(QComboBox *combo)
{
	if (!combo)
		return;

	int firstLensIndex = -1;
	for (int i = 0; i < combo->count(); ++i) {
		// On Mac, item data is device UUID, but display text contains lens name
		// Note: first item is empty placeholder, so skip empty items
		QString deviceString = combo->itemData(i).toString();
		if (deviceString.isEmpty())
			continue;

		int lensIndex = OBSPropertiesView::GetLensIndexFromDeviceString(deviceString);
		if (lensIndex < 0)
			continue;

		if (firstLensIndex < 0)
			firstLensIndex = i;

		if (pls_is_lens_active(lensIndex)) {
			combo->setCurrentIndex(i); // select first active lens
			return;
		}
	}

	if (firstLensIndex >= 0) {
		combo->setCurrentIndex(firstLensIndex); // select first lens if no active lens
	}
}

#endif
//----------------------------------- windows/mac util end ----------------------------------------

void WidgetInfo::RegisterLensState(OBSSource source, bool is_video_device)
{
	if (!source || !is_video_device)
		return;

	//--------------------------------------------------------------------
	QPointer<QWidget> ptr(widget);
	auto active_hander = [this, ptr, source](int lens, bool actived) { // called from UI thread
		if (!ptr)
			return;

		QComboBox *combo = dynamic_cast<QComboBox *>(widget);
		if (!combo) {
			assert(false);
			return;
		}

#if defined(Q_OS_WIN)
		RenameComboxItem(source, combo, property, lens, actived);
#elif defined(Q_OS_MACOS)
		RenameComboxItem(source, combo, property, lens, actived);
#endif
	};

	auto active_cb = [=](int lens, bool actived) {
		if (!ptr)
			return;

		QMetaObject::invokeMethod(
			ptr,
			[=]() {
				if (!ptr)
					return;

				active_hander(lens, actived);
			},
			Qt::QueuedConnection);
	};

	LensEvents evts;
	evts.active_cb = active_cb;
	pls_register_lens_events(this, evts);
}

void WidgetInfo::UnregisterLensState()
{
	pls_unregister_lens_events(this);
}

OBSSource OBSPropertiesView::GetPropertySource()
{
	if (weakObj && rawObj)
		return nullptr;

	OBSObject strongObj = GetObject();
	void *obj = strongObj ? strongObj.Get() : rawObj;
	if (!obj)
		return nullptr;

	OBSSource source = pls_get_source_by_pointer_address(obj);
	if (!source)
		return nullptr;

	return source;
}

bool OBSPropertiesView::IsForLensDeviceList(OBSSource source, obs_property_t *property)
{
	if (!property || !source)
		return false;

	const char *name = obs_property_name(property);
	if (!name)
		return false;

	auto id = obs_source_get_id(source);
	if (!id)
		return false;

#if defined(Q_OS_WIN)
	if (pls_is_equal(id, PRISM_LENS_SOURCE_ID) || pls_is_equal(id, PRISM_LENS_MOBILE_SOURCE_ID))
		return pls_is_equal(name, LENSV2_VIDEO_INDEX);

	if (pls_is_equal(id, OBS_DSHOW_SOURCE_ID))
		return pls_is_equal(name, CSTR_VIDEO_DEVICE_ID);

	return false;
#elif defined(Q_OS_MACOS)
	if (pls_is_equal(id, PRISM_LENS_SOURCE_ID) || pls_is_equal(id, PRISM_LENS_MOBILE_SOURCE_ID) ||
	    pls_is_equal(id, OBS_MACOS_VIDEO_CAPTURE_SOURCE_ID) || pls_is_equal(id, OBS_MACOS_CAPTURE_CARD_SOURCE_ID)) {
		return pls_is_equal(name, CSTR_VIDEO_DEVICE_ID);
	}

	return false;
#else
	return false;
#endif
}

bool OBSPropertiesView::IsStateSupportedInLensApp()
{
	QString path;
	bool installed = pls_is_install_cam_studio(path);
	if (!installed)
		return false;

#if defined(Q_OS_WIN) // <= Windows-2.0.2, lens app does not support lens state
	win_version_info info;
	if (!get_dll_ver(path.toStdWString().c_str(), &info))
		return false;

	QString version = QString("%1.%2.%3").arg(info.major).arg(info.minor).arg(info.build);
	return pls_compare_version(version, "2.0.2") > 0;
#elif defined(Q_OS_MACOS)
	QString version = pls_libutil_api_mac::pls_get_app_version_by_identifier("com.prismlive.camstudio");
	if (version.isEmpty())
		return false;

	// <= 2.0.2 does not expose lens state
	return pls_compare_version(version, "2.0.2") > 0;
#else
	return false;
#endif
}

std::string OBSPropertiesView::GenerateLensName(const char *name, bool actived)
{
	if (!name) {
		assert(false);
		return "";
	}

	// old lens app does not support lens state
	if (!IsStateSupportedInLensApp())
		return name ? name : "";

	QString state = actived ? QTStr("main.property.prism.lens.actived")
				: QTStr("main.property.prism.lens.inactived");

	QString text = QString("%1 (%2)").arg(name).arg(state);
	return text.toStdString();
}

int OBSPropertiesView::GetLensIndexFromDeviceString(const QString &text)
{
	if (text.isEmpty())
		return -1;

	if (text.contains(QT_UTF8(CSTR_PRISM_LEN1), Qt::CaseInsensitive))
		return 0;
	if (text.contains(QT_UTF8(CSTR_PRISM_LEN2), Qt::CaseInsensitive))
		return 1;
	if (text.contains(QT_UTF8(CSTR_PRISM_LEN3), Qt::CaseInsensitive))
		return 2;

	return -1;
}

std::string OBSPropertiesView::GetDisplayDeviceName(obs_property_t *prop, size_t idx, OBSSource source,
						    bool videoDevice)
{
	if (!prop) {
		assert(false);
		return "";
	}

	const char *name = obs_property_list_item_name(prop, idx);
	if (!videoDevice || !source || !name)
		return name ? name : "";

	auto id = obs_source_get_id(source);
	if (!id)
		return name;

#if defined(Q_OS_WIN)
	obs_combo_format format = obs_property_list_format(prop);
	if (pls_is_equal(id, PRISM_LENS_SOURCE_ID) || pls_is_equal(id, PRISM_LENS_MOBILE_SOURCE_ID)) {
		assert(format == OBS_COMBO_FORMAT_INT);
		auto index = obs_property_list_item_int(prop, idx);
		bool actived = pls_is_lens_active(index);
		return GenerateLensName(name, actived);
	}
	if (pls_is_equal(id, OBS_DSHOW_SOURCE_ID)) {
		assert(format == OBS_COMBO_FORMAT_STRING);
		auto value = obs_property_list_item_string(prop, idx);
		auto index = GetLensIndexFromDeviceString(value ? value : "");
		if (index < 0 || index >= MAX_LENS_COUNT)
			return name;

		bool actived = pls_is_lens_active(index);
		return GenerateLensName(name, actived);
	}
	assert(false);
	return name;
#elif defined(Q_OS_MACOS)
	obs_combo_format format = obs_property_list_format(prop);
	if (pls_is_equal(id, PRISM_LENS_SOURCE_ID) || pls_is_equal(id, PRISM_LENS_MOBILE_SOURCE_ID) ||
	    pls_is_equal(id, OBS_MACOS_VIDEO_CAPTURE_SOURCE_ID) || pls_is_equal(id, OBS_MACOS_CAPTURE_CARD_SOURCE_ID)) {
		assert(format == OBS_COMBO_FORMAT_STRING);
		auto value = obs_property_list_item_string(prop, idx);
		int index = GetLensIndexFromDeviceString(QT_UTF8(value));
		if (index < 0 || index >= MAX_LENS_COUNT)
			return name;

		bool actived = pls_is_lens_active(index);
		return GenerateLensName(name, actived);
	}
	return name;
#else
	return name;
#endif
}

bool OBSPropertiesView::IsCameraSource()
{
	OBSSource source = GetPropertySource();
	if (!source)
		return false;

	auto id = obs_source_get_id(source);
	if (!id)
		return false;

#if defined(Q_OS_WIN)
	return (pls_is_equal(id, OBS_DSHOW_SOURCE_ID));
#elif defined(Q_OS_MACOS)
	return (pls_is_equal(id, OBS_MACOS_VIDEO_CAPTURE_SOURCE_ID) ||
		pls_is_equal(id, OBS_MACOS_CAPTURE_CARD_SOURCE_ID));
#else
	assert(false);
	return false;
#endif
}

void OBSPropertiesView::SelectLensDevice()
{
	if (!IsCameraSource()) {
		assert(false);
		return;
	}

	for (auto &child : children) {
		if (!child || !child->widget || !child->property)
			continue;

		QComboBox *combo = dynamic_cast<QComboBox *>(child->widget);
		if (!combo)
			continue;

		const char *name = obs_property_name(child->property);
		if (!name)
			continue;

#if defined(Q_OS_WIN)
		// check this property is for video device list of camera
		if (pls_is_equal(name, CSTR_VIDEO_DEVICE_ID)) {
			SelectBestLens(combo);
			break;
		}
#elif defined(Q_OS_MACOS)
		if (pls_is_equal(name, CSTR_VIDEO_DEVICE_ID)) {
			SelectBestLens(combo);
			break;
		}
#endif
	}
}

void OBSPropertiesView::OnCameraChanged(bool userOperation)
{
	if (!m_openLensBtn)
		return;

	if (!IsCameraSource() || !settings) {
		assert(false);
		return;
	}

	QString program;
	bool lensInstalled = pls_is_install_cam_studio(program);

#if defined(Q_OS_WIN)
	const char *device = obs_data_get_string(settings, CSTR_VIDEO_DEVICE_ID);
	if (!device) {
		m_openLensBtn->setLensSelected(false);
		return;
	}

	int lens = GetLensIndexFromDeviceString(device);
	bool lens_selected = (lens >= 0 && lens < MAX_LENS_COUNT);
	m_openLensBtn->setLensSelected(lens_selected);

	bool enabled;
	if (lens_selected) { // lens is selected
		enabled = lensInstalled ? true : false;
	} else { // lens is not selected
		enabled = true;
	}

	for (auto &child : children) {
		if (!child || !child->widget || !child->property)
			continue;

		const char *name = obs_property_name(child->property);
		if (!name)
			continue;

		if (pls_is_equal(name, "activate")) {
			child->widget->setEnabled(enabled);
			break;
		}
	}
#elif defined(Q_OS_MACOS)
	const char *device = obs_data_get_string(settings, CSTR_VIDEO_DEVICE_ID);
	if (!device || !*device) {
		m_openLensBtn->setLensSelected(false);
		return;
	}

	int lens = GetLensIndexFromDeviceString(device);
	bool lens_selected = (lens >= 0 && lens < MAX_LENS_COUNT);
	m_openLensBtn->setLensSelected(lens_selected);
#endif

	if (lens_selected && !lensInstalled && (showInstallLens || userOperation)) {
		showInstallLens = false;
		pls_async_call(this, [this]() { showLensInstallTips(false); });
	}
}