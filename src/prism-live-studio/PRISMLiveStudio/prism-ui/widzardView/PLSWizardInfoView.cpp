#include "PLSWizardInfoView.h"
#include "ui_PLSWizardInfoView.h"
#include <qtimer.h>
#include "pls-shared-functions.h"
#include "libutils-api.h"

#define Loading QObject::tr("WizardView.LoadingSchedule")

PLSWizardInfoView::PLSWizardInfoView(ViewType type, QWidget *parent) : QPushButton(parent), m_type(type)
{
	ui = pls_new<Ui::PLSWizardInfoView>();
	ui->setupUi(this);
	setObjectName("PLSWizardInfoView");
	setInfoView(type);
	setLoaingImagePath(":/resource/images/loading/loading-%1.svg");
	loading(false);
	this->layout()->setContentsMargins(0, 0, 0, 0);
	ui->contentFrame->setAttribute(Qt::WA_TransparentForMouseEvents);
}

PLSWizardInfoView::~PLSWizardInfoView()
{
	pls_delete(ui);
}

void PLSWizardInfoView::setInfo(const QString &title, const QString &timeStr, const QString &plaform)
{
	this->setInfoText(title);
	this->setTimeText(timeStr);
	this->setTipText(plaform);
	this->ui->infoLabel->setEnabled(!timeStr.isEmpty());
	this->ui->infoFrame->setVisible(!timeStr.isEmpty());
}

void PLSWizardInfoView::setInfoText(const QString &text)
{
	ui->infoLabel->setText(text);
}

void PLSWizardInfoView::setTimeText(const QString &text)
{
	ui->timeLabel->setText(text);
}

void PLSWizardInfoView::setTipText(const QString &text)
{
	ui->tipLabel->setText(text);
}

void PLSWizardInfoView::loading(bool start)
{
	auto nextImage = [this]() {
		static int step = 1;
		auto imPath = mloadingPath.arg(step);
		ui->loading->setStyleSheet(QString("image:url(%1);").arg(imPath));
		step = step >= 8 ? 1 : ++step;
	};

	if (loadingTimer == nullptr) {
		loadingTimer = pls_new<QTimer>(this);
		loadingTimer->setInterval(200);
		connect(loadingTimer, &QTimer::timeout, this, nextImage, Qt::QueuedConnection);
	}

	if (start) {
		loadingTimer->start();
		QString html = R"(<p style="color:#BABABA;">%1</p>)";
		setInfo(html.arg(Loading));
		ui->loading->show();

	} else {
		loadingTimer->stop();
		ui->loading->hide();
	}
}

void PLSWizardInfoView::setInfoView(ViewType type)
{
	switch (type) {
	case PLSWizardInfoView::ViewType::Alert:
		ui->iconLabel->setProperty("type", "alert");
		ui->timeLabel->setVisible(false);
		ui->pointLabel->setVisible(false);
		ui->tipLabel->setProperty("type", "alert");
		setProperty("notShowHandCursor", true);
		break;
	case PLSWizardInfoView::ViewType::LiveInfo:
		ui->iconLabel->setProperty("type", "calendar");
		ui->tipLabel->setProperty("type", "calendar");
		ui->infoLabel->setWordWrap(false);
		setProperty("notShowHandCursor", true);
		break;
	case PLSWizardInfoView::ViewType::Blog:
		ui->iconLabel->setProperty("type", "blog");
		ui->infoFrame->setVisible(false);
		setProperty("showHandCursor", true);
		break;
	case PLSWizardInfoView::ViewType::Que:
		ui->iconLabel->setProperty("type", "que");
		ui->infoFrame->setVisible(false);
		setProperty("showHandCursor", true);
		break;
	default:
		break;
	}
}
