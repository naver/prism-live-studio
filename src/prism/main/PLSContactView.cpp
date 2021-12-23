#include <Windows.h>
#include "PLSContactView.hpp"
#include "ui_PLSContactView.h"
#include <QTextFrame>
#include <QFileDialog>
#include <QStandardPaths>
#include <QFontMetrics>
#include <QTimer>

#include "pls-common-define.hpp"
#include "frontend-api.h"
#include "log/log.h"
#include "PLSFileItemView.hpp"
#include "pls-app.hpp"
#include "alert-view.hpp"
#include "window-basic-main.hpp"
#include "PLSSceneDataMgr.h"
#include "ui-config.h"
#include <fstream>
#include "platform.hpp"

#define TEXT_EDIT_TOP_MARGIN 13
#define TEXT_EDIT_LEFT_MARGIN 15
#define TEXT_EDIT_RIGHT_MARGIN 10

#define TAG_LEFT_MARGIN 7
#define TAG_RIGHT_MARGIN 20
#define TAG_LIST_WIDGET_WIDTH 376
#define TAG_TOP_MARGIN 10
#define TAG_HEIGHT 24
#define TAG_EXTRA_MARGIN 38

#define ERROR_SINGLE_FILE_SIZE_LEVEL 2
#define ERROR_FILE_FORMAT_ERROR_LEVEL 1
#define NO_ERROR_LEVEL 0

static const int maxFileNumber = 4;
static const int textEditLengthLimit = 5000;
static const qint64 maxTotalFileSize = 20 * 1024 * 1024;
static const qint64 maxSingleFileSize = 10 * 1024 * 1024;

#define CONFIG_BASIC_WINDOW_MODULE "BasicWindow"
#define CONFIG_CONTACT_EMAIL_MODULE "ContactEmail"

PLSContactView::PLSContactView(QWidget *parent, PLSDpiHelper dpiHelper) : PLSDialogView(parent, dpiHelper), ui(new Ui::PLSContactView)
{
	dpiHelper.setCss(this, {PLSCssIndex::PLSLoadingBtn, PLSCssIndex::PLSContactView});
	dpiHelper.setFixedSize(this, {480, 489});
	chooseFileDir = QStandardPaths::standardLocations(QStandardPaths::DesktopLocation).first();
	setResizeEnabled(false);
	ui->setupUi(this->content());
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
	ui->identificationLabel->setText(QTStr("contact.email.report.ID.prefix").append(usercode));
	initConnect();
	dpiHelper.notifyDpiChanged(this, [this](double dpi, double, bool firstShow) {
		if (!firstShow) {
			updateItems(dpi);
		} else {
			QMetaObject::invokeMethod(
				this, [this]() { pls_flush_style(ui->emailLineEdit); }, Qt::QueuedConnection);
		}
	});
}

PLSContactView::~PLSContactView()
{
	delete ui;
}

void PLSContactView::updateItems(double dpi)
{

	//hidden listWidget view when item count is empty
	if (m_fileLists.size() == 0) {
		ui->tagTextEdit->setHidden(false);
		ui->tagListWidget->setHidden(true);
	} else {
		ui->tagTextEdit->setHidden(true);
		ui->tagListWidget->setHidden(false);
	}

	//refresh listwidget item
	ui->tagListWidget->clear();
	ui->tagListWidget->setSpacing(PLSDpiHelper::calculate(dpi, 7));
	for (int i = 0; i < m_fileLists.size(); i++) {
		PLSFileItemView *itemView = new PLSFileItemView(i, ui->tagListWidget);
		itemView->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
		QObject::connect(itemView, &PLSFileItemView::deleteItem, this, &PLSContactView::deleteItem);
		QFileInfo fileInfo = m_fileLists.at(i);
		QString fileName = fileInfo.fileName();
		QFontMetrics fontMetric(itemView->fileNameLabelFont());
		int maxWidth = (TAG_LIST_WIDGET_WIDTH - TAG_LEFT_MARGIN - TAG_RIGHT_MARGIN - TAG_EXTRA_MARGIN) * dpi;
		int fileNameWidth = fontMetric.boundingRect(fileName).width();
		if (fileNameWidth > maxWidth) {
			fileNameWidth = maxWidth;
		}
		QString str = fontMetric.elidedText(fileName, Qt::ElideRight, maxWidth);
		itemView->setFileName(str);
		int sizeHintWidth = (fileNameWidth + TAG_EXTRA_MARGIN * dpi);
		QListWidgetItem *listItem = new QListWidgetItem;
		listItem->setSizeHint(QSize(sizeHintWidth, TAG_HEIGHT * dpi));
		ui->tagListWidget->addItem(listItem);
		ui->tagListWidget->setItemWidget(listItem, itemView);
	}
}

void PLSContactView::deleteItem(int index)
{
	PLS_UI_STEP(CONTACT_US_MODULE, " PLSContactView deleteItem Button", ACTION_CLICK);
	m_fileLists.removeAt(index);
	ui->inquryTipLabel->setText("");
	updateItems(PLSDpiHelper::getDpi(this));
}

void PLSContactView::setupTextEdit()
{
	QTextDocument *document = ui->textEdit->document();
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
	bool isValue = config_has_user_value(App()->GlobalConfig(), CONFIG_BASIC_WINDOW_MODULE, CONFIG_CONTACT_EMAIL_MODULE);
	if (isValue) {
		const char *value = config_get_string(App()->GlobalConfig(), CONFIG_BASIC_WINDOW_MODULE, CONFIG_CONTACT_EMAIL_MODULE);
		if (value && value[0]) {
			ui->emailLineEdit->setText(QString(value));
			ui->sendButton->setEnabled(checkMailValid());
		}
	}
}

void PLSContactView::initConnect()
{
	QObject::connect(ui->emailLineEdit, &QLineEdit::editingFinished, this, &PLSContactView::on_emailLineEdit_editingFinished);
	QObject::connect(ui->emailLineEdit, &QLineEdit::textChanged, this, &PLSContactView::on_emailLineEdit_textChanged);
	QObject::connect(ui->textEdit, &QTextEdit::textChanged, this, &PLSContactView::on_textEdit_textChanged, Qt::QueuedConnection);
	QObject::connect(ui->fileButton, &QPushButton::clicked, this, &PLSContactView::on_fileButton_clicked);
	QObject::connect(ui->sendButton, &QPushButton::clicked, this, &PLSContactView::on_sendButton_clicked);
	QObject::connect(ui->cancelButton, &QPushButton::clicked, this, &PLSContactView::on_cancelButton_clicked);
}

bool PLSContactView::checkAddFileValid(const QFileInfo &fileInfo, int &errorLevel)
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

bool PLSContactView::checkTotalFileSizeValid(const QStringList &newFileList)
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

bool PLSContactView::checkSingleFileSizeValid(const QFileInfo &fileInfo)
{
	return fileInfo.size() < maxSingleFileSize;
}

bool PLSContactView::checkMailValid()
{
	QRegExp rep(EMAIL_REGEXP);
	bool result = rep.exactMatch(ui->emailLineEdit->text());
	return result;
}

bool PLSContactView::checkFileFormatValid(const QFileInfo &fileInfo)
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
	return valid;
}

void PLSContactView::showLoading(QWidget *parent)
{
	hideLoading();

	m_pWidgetLoadingBGParent = parent;

	m_pWidgetLoadingBG = new QWidget(parent);
	m_pWidgetLoadingBG->setObjectName("loadingBG");
	m_pWidgetLoadingBG->setGeometry(parent->geometry());
	m_pWidgetLoadingBG->show();

	auto layout = new QHBoxLayout(m_pWidgetLoadingBG);
	auto loadingBtn = new QPushButton(m_pWidgetLoadingBG);
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

		delete m_pWidgetLoadingBG;
		m_pWidgetLoadingBG = nullptr;
	}
}

static bool EnumSourceItem(obs_scene_t *, obs_sceneitem_t *item, void *ptr)
{
	std::vector<obs_source_t *> &items = *reinterpret_cast<std::vector<obs_source_t *> *>(ptr);
	if (obs_sceneitem_is_group(item)) {
		obs_sceneitem_group_enum_items(item, EnumSourceItem, ptr);
	} else {
		obs_source_t *source = obs_sceneitem_get_source(item);
		if (source) {
			items.push_back(source);
		}
	}

	return true;
}

static bool isGroup = false;

static bool LogSceneItem(obs_scene_t *, obs_sceneitem_t *item, void *param)
{
	const char *space = "";
	if (isGroup) {
		space = "  ";
	}
	obs_source_t *source = obs_sceneitem_get_source(item);
	const char *name = obs_source_get_name(source);
	const char *id = obs_source_get_id(source);
	std::ofstream &file = *reinterpret_cast<std::ofstream *>(param);

	file << space << "    source: " << name << "(" << id << ")" << std::endl;
	const char *type = "None";
	obs_monitoring_type monitorType = obs_source_get_monitoring_type(source);
	if (monitorType != OBS_MONITORING_TYPE_NONE) {
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
	}
	return scaleTypeName;
}
QString PLSContactView::WriteUserDetailInfo()
{
	QDir temp = QDir::temp();
	QString filePath = temp.absoluteFilePath("userInfo.txt");
	std::ofstream file(filePath.toStdString().c_str());
	if (!file) {
		PLS_INFO(CONTACT_US_MODULE, "write userinfo to local file failed");
		return "";
	}

	// prism version
	file << "Base Info:" << std::endl;
	std::string logUserID = PLSLoginUserInfo::getInstance()->getUserCode().toUtf8();
	file << "  PRISMLiveStudio Version: " << PLS_VERSION << std::endl;
	file << "  UserId: " << logUserID.c_str() << std::endl;

#ifdef _WIN32
	file << "  Current Date/Time:" << CurrentDateTimeString().c_str() << std::endl;
	bool browserHWAccel = config_get_bool(App()->GlobalConfig(), "General", "BrowserHWAccel");
	std::string browserHWAccelStr = browserHWAccel ? "true" : "false";
	file << "  Browser Hardware Acceleration: " << browserHWAccelStr << std::endl;
#endif
	std::string portableModeStr = portable_mode ? "true" : "false";
	file << "  Portable mode: " << portableModeStr << std::endl;

	// session id
	std::string neloSession = prismSession;
	file << "  NeloSession = " << neloSession.c_str() << std::endl;

	// current scene
	obs_source_t *currentScene = obs_frontend_get_current_scene();
	const char *currentSceneName = obs_source_get_name(currentScene);
	obs_source_release(currentScene);

	if (!currentScene) {
		PLSBasic *basic = PLSBasic::Get();
		OBSScene scene = basic->GetCurrentScene();
		obs_source_t *source = obs_scene_get_source(scene);
		currentSceneName = obs_source_get_name(source);
	}

	file << "  CurrentSceneName = " << (currentSceneName ? currentSceneName : "") << std::endl;
	file << std::endl;

	// all scenes
	file << "Loaded Scenes:" << std::endl;
	auto cb = [](void *param, obs_source_t *src) {
		obs_scene_t *scene = obs_scene_from_source(src);
		if (scene) {
			std::ofstream &file = *reinterpret_cast<std::ofstream *>(param);
			const char *name = obs_source_get_name(src);
			file << "  scene "
			     << "'" << (name ? name : "") << "'"
			     << ":" << std::endl;
			obs_scene_enum_items(scene, LogSceneItem, (void *)param);
		}
		return true;
	};
	obs_enum_scenes(cb, &file);

	// default audio mixer
	auto EnumDefaultAudioSources = [](void *param, obs_source_t *source) {
		if (!source)
			return true;

		std::vector<obs_source_t *> &items = *reinterpret_cast<std::vector<obs_source_t *> *>(param);
		if (obs_source_get_flags(source) & DEFAULT_AUDIO_DEVICE_FLAG) {
			items.push_back(source);
		}
		return true;
	};

	std::vector<obs_source_t *> items;
	obs_enum_sources(EnumDefaultAudioSources, &items);
	file << std::endl;
	file << "Default Audio Mixer:" << std::endl;
	for (int i = 0; i < items.size(); i++) {
		obs_source_t *source = items[i];
		if (!source) {
			continue;
		}
		const char *name = obs_source_get_name(source);
		const char *type = "None";
		obs_monitoring_type monitorType = obs_source_get_monitoring_type(source);
		if (monitorType != OBS_MONITORING_TYPE_NONE) {
			type = (monitorType == OBS_MONITORING_TYPE_MONITOR_ONLY) ? "Monitor Only" : "Monitor and Output";
		}
		float volume = obs_source_get_volume(source);
		file << "  " << (name ? name : "") << ": Audio Monitoring: " << type << ", Volume: " << obs_mul_to_db(volume) << " db" << std::endl;
	}

	// audio monitor device
	const char *name = nullptr;
	const char *id = nullptr;
	obs_get_audio_monitoring_device(&name, &id);
	file << std::endl;
	file << "Audio monitoring device:" << std::endl;
	file << "  Name: " << (name ? name : "") << std::endl;
	file << "  Id: " << (id ? id : "") << std::endl;

	// video settings
	struct obs_video_info ovi;
	if (obs_get_video_info(&ovi)) {
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
	struct obs_audio_info oai;
	if (obs_get_audio_info(&oai)) {
		file << std::endl;
		file << "Audio Settings:" << std::endl;
		file << "  Samples per sec: " << oai.samples_per_sec << std::endl;
		file << "  Speakers: " << oai.speakers << std::endl;
	}

	// hardware info
	obs_hardware_info hardwareInfo{};
	obs_get_current_hardware_info(&hardwareInfo);
	file << std::endl;
	file << "CPU Info:" << std::endl;
	file << "  CPU Name = " << hardwareInfo.cpu_name << std::endl;
	file << "  CPU Speed = " << hardwareInfo.cpu_speed_mhz << "MHz" << std::endl;
	file << "  Physical Memory = " << hardwareInfo.free_physical_memory_mb << "MB" << std::endl;
	file << "  Logical Cores = " << hardwareInfo.logical_cores << std::endl;
	file << "  Physical Cores = " << hardwareInfo.physical_cores << std::endl;

	file << "  Total Physical Memory = " << hardwareInfo.total_physical_memory_mb << "MB" << std::endl;
	file << "  Windows Version = " << hardwareInfo.windows_version << std::endl;
	file << std::endl;

	// adapter info
	obs_enter_graphics();
	gs_adapters_info_t *adapterInfo = gs_adapter_get_info();
	if (adapterInfo) {
		size_t adapterNum = adapterInfo->adapters.num;
		file << "Available Video Adapters (Current Index " << adapterInfo->current_index << ") :" << std::endl;
		for (size_t i = 0; i < adapterNum; i++) {
			file << "  Adapters " << i << ":" << std::endl;
			struct adapter_info info = adapterInfo->adapters.array[i];
			struct gs_luid luid = info.luid;
			file << "    Name = " << info.name << std::endl;
			if (info.feature_level)
				file << "    Feature Level = " << info.feature_level << std::endl;
			if (info.driver_version)
				file << "    Driver Version = " << info.driver_version << std::endl;
			file << "    Dedicated VRAM = " << info.dedicated_vram << "MB" << std::endl;
			file << "    Device ID = " << info.device_id << std::endl;
			file << "    Index = " << info.index << std::endl;
			file << "    LUID.high = " << luid.high_part << std::endl;
			file << "    LUID.low = " << luid.low_part << std::endl;
			file << "    Revision = " << info.revision << std::endl;
			file << "    Shared VRAM = " << info.shared_vram << "MB" << std::endl;
			file << "    Sub System ID = " << info.sub_system_id << std::endl;
			file << "    Vendor ID = " << info.vendor_id << std::endl;
			size_t monitorNum = info.monitors.num;
			file << "    Monitor:" << std::endl;
			for (size_t i = 0; i < monitorNum; i++) {
				struct gs_monitor_info monitor = info.monitors.array[i];
				file << "      output " << i << ": pos={" << monitor.x << "," << monitor.y << "}, size={" << monitor.cx << "," << monitor.cy
				     << "}, rotation degrees =" << monitor.rotation_degrees << std::endl;
			}
		}
	}
	obs_leave_graphics();
	file.close();

	return filePath;
}

void PLSContactView::closeEvent(QCloseEvent *event)
{
	// ALT+F4
	if ((GetAsyncKeyState(VK_MENU) < 0) && (GetAsyncKeyState(VK_F4) < 0)) {
		event->ignore();
	} else {
		PLSContactView::closeEvent(event);
	}
}

bool PLSContactView::eventFilter(QObject *watcher, QEvent *event)
{
	if (m_pWidgetLoadingBG && (watcher == m_pWidgetLoadingBGParent) && (event->type() == QEvent::Resize)) {
		QResizeEvent *resizeEvent = static_cast<QResizeEvent *>(event);
		m_pWidgetLoadingBG->setGeometry(0, 0, resizeEvent->size().width(), resizeEvent->size().height());
	}

	return PLSDialogView::eventFilter(watcher, event);
}

void PLSContactView::on_fileButton_clicked()
{
	PLS_UI_STEP(CONTACT_US_MODULE, " PLSContactView fileButton Button", ACTION_CLICK);
	if (m_fileLists.size() >= maxFileNumber) {
		ui->inquryTipLabel->setText(QTStr("contact.file.max.count"));
		return;
	}
	QString filter("Image Files(*.bmp *.jpg *.gif *.png);;Video Files(*.avi *.mp4 *.mpv *.mov);;Text Files(*.txt);;Zip Files(*.zip)");
	QStringList paths = QFileDialog::getOpenFileNames(this, QString(), chooseFileDir, filter);
	if (paths.size() > 0) {
		if (m_fileLists.size() + paths.size() > maxFileNumber) {
			ui->inquryTipLabel->setText(QTStr("contact.file.max.count"));
			return;
		}
		if (!checkTotalFileSizeValid(paths)) {
			ui->inquryTipLabel->setText(QTStr("contact.report.file.max.20M"));
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
		updateItems(PLSDpiHelper::getDpi(this));
		ui->tagListWidget->scrollToBottom();
		if (errorLevel == ERROR_SINGLE_FILE_SIZE_LEVEL) {
			ui->inquryTipLabel->setText(QTStr("contact.report.file.max.10M"));
		} else if (errorLevel == ERROR_FILE_FORMAT_ERROR_LEVEL) {
			ui->inquryTipLabel->setText(QTStr("contact.file.format.error"));
		} else {
			ui->inquryTipLabel->clear();
		}
	}
}

void PLSContactView::on_sendButton_clicked()
{
	showLoading(content());
	PLS_UI_STEP(CONTACT_US_MODULE, " PLSContactView sendButton Button", ACTION_CLICK);
	//background question,dont't care the request result.
	QString textContent = ui->textEdit->toPlainText();

	// write user info to local file
	QString filePath = WriteUserDetailInfo();
	QFileInfo info(filePath);
	QList<QFileInfo> fileLists = m_fileLists;
	if (info.exists()) {
		fileLists.append(info);
	} else {
		PLS_INFO(CONTACT_US_MODULE, "file was not existed: %s", GetFileName(filePath.toStdString().c_str()).c_str());
	}
	pls_upload_file_result_t chooseFileResult = pls_upload_contactus_files(ui->emailLineEdit->text(), textContent, fileLists);
	hideLoading();
	QFile::remove(filePath);
	if (chooseFileResult != pls_upload_file_result_t::Ok && this->isVisible()) {
		if (chooseFileResult == pls_upload_file_result_t::NetworkError) {
			PLSAlertView::warning(PLSBasic::Get(), QTStr("Alert.Title"), QTStr("login.check.note.network"));
		} else if (chooseFileResult == pls_upload_file_result_t::EmailFormatError) {
			PLSAlertView::warning(PLSBasic::Get(), QTStr("Alert.Title"), QTStr("contact.email.format.error"));
		} else if (chooseFileResult == pls_upload_file_result_t::FileFormatError) {
			PLSAlertView::warning(PLSBasic::Get(), QTStr("Alert.Title"), QTStr("contact.file.format.error"));
		} else if (chooseFileResult == pls_upload_file_result_t::AttachUpToMaxFile) {
			PLSAlertView::warning(PLSBasic::Get(), QTStr("Alert.Title"), QTStr("contact.file.max.count"));
		} else {
			PLS_WARN(CONTACT_US_MODULE, "pls_upload_contactus_files other error");
		}
		return;
	}
	config_set_string(App()->GlobalConfig(), CONFIG_BASIC_WINDOW_MODULE, CONFIG_CONTACT_EMAIL_MODULE, ui->emailLineEdit->text().toUtf8().constData());
	this->accept();
}

void PLSContactView::on_cancelButton_clicked()
{
	PLS_UI_STEP(CONTACT_US_MODULE, " PLSContactView cancelButton Button", ACTION_CLICK);
	config_set_string(App()->GlobalConfig(), CONFIG_BASIC_WINDOW_MODULE, CONFIG_CONTACT_EMAIL_MODULE, ui->emailLineEdit->text().toUtf8().constData());
	this->reject();
}

void PLSContactView::on_textEdit_textChanged()
{
	QString textContent = ui->textEdit->toPlainText();
	int length = textContent.count();
	if (length > textEditLengthLimit) {
		QSignalBlocker signalBlocker(ui->textEdit);
		int position = ui->textEdit->textCursor().position();
		QTextCursor textCursor = ui->textEdit->textCursor();
		textContent.remove(position - (length - textEditLengthLimit), length - textEditLengthLimit);
		ui->textEdit->setText(textContent);
		textCursor.setPosition(position - (length - textEditLengthLimit));
		ui->textEdit->setTextCursor(textCursor);
	}
}

void PLSContactView::on_emailLineEdit_editingFinished()
{
	ui->emailTipLabel->setVisible(!checkMailValid());
}

void PLSContactView::on_emailLineEdit_textChanged(const QString &string)
{
	Q_UNUSED(string)
	ui->sendButton->setEnabled(checkMailValid());
	ui->emailTipLabel->setVisible(false);
}
