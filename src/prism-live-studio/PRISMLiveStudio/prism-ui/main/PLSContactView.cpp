
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

PLSContactView::PLSContactView(const QString &message, const QString &additionalMessage, QWidget *parent) : PLSDialogView(parent), m_additionalMessage(additionalMessage)
{
	ui = pls_new<Ui::PLSContactView>();
	setupUi(ui);

	if (auto index = ui->verticalLayout_3->indexOf(ui->horizontalLayout_3); -1 != index) {
		auto flowLayout = pls_new<FlowLayout>(nullptr, 0, 6, 6);
		flowLayout->setAlignment(Qt::AlignTop | Qt::AlignLeft);
		flowLayout->setContentsMargins(0, 0, 0, 0);

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
	setMoveInContent(true);
	pls_add_css(this, {"PLSLoadingBtn", "PLSContactView"});
	setFixedSize(480, 576);
	ui->sendButton->setEnabled(false);
	ui->inquryTipLabel->setText("");
	ui->tagTextEdit->setHidden(false);
	ui->tagListWidget->setHidden(true);
	ui->fileButton->setFileButtonEnabled(true);
	ui->fileButton->setAutoDefault(false);
	ui->fileButton->setDefault(false);
	ui->cancelButton->setAutoDefault(false);
	ui->cancelButton->setDefault(false);
	ui->sendButton->setAutoDefault(false);
	ui->sendButton->setDefault(false);
	setupLineEdit();
	QString usercode = pls_get_prism_usercode().prepend(" ");
	ui->identificationLabel->setText(QTStr("Contact.Email.Report.ID.Prefix").append(usercode));
	initConnect();

	setupErrorMessageTextEdit(message);
#if defined(Q_OS_MACOS)
	setWindowTitle(tr("Contact.Top.Window.Title"));
	setProperty("type", "Mac");
	ui->sendButton->setProperty("type", "MacOS");
	ui->cancelButton->setProperty("type", "MacOS");
#elif defined(Q_OS_WIN)
	setProperty("type", "Win");
#endif
	QMargins margins = ui->horizontalLayout_12->contentsMargins();
	margins.setRight(10);
	ui->horizontalLayout_12->setContentsMargins(margins);
	m_verticalScrollBar = ui->textEdit->verticalScrollBar();
	m_verticalScrollBar->installEventFilter(this);
}

PLSContactView::~PLSContactView()
{
	pls_delete(ui);
}

void PLSContactView::setupErrorMessageTextEdit(const QString &message)
{
	if (message.isEmpty()) {
		return;
	}

	m_message = QString("%1\n").arg(message);
	m_originMessage = message;
	QSignalBlocker signalBlocker(ui->textEdit);
	ui->textEdit->setText(m_message);

	QTextCharFormat messageFmt;
	QColor textColor("#bababa");
	messageFmt.setForeground(textColor);
	messageFmt.setProperty(messageFmtPropertyKey, messageFmtPropertyValue);

	QTextCursor cursor = ui->textEdit->textCursor();
	ui->textEdit->setFocus();
	cursor.setPosition(0, QTextCursor::MoveAnchor);
	cursor.setPosition(message.length(), QTextCursor::KeepAnchor);
	cursor.mergeCharFormat(messageFmt);

	QTextCharFormat contentFmt;
	contentFmt.setForeground(Qt::white);
	contentFmt.setProperty(contentFmtPropertyKey, contentFmtPropertyValue);
	cursor.setPosition(message.length(), QTextCursor::MoveAnchor);
	cursor.setPosition(message.length() + 1, QTextCursor::KeepAnchor);
	cursor.mergeCharFormat(contentFmt);

	cursor.movePosition(QTextCursor::End);
	ui->textEdit->setTextCursor(cursor);

	//update send button state
	updateSendButtonState();
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
	if (m_fileLists.isEmpty() && inquiryText.isEmpty()) {
		ui->sendButton->setEnabled(false);
		return;
	}

	ui->sendButton->setEnabled(true);
}

void PLSContactView::updateItems(double dpi)
{

	//hidden listWidget view when item count is empty
	if (m_fileLists.isEmpty()) {
		ui->tagTextEdit->setHidden(false);
		ui->tagListWidget->setHidden(true);
	} else {
		ui->tagTextEdit->setHidden(true);
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
	const obs_source_t *source = obs_sceneitem_get_source(item);
	const char *name = obs_source_get_name(source);
	const char *id = obs_source_get_id(source);
	std::ofstream &file = *static_cast<std::ofstream *>(param);

	file << space << "    source: " << name << "(" << id << ")" << std::endl;
	const char *type = "None";
	if (obs_monitoring_type monitorType = obs_source_get_monitoring_type(source); monitorType != OBS_MONITORING_TYPE_NONE) {
		float volume = obs_source_get_volume(source);
		type = (monitorType == OBS_MONITORING_TYPE_MONITOR_ONLY) ? "Monitor Only" : "Monitor and Output";
		file << space << "    Audio Monitoring: " << type << ", Volume: " << obs_mul_to_db(volume) << " db" << std::endl;
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
	std::ofstream file(filePath.toStdString().c_str());
	if (!file) {
		PLS_INFO(CONTACT_US_MODULE, "write userinfo to local file failed");
		return "";
	}

	writePrismVersionInfo(file);
	writeCurrentSceneInfo(file);
	writeDefaultAudioMixerInfo(file);
	writeAudioMonitorDeviceInfo(file);
	writeHardwareInfo(file);
	file.close();

	return filePath;
}

void PLSContactView::writePrismVersionInfo(std::ofstream &file) const
{
	// prism version
	file << "Base Info:" << std::endl;
	std::string l_logUserID = PLSLoginUserInfo::getInstance()->getUserCode().toStdString();
	file << "  PRISMLiveStudio Version: " << PLS_VERSION << std::endl;
	file << "  UserId: " << l_logUserID.c_str() << std::endl;

#ifdef _WIN32
	file << "  Current Date/Time:" << CurrentDateTimeString().c_str() << std::endl;
	bool browserHWAccel = config_get_bool(App()->GetUserConfig(), "General", "BrowserHWAccel");
	std::string browserHWAccelStr = browserHWAccel ? "true" : "false";
	file << "  Browser Hardware Acceleration: " << browserHWAccelStr << std::endl;
#endif
	std::string portableModeStr = GlobalVars::portable_mode ? "true" : "false";
	file << "  Portable mode: " << portableModeStr << std::endl;

	// session id
	std::string neloSession = GlobalVars::prismSession;
	file << "  NeloSession = " << neloSession.c_str() << std::endl;
}

void PLSContactView::writeCurrentSceneInfo(std::ofstream &file) const
{
	// current scene
	obs_source_t *currentScene = obs_frontend_get_current_scene();
	const char *currentSceneName = obs_source_get_name(currentScene);
	obs_source_release(currentScene);

	if (!currentScene) {
		PLSBasic *basic = PLSBasic::instance();
		OBSScene scene = basic->GetCurrentScene();
		const obs_source_t *source = obs_scene_get_source(scene);
		currentSceneName = obs_source_get_name(source);
	}

	file << "  CurrentSceneName = " << (currentSceneName ? currentSceneName : "") << std::endl;
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
		file << "  Downscale filter: " << ConvertScaleType(ovi.scale_type).toStdString().c_str() << std::endl;
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
	auto tempPath = filePath.toStdString();
	QFileInfo info(filePath);
	QList<QFileInfo> fileLists = m_fileLists;
	if (info.exists()) {
		fileLists.append(info);
	}

	pls_upload_file_result_t chooseFileResult =
		pls_upload_contactus_files(static_cast<PLS_CONTACTUS_QUESTION_TYPE>(m_pInquireType->checkedId()), ui->emailLineEdit->text(), textContent, fileLists);
	hideLoading();
	QFile::remove(filePath);
	if (chooseFileResult != pls_upload_file_result_t::Ok && this->isVisible()) {
		if (chooseFileResult == pls_upload_file_result_t::NetworkError) {
			PLSAlertView::warning(pls_get_main_view(), QTStr("Alert.Title"), QTStr("Contact.Check.Network.Error"));
		} else if (chooseFileResult == pls_upload_file_result_t::EmailFormatError) {
			PLSAlertView::warning(pls_get_main_view(), QTStr("Alert.Title"), QTStr("Contact.Email.Format.Error"));
		} else if (chooseFileResult == pls_upload_file_result_t::FileFormatError) {
			PLSAlertView::warning(pls_get_main_view(), QTStr("Alert.Title"), QTStr("Contact.File.Format.Error"));
		} else if (chooseFileResult == pls_upload_file_result_t::AttachUpToMaxFile) {
			PLSAlertView::warning(pls_get_main_view(), QTStr("Alert.Title"), QTStr("Contact.File.Max.Count"));
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
	config_set_string(App()->GetUserConfig(), CONFIG_BASIC_WINDOW_MODULE, CONFIG_CONTACT_EMAIL_MODULE, ui->emailLineEdit->text().toUtf8().constData());
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
