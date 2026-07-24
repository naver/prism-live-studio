#include "PLSPropertiesViewVbChromakey.h"
#include "PLSPropertiesView.hpp"
#include "pls/pls-lens-event.h"
#include "pls/pls-source.h"
#include "obs-frontend-api.h"
#include "frontend-api.h"
#include "libutils-api.h"
#include "PLSBasic.h"
#include "pls-common-define.hpp"
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QFontMetrics>
#include <QResizeEvent>

using namespace common;

#define CHROMA_KEY_FILTER_ID "chroma_key_filter_v2"

#define VB_TO_REMOVE "property.lens.vb.toRemoved"
#define VB_TO_ORIGIN "property.lens.vb.toOriginal"
#define VB_TO_ORIGIN_CONFIRM "property.lens.vb.toOriginal.confirm"

#define VB_LENS_TIP_NEED_UPGRADE "property.lens.tip.upgrade"
#define VB_LENS_TIP_NO_CAPTURE "property.lens.tip.nocapture"
#define VB_LENS_TIP_ACTIVE_DEVICE "property.lens.tip.activedevice"
#define VB_LENS_TIP_INSTALL "property.lens.tip.install"
#define VB_LENS_TIP_INSTALL_NOW "property.lens.tip.install.button"

#define CHROMAKEY_TO_ADD "property.lens.chromakey.toadd"
#define CHROMAKEY_TO_CLEAR "property.lens.chromakey.toclear"
#define CHROMAKEY_CONFIRM "property.lens.chromakey.confirm"

#define VB_CHROMAKEY_LABEL "property.lens.vb.label"
#define VB_CHROMAKEY_DESC1 "property.lens.vbchromakey.desc1"
#define VB_CHROMAKEY_DESC2 "property.lens.vbchromakey.desc2"
#define VB_CHROMAKEY_DESC3 "property.lens.vbchromakey.desc3"

#define PTS_OBJECT(key) pls_text_t(tr(key), OBSApp::getEnglishTranslateStringByKey(key))

//------------------------------------------------------------------------------------------------------------------------------
DescLabel::DescLabel(const QString &text, QWidget *container, QWidget *parent) : QLabel(text, parent), containerWidget(container) {}

void DescLabel::setText(const QString &text)
{
	QLabel::setText(text);
	UpdateHeight();
}

void DescLabel::resizeEvent(QResizeEvent *event)
{
	QLabel::resizeEvent(event);
	UpdateHeight();
}

void DescLabel::UpdateHeight()
{
	if (width() <= 0)
		return;

	auto labelText = text();
	if (labelText.isEmpty())
		return;

	QFontMetrics fm(font());
	QRect rect = fm.boundingRect(QRect(0, 0, width(), INT_MAX), Qt::TextWordWrap, labelText);

	int lineHeight = fm.lineSpacing();
	int lines = (rect.height() + lineHeight - 1) / lineHeight;

	const auto oneLineHeight = 18; // figma design
	auto fixedHeight = (lines <= 1) ? oneLineHeight : (lines * oneLineHeight);

	setFixedHeight(fixedHeight);
	if (containerWidget)
		containerWidget->setFixedHeight(fixedHeight);
}

//------------------------------------------------------------------------------------------------------------------------------
void PLSVbChromakey::OnSourceFilterAdded(void *param, calldata_t *data)
{
	PLSPropertiesView *window = reinterpret_cast<PLSPropertiesView *>(param);
	obs_source_t *filter = (obs_source_t *)calldata_ptr(data, "filter");
	QMetaObject::invokeMethod(window, "OnFilterAddOrRemove", Q_ARG(OBSSource, OBSSource(filter)));
}

void PLSVbChromakey::OnSourceFilterRemoved(void *param, calldata_t *data)
{
	PLSPropertiesView *window = reinterpret_cast<PLSPropertiesView *>(param);
	obs_source_t *filter = (obs_source_t *)calldata_ptr(data, "filter");
	QMetaObject::invokeMethod(window, "OnFilterAddOrRemove", Q_ARG(OBSSource, OBSSource(filter)));
}

PLSVbChromakey::PLSVbChromakey(PLSPropertiesView &view, OBSSource src, int index, QWidget *parent)
	: QWidget(parent),
	  propertiesView(view),
	  source(src),
	  lensIndex(index),
	  isVbRemoved(pls_is_lens_vb_removed(index)),
	  addSignal(obs_source_get_signal_handler(src), "filter_add", PLSVbChromakey::OnSourceFilterAdded, this),
	  removeSignal(obs_source_get_signal_handler(src), "filter_remove", PLSVbChromakey::OnSourceFilterRemoved, this)
{
	RegisterEvents();
	CheckChromakey(source);

	vbButton = new PLSLoadingVbButton(tr(VB_TO_REMOVE), this);
	vbButton->setObjectName("openPrismLens");
	UpdateVbButtonState();
	auto main = PLSBasic::instance();
	if (main) {
		vbButton->setLoading(main->IsOpeningLens());
		connect(main, &PLSBasic::onLensOpening, this, [this](bool opening) { vbButton->setLoading(opening); });
	}

	chromakeyButton = new QPushButton(this);
	UpdateChromakeyButtonText();

	auto *hLayout = new QHBoxLayout;
	hLayout->setContentsMargins(0, 0, 0, 0);
	hLayout->setSpacing(10);
	hLayout->addWidget(chromakeyButton);
	hLayout->addWidget(vbButton);

	auto *vLayout = new QVBoxLayout(this);
	vLayout->setContentsMargins(0, 0, 0, 0);
	vLayout->setSpacing(0);
	vLayout->addLayout(hLayout);
	vLayout->addSpacing(20);
	AppendDesc(vLayout);

	connect(vbButton, &QPushButton::clicked, this, &PLSVbChromakey::OnVbButtonClicked);
	connect(chromakeyButton, &QPushButton::clicked, this, &PLSVbChromakey::OnChromakeyButtonClicked);
}

PLSVbChromakey::~PLSVbChromakey()
{
	pls_unregister_lens_events(this);
}

void PLSVbChromakey::OnFilterAddOrRemove(OBSSource filter)
{
	if (!filter || !source) {
		assert(false);
		return;
	}

	auto id = obs_source_get_id(filter);
	if (!id) {
		assert(false);
		return;
	}

	if (!pls_is_equal(id, CHROMA_KEY_FILTER_ID))
		return;

	if (CheckChromakey(source))
		UpdateChromakeyButtonText();
}

#define INVOKE_UPDATE_VB_BUTTON_STATE                  \
	QMetaObject::invokeMethod(                     \
		ptr,                                   \
		[this, ptr]() {                        \
			if (ptr)                       \
				UpdateVbButtonState(); \
		},                                     \
		Qt::QueuedConnection)

void PLSVbChromakey::RegisterEvents()
{
	QPointer<QWidget> ptr(this);

	auto lens_running_cb = [this, ptr](bool lensRunning) {
		if (!ptr)
			return;
		INVOKE_UPDATE_VB_BUTTON_STATE;
	};

	auto license_verified_cb = [this, ptr]() {
		if (!ptr)
			return;
		INVOKE_UPDATE_VB_BUTTON_STATE;
	};

	auto capture_cb = [this, ptr](int lens, bool capture_ready) {
		if (!ptr || lens != lensIndex)
			return;
		INVOKE_UPDATE_VB_BUTTON_STATE;
	};

	auto vb_state_vb = [this, ptr](int lens, bool vb_removed) {
		if (!ptr || lens != lensIndex)
			return;

		QMetaObject::invokeMethod(
			ptr,
			[this, ptr, vb_removed]() {
				if (ptr)
					OnLensVbChanged(vb_removed);
			},
			Qt::QueuedConnection);
	};

	LensEvents evts;
	evts.lens_running_cb = lens_running_cb;
	evts.license_verified_cb = license_verified_cb;
	evts.vb_state_vb = vb_state_vb;
	evts.capture_cb = capture_cb;

	pls_register_lens_events(this, evts);
}

void PLSVbChromakey::OnLensVbChanged(bool vbRemoved)
{
	if (isVbRemoved != vbRemoved) {
		isVbRemoved = vbRemoved;
		UpdateVbButtonState();
	}
}

void PLSVbChromakey::UpdateVbButtonState()
{
	QString program;
	if (!pls_is_install_cam_studio(program) || !OBSPropertiesView::IsStateSupportedInLensApp() || !pls_is_lens_running() || !pls_is_lens_license_verified() ||
	    !pls_is_lens_capture_ready(lensIndex)) {
		return;
	}

	QString text = isVbRemoved ? tr(VB_TO_ORIGIN) : tr(VB_TO_REMOVE);
	if (vbButton->text() != text)
		vbButton->setButtonText(text);
}

void PLSVbChromakey::OnVbButtonClicked()
{
	PLSErrorHandler::ExtraData extraData(QString::fromUtf8(__FUNCTION__));

	const char *id = obs_source_get_id(source);
	bool isMobile = pls_is_equal(id, PRISM_LENS_MOBILE_SOURCE_ID);

	QString program;
	bool installed = pls_is_install_cam_studio(program);
	if (!installed) { // lens app is not installed
		pls_show_cam_studio_uninstall(pls_get_toplevel_view(this), QTStr("Alert.Title"), PTS_OBJECT(VB_LENS_TIP_INSTALL), QTStr(VB_LENS_TIP_INSTALL_NOW), QTStr("main.property.lens.later"));
		return;
	}

	if (!OBSPropertiesView::IsStateSupportedInLensApp()) {
		PLSErrorHandler::showAlertByPrismCode(PLSErrorHandler::ALERT_LENS_VB_NEED_UPGRADE, PLSErrKeyAllAlert, {}, extraData, pls_get_toplevel_view(&propertiesView));
		return;
	}

	if (!pls_is_lens_running()) {
		propertiesView.OpenLensApp(isMobile, false);
		SwitchVbState();
		return;
	}

	if (!pls_is_lens_ui_controllable()) {
		PLSErrorHandler::showAlertByPrismCode(PLSErrorHandler::ALERT_LENS_VB_ACTIVE_DEVICE, PLSErrKeyAllAlert, {}, extraData, pls_get_toplevel_view(&propertiesView));
		return;
	}

	if (!pls_is_lens_capture_ready(lensIndex)) {
		PLSErrorHandler::showAlertByPrismCode(PLSErrorHandler::ALERT_LENS_VB_NO_CAPTURE, PLSErrKeyAllAlert, {}, extraData, pls_get_toplevel_view(&propertiesView));
		return;
	}

	if (!pls_is_lens_license_verified() || !pls_is_lens_on(lensIndex)) {
		PLSErrorHandler::showAlertByPrismCode(PLSErrorHandler::ALERT_LENS_VB_ACTIVE_DEVICE, PLSErrKeyAllAlert, {}, extraData, pls_get_toplevel_view(&propertiesView));
		return;
	}

	SwitchVbState();
}

void PLSVbChromakey::SwitchVbState()
{
	if (isVbRemoved) { // user click button to cancel vb-removed
		auto ret = PLSErrorHandler::showAlertByPrismCode(PLSErrorHandler::ALERT_LENS_VB_TO_ORIGIN_CONFIRM, PLSErrKeyAllAlert, {}, PLSErrorHandler::ExtraData(QString::fromUtf8(__FUNCTION__)),
								 pls_get_toplevel_view(&propertiesView));
		if (ret.clickedBtn != PLSAlertView::Button::Ok)
			return;

		PLS_UI_ACTION("request vb original");
		pls_request_lens_remove_vb(lensIndex, false);

	} else {
		PLS_UI_ACTION("request vb removed");
		pls_request_lens_remove_vb(lensIndex, true);
	}
}

void PLSVbChromakey::OnChromakeyButtonClicked()
{
	if (!source)
		return;

	if (isChromakeyAdded) {
		PLS_UI_ACTION("request clear chromakey");

		auto ret = PLSErrorHandler::showAlertByPrismCode(PLSErrorHandler::ALERT_LENS_CHROMAKEY_CONFIRM, PLSErrKeyAllAlert, {}, PLSErrorHandler::ExtraData(QString::fromUtf8(__FUNCTION__)),
								 pls_get_toplevel_view(&propertiesView));
		if (ret.clickedBtn != PLSAlertView::Button::Ok)
			return;

		PLS_UI_ACTION("confirm clear chromakey");
		ClearChromakey();
		PLS_UI_ACTION("complete clear chromakey");

	} else {
		PLS_UI_ACTION("request add chromakey");
		obs_frontend_open_source_filters(source);
		PLSBasic::instance()->AutoAddFilter(source, CHROMA_KEY_FILTER_ID);
	}
}

bool PLSVbChromakey::CheckChromakey(OBSSource source)
{
	if (!source)
		return false;

	int count = 0;
	obs_source_enum_filters(
		source,
		[](obs_source_t *, obs_source_t *filter, void *p) {
			auto id = obs_source_get_id(filter);
			if (p && id && pls_is_equal(id, CHROMA_KEY_FILTER_ID)) {
				int *count = reinterpret_cast<int *>(p);
				*count += 1;
			}
		},
		&count);

	bool oldValue = isChromakeyAdded;
	isChromakeyAdded = (count > 0);

	if (oldValue != isChromakeyAdded)
		return true;
	else
		return false;
}

void PLSVbChromakey::ClearChromakey()
{
	if (!source)
		return;

	std::vector<obs_source *> filters;
	obs_source_enum_filters(
		source,
		[](obs_source_t *, obs_source_t *filter, void *p) {
			auto id = obs_source_get_id(filter);
			if (p && id && pls_is_equal(id, CHROMA_KEY_FILTER_ID)) {
				std::vector<obs_source *> *filters = (std::vector<obs_source *> *)p;
				filters->push_back(filter);
			}
		},
		&filters);

	for (const auto &item : filters) {
		if (item)
			obs_source_filter_remove(this->source, item);
	}
}

void PLSVbChromakey::UpdateChromakeyButtonText()
{
	auto text = isChromakeyAdded ? tr(CHROMAKEY_TO_CLEAR) : tr(CHROMAKEY_TO_ADD);
	chromakeyButton->setText(text);
}

void PLSVbChromakey::AppendDesc(QVBoxLayout *vLayout)
{
	QLabel *label = CreateDescLabel(this, nullptr, tr(VB_CHROMAKEY_DESC1));
	QWidget *iconDesc1 = CreateDescLine(36, tr(VB_CHROMAKEY_DESC2));
	QWidget *iconDesc2 = CreateDescLine(18, tr(VB_CHROMAKEY_DESC3));

	vLayout->addWidget(label);
	vLayout->addSpacing(10);
	vLayout->addWidget(iconDesc1);
	vLayout->addSpacing(10);
	vLayout->addWidget(iconDesc2);
}

QLabel *PLSVbChromakey::CreateDescLabel(QWidget *parent, QWidget *container, const QString &text)
{
	auto *descLabel = new DescLabel(text, container, parent);
	descLabel->setAlignment(Qt::AlignLeft);
	descLabel->setWordWrap(true);
	descLabel->setObjectName("prismLensVbGuide");
	descLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

	return descLabel;
}

QWidget *PLSVbChromakey::CreateDescLine(int fixedHeight, const QString &text)
{
	QWidget *container = new QWidget(this);
	container->setSizePolicy(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Fixed);

	auto *icon = new QLabel(container);
	icon->setObjectName("lensDescIcon");

	auto desc = CreateDescLabel(container, container, text);

	auto *vLayout = new QVBoxLayout;
	vLayout->setContentsMargins(4, 5, 4, 0);
	vLayout->setSpacing(0);
	vLayout->addWidget(icon);
	vLayout->addStretch();

	auto *hLayout = new QHBoxLayout(container);
	hLayout->setContentsMargins(0, 0, 0, 0);
	hLayout->setSpacing(0);
	hLayout->addLayout(vLayout);
	hLayout->addWidget(desc);

	return container;
}

//------------------------------------------------------------------------------------------------------------------------------
int PLSPropertiesView::GetCurrentLensIndex()
{
	if (!isPrismLensOrMobileSource()) {
		assert(false);
		return -1;
	}

	OBSSource source = GetPropertySource();
	if (!source) {
		assert(false);
		return -1;
	}

	OBSDataAutoRelease settings = obs_source_get_settings(source);
	if (!settings) {
		assert(false);
		return -1;
	}

#if defined(Q_OS_WIN)
	auto index = obs_data_get_int(settings, LENSV2_VIDEO_INDEX);
	return index;
#elif defined(Q_OS_MACOS)
	auto deviceId = obs_data_get_string(settings, CSTR_VIDEO_DEVICE_ID);
	return GetLensIndexFromDeviceString(deviceId);
#endif

	assert(false);
	return -1;
}

void PLSPropertiesView::AddVbChromakey(QWidget *parent, QFormLayout *formLayout)
{
	if (!parent || !formLayout || !isPrismLensOrMobileSource()) {
		assert(false);
		return;
	}

	OBSSource source = GetPropertySource();
	if (!source) {
		assert(false);
		return;
	}

	int index = GetCurrentLensIndex();
	if (index < 0 || index >= MAX_LENS_COUNT) {
		assert(false);
		return;
	}

	PLSVbChromakey *ui = new PLSVbChromakey(*this, source, index, parent);
	if (!ui) {
		assert(false);
		return;
	}

	QLabel *label = new QLabel(tr(VB_CHROMAKEY_LABEL));
	label->setObjectName(OBJECT_NAME_FORMLABEL);
	label->setProperty("lensSource", true);

	formLayout->addRow(label, ui);
}
