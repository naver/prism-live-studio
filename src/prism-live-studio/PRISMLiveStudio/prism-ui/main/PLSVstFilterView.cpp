#include "PLSVstFilterView.h"
#include "ui_PLSVstFilterView.h"
#include "liblog.h"
#include "log/module_names.h"
#include "action.h"
#include <frontend-api.h>
#include <QDir>
#include <QMovie>
#include <QDirIterator>
#include <QStylePainter>
#include <QHBoxLayout>
#include <QCheckBox>
#include <qregularexpression.h>
#include <obs-app.hpp>
#include <vertical-scroll-area.hpp>
#include "PLSCommonScrollBar.h"
#include <pls/pls-source.h>
#include <pls/pls-obs-api.h>

constexpr auto g_vst_open_interface = "open_vst_settings";
constexpr auto g_vst_open_when_active = "open_when_active_vst_settings";
constexpr auto g_vst_path = "plugin_path";
constexpr auto g_tr_vst_select_plugin = "Basic.Filter.vstfilter.view.selectplugin";
constexpr auto g_vst_state_none = -1;

PLSVstFilterView::PLSVstFilterView(OBSData settings, OBSSource source, QWidget *parent) : QWidget(parent), filterSource(source), filterSettings(settings)
{
	ui = pls_new<Ui::PLSVstFilterView>();
	ui->setupUi(this);
	pls_set_css(this, {"PLSVstFilterView"});
	resetUi();

	qRegisterMetaType<uint64_t>("uint64_t");

	connect(ui->comboBox_vst_list, qOverload<int>(&QComboBox::currentIndexChanged), this, &PLSVstFilterView::selectVstPlugin);
	connect(ui->checkBox_open_when_activate, &PLSCheckBox::clicked, this, &PLSVstFilterView::onOpenWhenActiveClicked);
	connect(ui->btn_vst_interface, &QPushButton::clicked, this, &PLSVstFilterView::onOpenVstInterface);
	auto vertial_scroll_bar = dynamic_cast<PLSCommonScrollBar *>(ui->scrollArea->verticalScrollBar());
	connect(vertial_scroll_bar, &PLSCommonScrollBar::isShowScrollBar, this, &PLSVstFilterView::onShowScrollBar);
	signal_handler_connect_ref(obs_source_get_signal_handler(source), "vst_state_changed", onVstStateChanged, this);

	initProperties();
}

PLSVstFilterView::~PLSVstFilterView()
{
	loadingEvent.stopLoadingTimer();
	signal_handler_disconnect(obs_source_get_signal_handler(filterSource), "vst_state_changed", onVstStateChanged, this);
	pls_delete(ui);
}

void PLSVstFilterView::resetProperties()
{
	obs_source_update(filterSource, nullptr);
	initProperties();
	resetUi();
}

void PLSVstFilterView::onVstStateChanged(void *param, calldata_t *data)
{
	auto view = (PLSVstFilterView *)(param);
	if (!view)
		return;
	obs_source_t *source = nullptr;
	const char *vst_path = nullptr;
	int64_t state;
	calldata_get_ptr(data, "source", &source);
	calldata_get_string(data, "vst_path", &vst_path);
	calldata_get_int(data, "vst_state", &state);
	QMetaObject::invokeMethod(view, "handleVstStateChange", Qt::QueuedConnection, Q_ARG(const QString &, QString(vst_path)), Q_ARG(int, (int)state),
				  Q_ARG(uint64_t, reinterpret_cast<uint64_t>(source)));
}

void PLSVstFilterView::handleVstStateChange(const QString &vst_path, int vst_state, uint64_t source_ptr)
{
	if (source_ptr != (uint64_t)(filterSource.Get()))
		return;
	updateVstState(vst_path, vst_state);
}

void PLSVstFilterView::initProperties()
{
	auto settings = pls_get_source_setting(filterSource);
	bool openInterfaceWhenActive = obs_data_get_bool(settings, g_vst_open_when_active);
	const char *path = obs_data_get_string(settings, g_vst_path);

	ui->checkBox_open_when_activate->setChecked(openInterfaceWhenActive);

	QSignalBlocker blocker(ui->comboBox_vst_list);
	ui->comboBox_vst_list->clear();
	fill_out_plugins();
	auto index = ui->comboBox_vst_list->findData(QString(path));
	ui->comboBox_vst_list->setCurrentIndex((-1 != index) ? index : 0);

	if (path && 0 == strcmp(path, ""))
		return;

	obs_data_t *vst_data = obs_data_create();
	obs_data_set_string(vst_data, "method", "get_vst_state");
	pls_source_get_private_data(filterSource, vst_data);
	const char *vst_path = obs_data_get_string(vst_data, "vst_path");
	auto vst_state = (int)obs_data_get_int(vst_data, "vst_state");
	if (path && vst_path && 0 == strcmp(path, vst_path)) {
		updateVstState(path, vst_state);
	}
	obs_data_release(vst_data);
}

QWidget *getIndexWidget(const ComboBoxWithIcon *w, int index)
{
	if (!w)
		return nullptr;
	auto model = w->view()->model();
	auto item = w->view()->indexWidget(model->index(index, 0));
	return item;
}

void PLSVstFilterView::fill_out_plugins()
{
	QStringList dir_list;

#ifdef __APPLE__
	dir_list << "/Library/Audio/Plug-Ins/VST/"
		 << "~/Library/Audio/Plug-ins/VST/";
#elif WIN32
#ifndef _WIN64
	HANDLE hProcess = GetCurrentProcess();

	BOOL isWow64;
	IsWow64Process(hProcess, &isWow64);

	if (!isWow64) {
#endif
		dir_list << qEnvironmentVariable("ProgramFiles") + "/Steinberg/VstPlugins/" << qEnvironmentVariable("CommonProgramFiles") + "/Steinberg/Shared Components/"
			 << qEnvironmentVariable("CommonProgramFiles") + "/VST2" << qEnvironmentVariable("CommonProgramFiles") + "/Steinberg/VST2"
			 << qEnvironmentVariable("CommonProgramFiles") + "/VSTPlugins/" << qEnvironmentVariable("ProgramFiles") + "/VSTPlugins/";
#ifndef _WIN64
	} else {
		dir_list << qEnvironmentVariable("ProgramFiles(x86)") + "/Steinberg/VstPlugins/" << qEnvironmentVariable("CommonProgramFiles(x86)") + "/Steinberg/Shared Components/"
			 << qEnvironmentVariable("CommonProgramFiles(x86)") + "/VST2" << qEnvironmentVariable("CommonProgramFiles(x86)") + "/VSTPlugins/"
			 << qEnvironmentVariable("ProgramFiles(x86)") + "/VSTPlugins/";
	}
#endif
#elif __linux__
	// If the user has set the VST_PATH environmental
	// variable, then use it. Else default to a list
	// of common locations.
	char *vstPathEnv;
	vstPathEnv = getenv("VST_PATH");
	if (vstPathEnv != nullptr) {
		dir_list << vstPathEnv;
	} else {
		// Choose the most common locations
		dir_list << "/usr/lib/vst/"
			 << "/usr/lib/lxvst/"
			 << "/usr/lib/linux_vst/"
			 << "/usr/lib64/vst/"
			 << "/usr/lib64/lxvst/"
			 << "/usr/lib64/linux_vst/"
			 << "/usr/local/lib/vst/"
			 << "/usr/local/lib/lxvst/"
			 << "/usr/local/lib/linux_vst/"
			 << "/usr/local/lib64/vst/"
			 << "/usr/local/lib64/lxvst/"
			 << "/usr/local/lib64/linux_vst/"
			 << "~/.vst/"
			 << "~/.lxvst/";
	}
#endif

	QStringList filters;

#ifdef __APPLE__
	filters << "*.vst";
#elif WIN32
	filters << "*.dll";
#elif __linux__
	filters << "*.so"
		<< "*.o";
#endif

	QStringList vst_list;

	// Read all plugins into a list...
	for (int a = 0; a < dir_list.size(); ++a) {
		QDir search_dir(dir_list[a]);
		search_dir.setNameFilters(filters);
		QDirIterator it(search_dir, QDirIterator::Subdirectories);
		while (it.hasNext()) {
			QString path = it.next();
			QString name = it.fileName();

#ifdef __APPLE__
			name.remove(QRegularExpression("(\\.vst)"));
#elif WIN32
			name.remove(QRegularExpression("(\\.dll)"));
#elif __linux__
			name.remove(QRegularExpression("(\\.so|\\.o)"));
#endif

			name.append("=").append(path);
			vst_list << name;
		}
	}

	// Now sort list alphabetically (still case-sensitive though).
	std::stable_sort(vst_list.begin(), vst_list.end(), std::less<QString>());
	ui->comboBox_vst_list->addItem(QTStr(g_tr_vst_select_plugin), "");

	for (int b = 0; b < vst_list.size(); ++b) {
		QString vst_sorted = vst_list[b];
		ui->comboBox_vst_list->addItem(vst_sorted.left(vst_sorted.indexOf('=')), vst_sorted.mid(vst_sorted.indexOf('=') + 1));
		// initialize the icon state
		auto item = getIndexWidget(ui->comboBox_vst_list, b);
		if (item) {
			item->setProperty("vst_state", g_vst_state_none);
			pls_flush_style(item);
		}
	}
}

void PLSVstFilterView::updateVstState(const QString &vst_plugin_in, int state)
{
	auto fileName = pls_get_path_file_name(vst_plugin_in);
	PLS_INFO(MAINFILTER_MODULE, "update vst state, vst plugin: %s, state: %d", qUtf8Printable(fileName), state);

	auto current_vst_path = ui->comboBox_vst_list->currentData().toString();
	if (current_vst_path != vst_plugin_in)
		return;

	auto state_inner = (enum obs_vst_verify_state)state;
	if (VST_STATUS_CHECKING == state_inner) {
		ui->comboBox_vst_list->hideIcon();
		ui->btn_vst_interface->setEnabled(false);
		ui->frame->show();
		setLoadingVisible(true);
		updateTipsState(QTStr("Basic.Filter.vstfilter.state.checking"), false);
		return;
	}

	if (VST_STATUS_AVAILABLE == state_inner) {
		ui->comboBox_vst_list->hideIcon();
		ui->btn_vst_interface->setEnabled(true);
		setLoadingVisible(false);
		ui->frame->show();
		updateTipsState(QTStr("Basic.Filter.vstfilter.state.available"), false);
		return;
	}
	ui->comboBox_vst_list->showIcon();
	ui->btn_vst_interface->setEnabled(false);
	ui->frame->show();
	setLoadingVisible(false);
	QString tipsString = "";
	switch (state_inner) {
	case VST_STATUS_INVALID_ARCH:
		tipsString = QTStr("Basic.Filter.vstfilter.state.invalidarch");
		break;
	case VST_STATUS_EFFECT_UNSUPPORT:
		tipsString = QTStr("Basic.Filter.vstfilter.state.effectunsupport");
		break;
	case VST_STATUS_TIMEOUT:
		tipsString = QTStr("Basic.Filter.vstfilter.state.timeout");
		break;
	case VST_STATUS_CRASH:
	case VST_STATUS_PROCESS_DISAPPEAR:
		tipsString = QTStr("Basic.Filter.vstfilter.state.unstable");
		break;
	case VST_STATUS_NOT_VST:
		tipsString = QTStr("Basic.Filter.vstfilter.state.notvst");
		break;
	case VST_STATUS_UNKNOWN_ERROR:
	case VST_STATUS_PROCESS_UNKNOWN_ERROR:
	case VST_STATUS_PROCESS_FAILED_TO_START:
	case VST_STATUS_PROCESS_READ_ERROR:
	case VST_STATUS_PROCESS_WRITE_ERROR:
	case VST_STATUS_EFFECT_NULLPTR:
	case VST_STATUS_PROCESS_OPEN_DLL_ERROR:
	case VST_STATUS_PROCESS_GET_SCAN_FUNC_ERROR:
		tipsString = QTStr("Basic.Filter.vstfilter.state.unknownerror");
		break;
	case VST_STATUS_CHANNEL_UNSUPPORT:
		tipsString = QTStr("Basic.Filter.vstfilter.state.channelunsupport");
		break;
	case VST_STATUS_DLL_LOAD_FAIL:
		tipsString = QTStr("Basic.Filter.vstfilter.state.dllloadfaild");
		break;
	default:
		tipsString = QTStr("Basic.Filter.vstfilter.state.unknownerror");
		assert(false && "unknown vst state");
		break;
	}
	updateTipsState(tipsString, true);
}

void PLSVstFilterView::updateTipsState(const QString &tipsStr, bool warning)
{
	ui->label_vst_tips->setText(tipsStr);
	ui->label_vst_tips->setProperty("warning", warning);
	pls_flush_style(ui->label_vst_tips);
}

void PLSVstFilterView::resetUi()
{
	ui->comboBox_vst_list->hideIcon();
	ui->frame->hide();
	ui->btn_vst_interface->setEnabled(false);
}

void PLSVstFilterView::setLoadingVisible(bool visible)
{
	if (visible) {
		ui->label_loading->show();
		loadingEvent.startLoadingTimer(ui->label_loading);
	} else {
		ui->label_loading->hide();
		loadingEvent.stopLoadingTimer();
	}
}

void PLSVstFilterView::selectVstPlugin(int index)
{
	auto plugin_path = ui->comboBox_vst_list->itemData(index).toString();
	auto plugin_name = ui->comboBox_vst_list->itemText(index);
	obs_data_set_string(filterSettings, g_vst_path, plugin_path.toStdString().c_str());
	obs_source_update(filterSource, filterSettings);
	if (plugin_path.isEmpty()) {
		resetUi();
	} else {
		QString logInfo = QString("User selects vst plugin: ").append(plugin_name);
		PLS_UI_STEP(MAINFILTER_MODULE, qUtf8Printable(logInfo), ACTION_CLICK);
	}
}

void PLSVstFilterView::onOpenVstInterface() const
{
	auto plugin_name = ui->comboBox_vst_list->currentText();
	PLS_UI_STEP(MAINFILTER_MODULE, qUtf8Printable(plugin_name.prepend("User open vst plugin's window: ")), ACTION_CLICK);
	obs_properties_t *props = obs_source_properties(filterSource);
	obs_property_t *open_interface_pro = obs_properties_get(props, g_vst_open_interface);
	obs_property_button_clicked(open_interface_pro, filterSource);
	obs_properties_destroy(props);
}

void PLSVstFilterView::onOpenWhenActiveClicked(bool trigger) const
{
	QString logInfo("User trigger Open when active: ");
	PLS_UI_STEP(MAINFILTER_MODULE, qUtf8Printable(logInfo.append(trigger ? "On" : "Off")), ACTION_CLICK);
	obs_data_set_bool(filterSettings, g_vst_open_when_active, trigger);
	obs_source_update(filterSource, filterSettings);
}

void PLSVstFilterView::onShowScrollBar(bool show)
{
	if (show) {
		int width = ui->scrollArea->verticalScrollBar()->width();
		ui->scrollAreaWidgetContents->setContentsMargins(10, 0, 19 - width, 0);
	} else {
		ui->scrollAreaWidgetContents->setContentsMargins(QMargins(10, 0, 19, 0));
	}
}

/**
 * Customize a combobox with loading icons on head and list item view.
 */
ComboBoxWithIcon::ComboBoxWithIcon(QWidget *parent) : PLSComboBox(parent)
{
	loadingIcon = pls_new<QLabel>(this);
	loadingIcon->hide();
	loadingIcon->setObjectName("comboBoxStateIcon");

	loadingIcon->setStyleSheet("#comboBoxStateIcon{min-width: /*hdpi*/16px;"
				   "max-width: /*hdpi*/16px;min-height: /*hdpi*/16px;"
				   "background:transparent;"
				   "max-height: /*hdpi*/16px;image:url(:/resource/images/icon-error-3.svg);}");
	auto layout = pls_new<QHBoxLayout>(this);
	layout->addStretch();
	layout->addWidget(loadingIcon);
	layout->setContentsMargins(0, 0, 45, 0);

	view()->window()->setWindowFlags(Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
}

void ComboBoxWithIcon::showIcon()
{
	loadingIcon->show();
}

void ComboBoxWithIcon::hideIcon()
{
	loadingIcon->hide();
}
