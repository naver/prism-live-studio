
#include <QObject>

#if defined(Q_OS_WINDOWS)
#include <Windows.h>
#endif

#include "PLSContactView.hpp"
#include "ui_PLSContactView.h"
#include <QTextFrame>
#include <QFileDialog>
#include <QStandardPaths>
#include <QFontMetrics>
#include <QTimer>

#include <qregularexpression.h>
#include "utils-api.h"

#include "pls-common-define.hpp"
#include "frontend-api.h"
#include "log/log.h"
#include "PLSFileItemView.hpp"
#include "window-basic-main.hpp"
#include "PLSSceneDataMgr.h"
#include "ui-config.h"
#include <qpaintdevice.h>
#include <fstream>
#include "platform.hpp"
#include "login-user-info.hpp"
#include "prism-version.h"
#include "PLSBasic.h"
#include "pls/pls-properties.h"
#include "flowlayout.h"

using namespace common;
static const int TEXT_EDIT_TOP_MARGIN = 13;
static const int TEXT_EDIT_LEFT_MARGIN = 15;
static const int TEXT_EDIT_RIGHT_MARGIN = 10;

static const int TAG_LEFT_MARGIN = 7;
static const int TAG_RIGHT_MARGIN = 20;
static const int TAG_LIST_WIDGET_WIDTH = 376;
static const int TAG_HEIGHT = 24;
static const int TAG_EXTRA_MARGIN = 38;

static const int ERROR_SINGLE_FILE_SIZE_LEVEL = 2;
static const int ERROR_FILE_FORMAT_ERROR_LEVEL = 1;
static const int NO_ERROR_LEVEL = 0;

static const int maxFileNumber = 4;
static const int textEditLengthLimit = 5000;
static const qint64 maxTotalFileSize = 20 * 1024 * 1024;
static const qint64 maxSingleFileSize = 10 * 1024 * 1024;

constexpr auto CONFIG_BASIC_WINDOW_MODULE = "BasicWindow";
constexpr auto CONFIG_CONTACT_EMAIL_MODULE = "ContactEmail";

static const int messageFmtPropertyKey = 11;
static const int contentFmtPropertyKey = 22;
static const int messageFmtPropertyValue = 111;
static const int contentFmtPropertyValue = 222;

PLSContactView::PLSContactView(const QString &message, const QString &code, const QString &additionalMessage, QWidget *parent) : PLSDialogView(parent), m_additionalMessage(additionalMessage)
{
	ui = pls_new<Ui::PLSContactView>();
	setupUi(ui);

	for (auto i = 0; i < ui->horizontalLayout_3->count(); ++i) {
		if (auto radioButton = qobject_cast<PLSRadioButton *>(ui->horizontalLayout_3->itemAt(i)->widget()); nullptr != radioButton) {
			pls_uistep_v2_set_name(radioButton, QStringLiteral("Inquiry Type"));
		}
	}

	if (auto index = ui->verticalLayout_3->indexOf(ui->horizontalLayout_3); -1 != index) {
		auto flowLayout = pls_new<FlowLayout>(nullptr, 0, 6, 9);
		flowLayout->setAlignment(Qt::AlignTop | Qt::AlignLeft);
		flowLayout->setContentsMargins(0, 0, 0, 0);
		flowLayout->setHorizontalSpacing(20);
		flowLayout->addWidget(ui->radioButtonTypeError);
		flowLayout->addWidget(ui->radioButtonTypeAdvice);
		flowLayout->addWidget(ui->radioButtonTypeConsult);
		flowLayout->addWidget(ui->radioButtonTypePlus);
		flowLayout->addWidget(ui->radioButtonTypeOther);

		ui->radioButtonTypeError->show();
		ui->radioButtonTypeAdvice->show();
		ui->radioButtonTypeConsult->show();
		ui->radioButtonTypePlus->show();
		ui->radioButtonTypeOther->show();

		delete ui->horizontalLayout_3;
		ui->verticalLayout_3->insertLayout(index, flowLayout);
	}

	m_pInquireType = pls_new<PLSRadioButtonGroup>(this);
	m_pInquireType->addButton(ui->radioButtonTypeError, static_cast<int>(PLS_CONTACTUS_QUESTION_TYPE::Error));
	m_pInquireType->addButton(ui->radioButtonTypeAdvice, static_cast<int>(PLS_CONTACTUS_QUESTION_TYPE::Advice));
	m_pInquireType->addButton(ui->radioButtonTypeConsult, static_cast<int>(PLS_CONTACTUS_QUESTION_TYPE::Consult));
	m_pInquireType->addButton(ui->radioButtonTypePlus, static_cast<int>(PLS_CONTACTUS_QUESTION_TYPE::Plus));
	m_pInquireType->addButton(ui->radioButtonTypeOther, static_cast<int>(PLS_CONTACTUS_QUESTION_TYPE::Other));

	chooseFileDir = QStandardPaths::standardLocations(QStandardPaths::DesktopLocation).first();
	setResizeEnabled(false);
	pls_add_css(this, {"PLSLoadingBtn", "PLSContactView"});
	setFixedWidth(480);
	ui->sendButton->setEnabled(false);
	ui->tagListWidget->setHidden(true);
	ui->fileButton->setFileButtonEnabled(true);
	ui->cancelButton->setAutoDefault(false);
	ui->cancelButton->setDefault(false);
	ui->sendButton->setAutoDefault(false);
	ui->sendButton->setDefault(false);
	setupLineEdit();
	auto userID = pls_get_prism_usercode();
	if (userID.isEmpty()) {
		ui->identificationLabel->setVisible(false);
		ui->copyUserID->setVisible(false);
	} else {
		ui->identificationLabel->setText(QTStr("Contact.Email.Report.ID.Prefix").append(" "));
		ui->copyUserID->setText(userID);
		ui->identificationLabel->setVisible(true);
		ui->copyUserID->setVisible(true);
	}
	initConnect();
	m_errorMessage = message;
	m_errorCode = code;
#if defined(Q_OS_MACOS)
	setWindowTitle(tr("Contact.Top.Window.Title"));
#endif
	QMargins margins = ui->horizontalLayout_12->contentsMargins();
	margins.setRight(10);
	ui->horizontalLayout_12->setContentsMargins(margins);
	m_verticalScrollBar = ui->textEdit->verticalScrollBar();
	m_verticalScrollBar->installEventFilter(this);
#if defined(Q_OS_WIN)
	ui->verticalLayout->removeWidget(ui->topFrame);
	setTitleWidget(ui->topFrame);
#endif

	pls_uistep_v2_set_title(this, QStringLiteral("Contact US"));
	pls_uistep_v2_set_name(ui->emailLineEdit, QStringLiteral("Title"));
	pls_uistep_v2_set_name(ui->textEdit, QStringLiteral("Content"));
	pls_uistep_v2_auto_bind(this);
}

PLSContactView::~PLSContactView()
{
	pls_delete(ui);
}

void PLSContactView::updateSendButtonState()
{
	if (!checkMailValid()) {
		ui->sendButton->setEnabled(false);
		return;
	}

	if (m_pInquireType->checkedId() == -1) {
		return;
	}

	QString inquiryText = ui->textEdit->toPlainText();
	if (!checkMessageValid(inquiryText)) {
		ui->sendButton->setEnabled(false);
		return;
	}

	ui->sendButton->setEnabled(true);
}

bool PLSContactView::checkMessageValid(const QString &message)
{
	if (message.toUcs4().size() < 20) {
		return false;
	}
	const QRegularExpression re(QStringLiteral("^[\\s\\p{P}\\p{S}"
						   "\\x{2600}-\\x{26FF}"
						   "\\x{2700}-\\x{27BF}"
						   "\\x{1F300}-\\x{1F5FF}"
						   "\\x{1F600}-\\x{1F64F}"
						   "\\x{1F680}-\\x{1F6FF}"
						   "\\x{1F900}-\\x{1F9FF}"
						   "\\x{1FA70}-\\x{1FAFF}"
						   "\\x{1F1E6}-\\x{1F1FF}"
						   "\\x{FE00}-\\x{FE0F}"
						   "\\x{200D}"
						   "]+$"));
	return !re.match(message).hasMatch();
}

void PLSContactView::updateItems(double dpi)
{

	//hidden listWidget view when item count is empty
	if (m_fileLists.isEmpty()) {
		ui->tagListWidget->setHidden(true);
	} else {
		ui->tagListWidget->setHidden(false);
	}

	//refresh listwidget item
	ui->tagListWidget->clear();
	ui->tagListWidget->setSpacing(7);
	for (int i = 0; i < m_fileLists.size(); i++) {
		auto itemView = pls_new<PLSFileItemView>(i, ui->tagListWidget);
		itemView->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
		QObject::connect(itemView, &PLSFileItemView::deleteItem, this, &PLSContactView::deleteItem);
		QFileInfo fileInfo = m_fileLists.at(i);
		QString fileName = fileInfo.fileName();
		QFontMetrics fontMetric(itemView->fileNameLabelFont());
		auto maxWidth = static_cast<int>((TAG_LIST_WIDGET_WIDTH - TAG_LEFT_MARGIN - TAG_RIGHT_MARGIN - TAG_EXTRA_MARGIN) * dpi);
		int fileNameWidth = fontMetric.boundingRect(fileName).width();
		if (fileNameWidth > maxWidth) {
			fileNameWidth = maxWidth;
		}
		QString str = fontMetric.elidedText(fileName, Qt::ElideRight, maxWidth);
		itemView->setFileName(str);
		int sizeHintWidth = (fileNameWidth + (int)(TAG_EXTRA_MARGIN * dpi));
		auto listItem = pls_new<QListWidgetItem>();
		QSize itemSize = QSize(sizeHintWidth, static_cast<int>(TAG_HEIGHT * dpi));
		itemView->setFixedSize(itemSize);
		listItem->setSizeHint(itemSize);
		ui->tagListWidget->addItem(listItem);
		ui->tagListWidget->setItemWidget(listItem, itemView);
	}

	//update send button state
	updateSendButtonState();
}

void PLSContactView::deleteItem(int index)
{
	PLS_UI_STEP(CONTACT_US_MODULE, " PLSContactView deleteItem Button", ACTION_CLICK);
	m_fileLists.removeAt(index);
	ui->inquryTipLabel->setText("");
	updateItems(1);
}

void PLSContactView::setupTextEdit() const
{
	auto document = ui->textEdit->document();
	QTextFrame *rootFrame = document->rootFrame();
	QTextFrameFormat format;
	format.setLeftMargin(TEXT_EDIT_LEFT_MARGIN);
	format.setTopMargin(TEXT_EDIT_TOP_MARGIN);
	format.setRightMargin(TEXT_EDIT_RIGHT_MARGIN);
	format.setBottomMargin(TEXT_EDIT_TOP_MARGIN);
	format.setBorderBrush(Qt::red);
	format.setBorder(3);
	rootFrame->setFrameFormat(format);
}

void PLSContactView::setupLineEdit()
{
	QSizePolicy sizePolicy = ui->emailTipLabel->sizePolicy();
	sizePolicy.setRetainSizeWhenHidden(true);
	ui->emailTipLabel->setSizePolicy(sizePolicy);
	ui->emailTipLabel->setVisible(false);
	bool isValue = config_has_user_value(App()->GetUserConfig(), CONFIG_BASIC_WINDOW_MODULE, CONFIG_CONTACT_EMAIL_MODULE);
	if (isValue) {
		const char *value = config_get_string(App()->GetUserConfig(), CONFIG_BASIC_WINDOW_MODULE, CONFIG_CONTACT_EMAIL_MODULE);
		if (value && value[0]) {
			ui->emailLineEdit->setText(QString(value));
		}
	}
}

void PLSContactView::initConnect() const
{
	QObject::connect(ui->emailLineEdit, &QLineEdit::editingFinished, this, &PLSContactView::on_emailLineEdit_editingFinished);
	QObject::connect(ui->emailLineEdit, &QLineEdit::textChanged, this, &PLSContactView::on_emailLineEdit_textChanged);
	QObject::connect(ui->textEdit, &QTextEdit::textChanged, this, &PLSContactView::on_textEdit_textChanged, Qt::QueuedConnection);
	connect(ui->fileButton, &PLSFileButton::fileSelected, this, &PLSContactView::on_fileButton_clicked);
	connect(m_pInquireType, &PLSRadioButtonGroup::idClicked, this, &PLSContactView::updateSendButtonState);
}

bool PLSContactView::checkAddFileValid(const QFileInfo &fileInfo, int &errorLevel) const
{
	bool valid = true;
	errorLevel = NO_ERROR_LEVEL;
	if (!checkSingleFileSizeValid(fileInfo)) {
		valid = false;
		errorLevel = ERROR_SINGLE_FILE_SIZE_LEVEL;
	} else if (!checkFileFormatValid(fileInfo)) {
		valid = false;
		errorLevel = ERROR_FILE_FORMAT_ERROR_LEVEL;
	}
	return valid;
}

bool PLSContactView::checkTotalFileSizeValid(const QStringList &newFileList) const
{
	qint64 totalSize = 0;
	for (QFileInfo info : m_fileLists) {
		totalSize += info.size();
	}
	for (const auto &path : newFileList) {
		QFileInfo fileInfo(path);
		totalSize += fileInfo.size();
	}
	return totalSize < maxTotalFileSize;
}

bool PLSContactView::checkSingleFileSizeValid(const QFileInfo &fileInfo) const
{
	return fileInfo.size() < maxSingleFileSize;
}

bool PLSContactView::checkMailValid() const
{
	QRegularExpression re(EMAIL_REGEXP);
	QRegularExpressionMatch match = re.match(ui->emailLineEdit->text());
	if (match.hasMatch()) {
		return true;
	}
	return false;
}

bool PLSContactView::checkFileFormatValid(const QFileInfo &fileInfo) const
{
	bool valid = true;
	if (fileInfo.fileName().isEmpty()) {
		valid = false;
	} else {
		QStringList allFilterExtensionList;
		allFilterExtensionList << "bmp"
				       << "jpg"
				       << "gif"
				       << "png"
				       << "avi"
				       << "mp4"
				       << "mpv"
				       << "mov"
				       << "txt"
				       << "zip";
		if (!allFilterExtensionList.contains(fileInfo.suffix().toLower())) {
			valid = false;
		}
	}
	PLS_INFO(CONTACT_US_MODULE, "file name is %s, file suffix is %s, file suffix lower is %s", fileInfo.fileName().toUtf8().constData(), fileInfo.suffix().toUtf8().constData(),
		 fileInfo.suffix().toLower().toUtf8().constData());
	return valid;
}

void PLSContactView::showLoading(QWidget *parent)
{
	hideLoading();

	m_pWidgetLoadingBGParent = parent;

	m_pWidgetLoadingBG = pls_new<QWidget>(parent);
	m_pWidgetLoadingBG->setObjectName("loadingBG");
	m_pWidgetLoadingBG->setGeometry(parent->geometry());
	m_pWidgetLoadingBG->show();
	auto layout = pls_new<QHBoxLayout>(m_pWidgetLoadingBG);
	auto loadingBtn = pls_new<QPushButton>(m_pWidgetLoadingBG);
	pls_uistep_v2_set_custom_show_hide_name(loadingBtn, "PLSContactView loadingBtn");
	layout->addWidget(loadingBtn);
	loadingBtn->setObjectName("loadingBtn");
	loadingBtn->show();
	m_loadingEvent.startLoadingTimer(loadingBtn);
	if (m_pWidgetLoadingBGParent) {
		m_pWidgetLoadingBGParent->installEventFilter(this);
	}
}

void PLSContactView::hideLoading()
{
	if (m_pWidgetLoadingBGParent) {
		m_pWidgetLoadingBGParent->removeEventFilter(this);
		m_pWidgetLoadingBGParent = nullptr;
	}

	if (nullptr != m_pWidgetLoadingBG) {
		m_loadingEvent.stopLoadingTimer();
		pls_delete(m_pWidgetLoadingBG, nullptr);
	}
}

static bool LogSceneItem(obs_scene_t *, obs_sceneitem_t *item, void *param)
{
	static bool isGroup = false;
	const char *space = "";
	if (isGroup) {
		space = "  ";
	}
	std::ofstream &file = *static_cast<std::ofstream *>(param);
	if (const obs_source_t *source = obs_sceneitem_get_source(item)) {
		const char *name = obs_source_get_name(source);
		const char *id = obs_source_get_id(source);
		file << space << "    source: " << name << "(" << id << ")" << std::endl;

		const char *type = "None";
		if (obs_monitoring_type monitorType = obs_source_get_monitoring_type(source); monitorType != OBS_MONITORING_TYPE_NONE) {
			float volume = obs_source_get_volume(source);
			type = (monitorType == OBS_MONITORING_TYPE_MONITOR_ONLY) ? "Monitor Only" : "Monitor and Output";
			file << space << "    Audio Monitoring: " << type << ", Volume: " << obs_mul_to_db(volume) << " db" << std::endl;
		}
	}

	if (obs_sceneitem_is_group(item)) {
		isGroup = true;
		obs_sceneitem_group_enum_items(item, LogSceneItem, &file);
		isGroup = false;
	}
	return true;
}

static QString ConvertScaleType(enum obs_scale_type scaleType)
{
	QString scaleTypeName = "";
	switch (scaleType) {
	case OBS_SCALE_DISABLE:
		scaleTypeName = "Disabled";
		break;
	case OBS_SCALE_POINT:
		scaleTypeName = "Point";
		break;
	case OBS_SCALE_BICUBIC:
		scaleTypeName = "Bicubic";
		break;
	case OBS_SCALE_BILINEAR:
		scaleTypeName = "Bilinear";
		break;
	case OBS_SCALE_LANCZOS:
		scaleTypeName = "Lanczos";
		break;
	case OBS_SCALE_AREA:
		scaleTypeName = "Area";
		break;
	default:
		break;
	}
	return scaleTypeName;
}
QString PLSContactView::WriteUserDetailInfo() const
{
	QDir temp = QDir::temp();
	QString filePath = temp.absoluteFilePath("userInfo.txt");
	std::ofstream file(filePath.toUtf8().constData(), std::ios::out | std::ios::trunc);
	if (!file) {
		PLS_ERROR(CONTACT_US_MODULE, "write userinfo open failed, errno=%d (%s), path=%s", errno, std::strerror(errno), filePath.toUtf8().constData());
		return "";
	}

	writePrismVersionInfo(file);
	writeCurrentSceneInfo(file);
	writeDefaultAudioMixerInfo(file);
	writeAudioMonitorDeviceInfo(file);
	writeHardwareInfo(file);
	writeErrorAlertInfo(file);

	file.close();

	return filePath;
}

void PLSContactView::writePrismVersionInfo(std::ofstream &file) const
{
	// prism version
	file << "Base Info:" << std::endl;
	std::string l_logUserID = PLSLoginUserInfo::getInstance()->getUserCode().toStdString();
	file << "  PRISMLiveStudio Version: " << PLS_VERSION << std::endl;

	pls_datetime_t time;
	pls_get_current_datetime(time);
	file << "  Current Date/Time: " << pls_datetime_to_string(time).toStdString() << std::endl;
	bool browserHWAccel = config_get_bool(App()->GetUserConfig(), "General", "BrowserHWAccel");
	std::string browserHWAccelStr = browserHWAccel ? "true" : "false";
	std::string portableModeStr = GlobalVars::portable_mode ? "true" : "false";
	file << "  Portable mode: " << portableModeStr << std::endl;

	// session id
	file << "  PrismSession: " << GlobalVars::prismSession << std::endl;
	file << "  PrismSubSession: " << GlobalVars::prismSubSession << std::endl;

	if (!m_userInfoExtraLines.isEmpty()) {
		for (const auto &line : m_userInfoExtraLines) {
			file << line.toStdString() << std::endl;
		}
	}
}

void PLSContactView::writeCurrentSceneInfo(std::ofstream &file) const
{
	// current scene
	const char *currentSceneName = "";
	if (obs_source_t *currentScene = obs_frontend_get_current_scene()) {
		currentSceneName = obs_source_get_name(currentScene);
		obs_source_release(currentScene);
	} else {
		PLSBasic *basic = PLSBasic::instance();
		OBSScene scene = basic->GetCurrentScene();
		if (scene) {
			const obs_source_t *source = obs_scene_get_source(scene);
			if (source)
				currentSceneName = obs_source_get_name(source);
		}
	}

	file << "  CurrentSceneName: " << (currentSceneName ? currentSceneName : "") << std::endl;
	file << std::endl;

	// all scenes
	file << "Loaded Scenes:" << std::endl;
	auto cb = [](void *param, obs_source_t *src) {
		if (obs_scene_t *scene = obs_scene_from_source(src); scene) {
			std::ofstream &file_ = *static_cast<std::ofstream *>(param);
			const char *name = obs_source_get_name(src);
			file_ << "  scene "
			      << "'" << (name ? name : "") << "'"
			      << ":" << std::endl;
			obs_scene_enum_items(scene, LogSceneItem, param);
		}
		return true;
	};
	obs_enum_scenes(cb, &file);
}

void PLSContactView::writeDefaultAudioMixerInfo(std::ofstream &file) const
{
	// default audio mixer
	auto EnumDefaultAudioSources = [](void *param, obs_source_t *source) {
		if (!source)
			return true;

		std::vector<obs_source_t *> &items = *static_cast<std::vector<obs_source_t *> *>(param);
		if (obs_source_get_flags(source) & DEFAULT_AUDIO_DEVICE_FLAG) {
			items.push_back(source);
		}
		return true;
	};

	std::vector<obs_source_t *> items;
	obs_enum_sources(EnumDefaultAudioSources, &items);
	file << std::endl;
	file << "Default Audio Mixer:" << std::endl;
	for (const auto &source : items) {
		if (!source) {
			continue;
		}
		const char *name = obs_source_get_name(source);
		const char *type = "None";
		if (obs_monitoring_type monitorType = obs_source_get_monitoring_type(source); monitorType != OBS_MONITORING_TYPE_NONE) {
			type = (monitorType == OBS_MONITORING_TYPE_MONITOR_ONLY) ? "Monitor Only" : "Monitor and Output";
		}
		float volume = obs_source_get_volume(source);
		file << "  " << (name ? name : "") << ": Audio Monitoring: " << type << ", Volume: " << obs_mul_to_db(volume) << " db" << std::endl;
	}
}

void PLSContactView::writeAudioMonitorDeviceInfo(std::ofstream &file) const
{
	// audio monitor device
	const char *name = nullptr;
	const char *id = nullptr;
	obs_get_audio_monitoring_device(&name, &id);
	file << std::endl;
	file << "Audio monitoring device:" << std::endl;
	file << "  Name: " << (name ? name : "") << std::endl;
	file << "  Id: " << (id ? id : "") << std::endl;

	// video settings
	if (struct obs_video_info ovi; obs_get_video_info(&ovi)) {
		bool yuv = format_is_yuv(ovi.output_format);
		const char *yuv_format = get_video_colorspace_name(ovi.colorspace);
		const char *yuv_range = get_video_range_name(ovi.output_format, ovi.range);

		file << std::endl;
		file << "Video Settings:" << std::endl;
		file << "  Base resolution: " << ovi.base_width << "x" << ovi.base_height << std::endl;
		file << "  Output resolution: " << ovi.output_width << "x" << ovi.output_height << std::endl;
		file << "  Downscale filter: " << ConvertScaleType(ovi.scale_type).toUtf8().constData() << std::endl;
		file << "  Fps: " << ovi.fps_num << "/" << ovi.fps_den << std::endl;
		file << "  Format: " << get_video_format_name(ovi.output_format) << std::endl;

		const char *yvuFormatStr = yuv ? yuv_format : "None";
		const char *yvuStr = yuv ? "/" : "";
		const char *yvuRangeStr = yuv ? yuv_range : "";
		file << "  YUV Mode: " << yvuFormatStr << yvuStr << yvuRangeStr << std::endl;
	}

	// audio settings
	if (struct obs_audio_info oai; obs_get_audio_info(&oai)) {
		file << std::endl;
		file << "Audio Settings:" << std::endl;
		file << "  Samples per sec: " << oai.samples_per_sec << std::endl;
		file << "  Speakers: " << oai.speakers << std::endl;
	}
}

void PLSContactView::writeHardwareInfo(std::ofstream &file) const
{
	//// hardware info
	//obs_hardware_info hardwareInfo{};
	//obs_get_current_hardware_info(&hardwareInfo);
	//file << std::endl;
	//file << "CPU Info:" << std::endl;
	//file << "  CPU Name = " << hardwareInfo.cpu_name << std::endl;
	//file << "  CPU Speed = " << hardwareInfo.cpu_speed_mhz << "MHz" << std::endl;
	//file << "  Physical Memory = " << hardwareInfo.free_physical_memory_mb << "MB" << std::endl;
	//file << "  Logical Cores = " << hardwareInfo.logical_cores << std::endl;
	//file << "  Physical Cores = " << hardwareInfo.physical_cores << std::endl;

	//file << "  Total Physical Memory = " << hardwareInfo.total_physical_memory_mb << "MB" << std::endl;
	//file << "  Windows Version = " << hardwareInfo.windows_version << std::endl;
	//file << std::endl;

	//// adapter info
	//obs_enter_graphics();
	//if (auto adapterInfo = gs_adapter_get_info(); adapterInfo) {
	//	size_t adapterNum = adapterInfo->adapters.num;
	//	file << "Available Video Adapters (Current Index " << adapterInfo->current_index << ") :" << std::endl;
	//	for (size_t i = 0; i < adapterNum; i++) {
	//		file << "  Adapters " << i << ":" << std::endl;
	//		struct adapter_info info = adapterInfo->adapters.array[i];
	//		struct gs_luid luid = info.luid;
	//		file << "    Name = " << info.name << std::endl;
	//		if (info.feature_level)
	//			file << "    Feature Level = " << info.feature_level << std::endl;
	//		if (info.driver_version)
	//			file << "    Driver Version = " << info.driver_version << std::endl;
	//		file << "    Dedicated VRAM = " << info.dedicated_vram << "MB" << std::endl;
	//		file << "    Device ID = " << info.device_id << std::endl;
	//		file << "    Index = " << info.index << std::endl;
	//		file << "    LUID.high = " << luid.high_part << std::endl;
	//		file << "    LUID.low = " << luid.low_part << std::endl;
	//		file << "    Revision = " << info.revision << std::endl;
	//		file << "    Shared VRAM = " << info.shared_vram << "MB" << std::endl;
	//		file << "    Sub System ID = " << info.sub_system_id << std::endl;
	//		file << "    Vendor ID = " << info.vendor_id << std::endl;
	//		size_t monitorNum = info.monitors.num;
	//		file << "    Monitor:" << std::endl;
	//		for (size_t i_ = 0; i_ < monitorNum; i_++) {
	//			struct gs_monitor_info monitor = info.monitors.array[i_];
	//			file << "      output " << i_ << ": pos={" << monitor.x << "," << monitor.y << "}, size={" << monitor.cx << "," << monitor.cy
	//			     << "}, rotation degrees =" << monitor.rotation_degrees << std::endl;
	//		}
	//	}
	//}
}

void PLSContactView::writeErrorAlertInfo(std::ofstream &file) const
{
	file << "Error Alert:" << std::endl;
	if (!m_errorMessage.isEmpty()) {
		file << "  Message: " << m_errorMessage.toUtf8().constData() << std::endl;
	}
	if (!m_errorCode.isEmpty()) {
		file << "  Error Code: " << m_errorCode.toUtf8().constData() << std::endl;
	}
}

void PLSContactView::closeEvent(QCloseEvent *event)
{
	// ALT+F4
#if defined(Q_OS_WINDOWS)
	if ((GetAsyncKeyState(VK_MENU) < 0) && (GetAsyncKeyState(VK_F4) < 0)) {
		event->ignore();
		return;
	}
#endif
	PLSDialogView::closeEvent(event);
}

bool PLSContactView::eventFilter(QObject *watcher, QEvent *event)
{
	if (m_pWidgetLoadingBG && (watcher == m_pWidgetLoadingBGParent) && (event->type() == QEvent::Resize)) {
		auto resizeEvent = static_cast<QResizeEvent *>(event);
		m_pWidgetLoadingBG->setGeometry(0, 0, resizeEvent->size().width(), resizeEvent->size().height());
	} else if (m_verticalScrollBar && watcher == m_verticalScrollBar) {
		if (event->type() == QEvent::Show) {
			QMargins margins = ui->horizontalLayout_12->contentsMargins();
			margins.setRight(0);
			ui->horizontalLayout_12->setContentsMargins(margins);
		} else if (event->type() == QEvent::Hide) {
			QMargins margins = ui->horizontalLayout_12->contentsMargins();
			margins.setRight(10);
			ui->horizontalLayout_12->setContentsMargins(margins);
		}
	}
	return PLSDialogView::eventFilter(watcher, event);
}

void PLSContactView::on_fileButton_clicked()
{
	PLS_UI_STEP(CONTACT_US_MODULE, " PLSContactView fileButton Button", ACTION_CLICK);
	if (m_fileLists.size() >= maxFileNumber) {
		ui->inquryTipLabel->setText(QTStr("Contact.File.Max.Count"));
		return;
	}
	pls::HotKeyLocker locker;
	QString filter("Image Files(*.bmp *.jpg *.gif *.png);;Video Files(*.avi *.mp4 *.mpv *.mov);;Text Files(*.txt);;Zip Files(*.zip)");
	QStringList paths = QFileDialog::getOpenFileNames(this, QString(), chooseFileDir, filter);
	if (!paths.isEmpty()) {
		if (m_fileLists.size() + paths.size() > maxFileNumber) {
			ui->inquryTipLabel->setText(QTStr("Contact.File.Max.Count"));
			return;
		}
		if (!checkTotalFileSizeValid(paths)) {
			ui->inquryTipLabel->setText(QTStr("Contact.Report.File.Max20M"));
			return;
		}

		int errorLevel = NO_ERROR_LEVEL;
		for (const QString &path : paths) {
			QFileInfo fileInfo(path);
			int fileErrorLevel = NO_ERROR_LEVEL;
			if (checkAddFileValid(fileInfo, fileErrorLevel)) {
				m_fileLists.append(fileInfo);
			}
			if (fileErrorLevel > errorLevel) {
				errorLevel = fileErrorLevel;
			}
			chooseFileDir = fileInfo.dir().absolutePath();
		}
		updateItems(1);
		ui->tagListWidget->scrollToBottom();
		if (errorLevel == ERROR_SINGLE_FILE_SIZE_LEVEL) {
			ui->inquryTipLabel->setText(QTStr("Contact.Report.File.Max10M"));
		} else if (errorLevel == ERROR_FILE_FORMAT_ERROR_LEVEL) {
			ui->inquryTipLabel->setText(QTStr("Contact.File.Format.Error"));
		} else {
			ui->inquryTipLabel->clear();
		}
	}
}

void PLSContactView::on_sendButton_clicked()
{
	showLoading(content());
	PLS_UI_STEP(CONTACT_US_MODULE, " PLSContactView sendButton Button", ACTION_CLICK);
	PLS_LOGEX(PLS_LOG_INFO, CONTACT_US_MODULE, {{"contactus-action", "send"}}, "Contact us user click action");

	//background question,dont't care the request result.
	QString textContent = ui->textEdit->toPlainText();

	// write user info to local file
	QString filePath = WriteUserDetailInfo();
	QFileInfo info(filePath);
	QList<QFileInfo> fileLists = m_fileLists;
	if (info.exists()) {
		fileLists.append(info);
	}

	pls_upload_file_result_t chooseFileResult =
		pls_upload_contactus_files(static_cast<PLS_CONTACTUS_QUESTION_TYPE>(m_pInquireType->checkedId()), ui->emailLineEdit->text(), textContent, fileLists);
	hideLoading();
	if (!filePath.isEmpty()) {
		QFile::remove(filePath);
	}
	if (chooseFileResult != pls_upload_file_result_t::Ok && this->isVisible()) {
		PLSErrorHandler::ExtraData extraData("PLSContactView");
		if (chooseFileResult == pls_upload_file_result_t::NetworkError) {
			PLSErrorHandler::showAlertByPrismCode(PLSErrorHandler::ALERT_CONTACT_CHECK_NETWORK_ERROR, PLSErrKeyAllAlert, {}, extraData);
		} else if (chooseFileResult == pls_upload_file_result_t::EmailFormatError) {
			PLSErrorHandler::showAlertByPrismCode(PLSErrorHandler::ALERT_CONTACT_EMAIL_FORMAT_ERROR, PLSErrKeyAllAlert, {}, extraData);
		} else if (chooseFileResult == pls_upload_file_result_t::FileFormatError) {
			PLSErrorHandler::showAlertByPrismCode(PLSErrorHandler::ALERT_CONTACT_FILE_FORMAT_ERROR, PLSErrKeyAllAlert, {}, extraData);
		} else if (chooseFileResult == pls_upload_file_result_t::AttachUpToMaxFile) {
			PLSErrorHandler::showAlertByPrismCode(PLSErrorHandler::ALERT_CONTACT_FILE_MAX_COUNT, PLSErrKeyAllAlert, {}, extraData);
		} else {
			PLS_WARN(CONTACT_US_MODULE, "pls_upload_contactus_files other error");
		}
		return;
	}
	config_set_string(App()->GetUserConfig(), CONFIG_BASIC_WINDOW_MODULE, CONFIG_CONTACT_EMAIL_MODULE, ui->emailLineEdit->text().toUtf8().constData());
	this->accept();
}

void PLSContactView::on_cancelButton_clicked()
{
	PLS_UI_STEP(CONTACT_US_MODULE, " PLSContactView cancelButton Button", ACTION_CLICK);
	PLS_LOGEX(PLS_LOG_INFO, CONTACT_US_MODULE, {{"contactus-action", "cancel"}}, "Contact us user click action");
	this->reject();
}

void PLSContactView::on_textEdit_textChanged()
{
	if (m_message.length() > 0) {

		QTextCursor cursor = ui->textEdit->textCursor();
		cursor.setPosition(0, QTextCursor::MoveAnchor);
		cursor.setPosition(m_originMessage.length(), QTextCursor::KeepAnchor);
		QTextCharFormat messageFormat = cursor.charFormat();
		int messageFmtValue = messageFormat.property(messageFmtPropertyKey).toInt();

		cursor.setPosition(m_originMessage.length(), QTextCursor::MoveAnchor);
		cursor.setPosition(m_originMessage.length() + 1, QTextCursor::KeepAnchor);
		QTextCharFormat contentFormat = cursor.charFormat();
		int contentFmtValue = contentFormat.property(contentFmtPropertyKey).toInt();

		if (messageFmtValue != messageFmtPropertyValue || contentFmtValue != contentFmtPropertyValue) {
			QSignalBlocker signalBlocker(ui->textEdit);
			ui->textEdit->document()->undo();
		}
	}

	QString textContent = ui->textEdit->toPlainText();
	int length = (int)textContent.count();
	if (length > textEditLengthLimit) {
		QSignalBlocker signalBlocker(ui->textEdit);
		QTextCursor textCursor = ui->textEdit->textCursor();
		textCursor.setPosition(textEditLengthLimit, QTextCursor::MoveAnchor);
		textCursor.setPosition(length, QTextCursor::KeepAnchor);
		textCursor.removeSelectedText();
		ui->textEdit->setTextCursor(textCursor);
	}

	updateSendButtonState();
}

void PLSContactView::on_emailLineEdit_editingFinished()
{
	ui->emailTipLabel->setVisible(!checkMailValid());
}

void PLSContactView::on_emailLineEdit_textChanged(const QString &string)
{
	Q_UNUSED(string)
	updateSendButtonState();
	ui->emailTipLabel->setVisible(false);
}
