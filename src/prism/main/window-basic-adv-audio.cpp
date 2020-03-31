#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QScrollArea>
#include <QPushButton>
#include <QLabel>
#include "window-basic-adv-audio.hpp"
#include "window-basic-main.hpp"
#include "item-widget-helpers.hpp"
#include "adv-audio-control.hpp"
#include "pls-app.hpp"
#include "qt-wrappers.hpp"

Q_DECLARE_METATYPE(OBSSource);

PLSBasicAdvAudio::PLSBasicAdvAudio(QWidget *parent)
	: PLSDialogView(parent),
	  sourceAddedSignal(obs_get_signal_handler(), "source_activate", OBSSourceAdded, this),
	  sourceRemovedSignal(obs_get_signal_handler(), "source_deactivate", OBSSourceRemoved, this)
{
	QScrollArea *scrollArea;
	QVBoxLayout *vlayout;
	QWidget *widget;
	QLabel *label;
	QSpacerItem *horizontalSpacer;

	mainLayout = new QGridLayout;
	mainLayout->setHorizontalSpacing(0);
	mainLayout->setVerticalSpacing(10);
	mainLayout->setContentsMargins(0, 0, 0, 0);

	//init the name title label
	int idx = 0;
	QString styleSheet = QString("font-weight:bold; font-size:14px; min-height:28px; max-height:28px;");
	label = new QLabel(QTStr("Basic.AdvAudio.Name"));
	label->setStyleSheet(styleSheet);
	mainLayout->addWidget(label, 0, idx++);

	//add horizon space item
	horizontalSpacer = new QSpacerItem(20, 20, QSizePolicy::Fixed, QSizePolicy::Minimum);
	mainLayout->addItem(horizontalSpacer, 0, idx++);

	//init the audio monitor label
#if defined(_WIN32) || defined(__APPLE__) || HAVE_PULSEAUDIO
	label = new QLabel(QTStr("Basic.AdvAudio.Monitoring"));
	label->setStyleSheet(styleSheet + "padding-left:3px");
	mainLayout->addWidget(label, 0, idx++);
#endif

	//add horizon space item
	horizontalSpacer = new QSpacerItem(25, 20, QSizePolicy::Fixed, QSizePolicy::Minimum);
	mainLayout->addItem(horizontalSpacer, 0, idx++);

	// add volumn label
	label = new QLabel(QTStr("Basic.AdvAudio.Volume"));
	label->setStyleSheet(styleSheet + "padding-left:1px;");
	mainLayout->addWidget(label, 0, idx++);

	//add horizon space it em
	horizontalSpacer = new QSpacerItem(25, 20, QSizePolicy::Fixed, QSizePolicy::Minimum);
	mainLayout->addItem(horizontalSpacer, 0, idx++);

	// add balance label
	label = new QLabel(QTStr("Basic.AdvAudio.Balance"));
	label->setStyleSheet(styleSheet);
	mainLayout->addWidget(label, 0, idx++);

	//add horizon space item
	horizontalSpacer = new QSpacerItem(23, 20, QSizePolicy::Fixed, QSizePolicy::Minimum);
	mainLayout->addItem(horizontalSpacer, 0, idx++);

	// add mono label
	label = new QLabel(QTStr("Basic.AdvAudio.Mono"));
	label->setAlignment(Qt::AlignCenter);
	label->setStyleSheet(styleSheet);
	mainLayout->addWidget(label, 0, idx++);

	//add horizon space item
	horizontalSpacer = new QSpacerItem(15, 20, QSizePolicy::Fixed, QSizePolicy::Minimum);
	mainLayout->addItem(horizontalSpacer, 0, idx++);

	// add sync offset label
	label = new QLabel(QTStr("Basic.AdvAudio.SyncOffset"));
	label->setStyleSheet(styleSheet);
	mainLayout->addWidget(label, 0, idx++);

	//add horizon space item
	horizontalSpacer = new QSpacerItem(25, 20, QSizePolicy::Fixed, QSizePolicy::Minimum);
	mainLayout->addItem(horizontalSpacer, 0, idx++);

	// add audio tracks label
	label = new QLabel(QTStr("Basic.AdvAudio.AudioTracks"));
	label->setStyleSheet(styleSheet);
	mainLayout->addWidget(label, 0, idx++);

	controlArea = new QWidget;
	controlArea->setLayout(mainLayout);
	controlArea->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

	vlayout = new QVBoxLayout;
	vlayout->setContentsMargins(25, 30, 0, 10);
	vlayout->addWidget(controlArea);

	widget = new QWidget;
	widget->setLayout(vlayout);
	widget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);

	scrollArea = new QScrollArea;
	scrollArea->setObjectName("advScrollArea");
	scrollArea->setWidget(widget);
	scrollArea->setWidgetResizable(true);
	scrollArea->setFrameShape(QFrame::NoFrame);

	vlayout = new QVBoxLayout;
	vlayout->setContentsMargins(0, 0, 0, 0);
	vlayout->addWidget(scrollArea);
	this->content()->setLayout(vlayout);

	installEventFilter(CreateShortcutFilter());

	/* enum user scene/sources */
	obs_enum_sources(EnumSources, this);

	resize(1187, 280);
	setHasMaxResButton(true);
	setCaptionButtonMargin(9);
	setWindowTitle(QTStr("Basic.AdvAudio"));
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
	//setSizeGripEnabled(true);
	setWindowModality(Qt::NonModal);
	setAttribute(Qt::WA_DeleteOnClose, true);

	//install namelabel event filter
	this->installEventFilter(this);
}

PLSBasicAdvAudio::~PLSBasicAdvAudio()
{
	PLSBasic *main = reinterpret_cast<PLSBasic *>(parent());
	for (size_t i = 0; i < controls.size(); ++i)
		delete controls[i];

	main->SaveProject();
}

bool PLSBasicAdvAudio::eventFilter(QObject *watched, QEvent *e)
{
	if (e->type() == QEvent::UpdateRequest) {
		if (watched == this) {
			for (auto control : controls) {
				control->setSourceName();
			}
		}
	}
	return PLSDialogView::eventFilter(watched, e);
}

bool PLSBasicAdvAudio::EnumSources(void *param, obs_source_t *source)
{
	PLSBasicAdvAudio *dialog = reinterpret_cast<PLSBasicAdvAudio *>(param);
	uint32_t flags = obs_source_get_output_flags(source);
	if ((flags & OBS_SOURCE_AUDIO) != 0 && obs_source_audio_active(source) && obs_source_active(source)) {
		dialog->AddAudioSource(source);
	}
	return true;
}

void PLSBasicAdvAudio::OBSSourceAdded(void *param, calldata_t *calldata)
{
	OBSSource source((obs_source_t *)calldata_ptr(calldata, "source"));
	QMetaObject::invokeMethod(reinterpret_cast<PLSBasicAdvAudio *>(param), "SourceAdded", Q_ARG(OBSSource, source));
}

void PLSBasicAdvAudio::OBSSourceRemoved(void *param, calldata_t *calldata)
{
	OBSSource source((obs_source_t *)calldata_ptr(calldata, "source"));
	QMetaObject::invokeMethod(reinterpret_cast<PLSBasicAdvAudio *>(param), "SourceRemoved", Q_ARG(OBSSource, source));
}

inline void PLSBasicAdvAudio::AddAudioSource(obs_source_t *source)
{
	QString strName = obs_source_get_name(source);
	assert(strName.length() > 0);
	for (auto control : controls) {
		if (control->objectName() == strName)
			return;
	}

	PLSAdvAudioCtrl *control = new PLSAdvAudioCtrl(mainLayout, source);
	control->setObjectName(strName);

	InsertQObjectByName(controls, control);
	for (auto control : controls) {
		control->ShowAudioControl(mainLayout);
	}
}

void PLSBasicAdvAudio::SourceAdded(OBSSource source)
{
	uint32_t flags = obs_source_get_output_flags(source);

	if ((flags & OBS_SOURCE_AUDIO) == 0)
		return;
	//check the audio source active status
	if (!obs_source_audio_active(source)) {
		return;
	}
	AddAudioSource(source);
}

void PLSBasicAdvAudio::SourceRemoved(OBSSource source)
{
	uint32_t flags = obs_source_get_output_flags(source);

	if ((flags & OBS_SOURCE_AUDIO) == 0)
		return;

	for (size_t i = 0; i < controls.size(); i++) {
		if (controls[i]->GetSource() == source) {
			delete controls[i];
			controls.erase(controls.begin() + i);
			break;
		}
	}
}
