#include "about-view.hpp"
#include "ui_PLSAboutView.h"
#include "ui-config.h"
#include "pls-common-language.hpp"

PLSAboutView::PLSAboutView(QWidget *parent, PLSDpiHelper dpiHelper) : PLSDialogView(parent, dpiHelper), ui(new Ui::PLSAboutView)
{
	setResizeEnabled(false);
	dpiHelper.setCss(this, {PLSCssIndex::PLSAboutView});
	dpiHelper.setFixedSize(this, {342, 360});
	ui->setupUi(this->content());
	QString version = QString::asprintf("%s %s", tr(ABOUT_CURRENT_VERSION).toUtf8().constData(), PLS_VERSION);
	ui->versionLabel->setText(version);
	bool disabled = pls_is_living_or_recording();
	ui->checkUpdateButton->setDisabled(disabled);
	initConnect();
}

PLSAboutView::~PLSAboutView()
{
	delete ui;
}

void PLSAboutView::initConnect()
{
	connect(ui->checkUpdateButton, SIGNAL(clicked()), this, SLOT(on_checkUpdateButton_clicked()));
}

void PLSAboutView::on_checkUpdateButton_clicked()
{
	this->accept();
}
