#include "PLSOpenSourceView.h"
#include "ui_PLSOpenSourceView.h"
#include <QTextFrame>
#include <platform.hpp>
#include <QFile>

#define TEXT_EDIT_TOP_MARGIN 15
#define TEXT_EDIT_LEFT_MARGIN 15
#define TEXT_EDIT_RIGHT_MARGIN 10

PLSOpenSourceView::PLSOpenSourceView(QWidget *parent) : PLSDialogView(parent), ui(new Ui::PLSOpenSourceView)
{
	ui->setupUi(this->content());
	setupTextEdit();
	setFixedSize(720, 458);
	connect(ui->confirmButton, SIGNAL(clicked()), this, SLOT(on_confirmButton_clicked()));
}

PLSOpenSourceView::~PLSOpenSourceView()
{
	delete ui;
}

void PLSOpenSourceView::setupTextEdit()
{
	// modify the text frame layout
	QTextDocument *document = ui->textEdit->document();
	QTextFrame *rootFrame = document->rootFrame();
	QTextFrameFormat format;
	format.setLeftMargin(TEXT_EDIT_LEFT_MARGIN);
	format.setTopMargin(TEXT_EDIT_TOP_MARGIN);
	format.setRightMargin(TEXT_EDIT_RIGHT_MARGIN);
	format.setBottomMargin(TEXT_EDIT_TOP_MARGIN);
	format.setBorderBrush(Qt::transparent);
	format.setBorder(3);
	rootFrame->setFrameFormat(format);

	// read open source license file
	std::string path;
	QString error = "Error! File could not be read.";
	if (!GetDataFilePath("license/License_PRISMLiveStudio_Win.txt", path)) {
		ui->textEdit->setPlainText(error);
		return;
	}

	// read file to textEdit
	QFile file(QString::fromStdString(path));
	if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		ui->textEdit->setPlainText(file.readAll());
	}
}

void PLSOpenSourceView::on_confirmButton_clicked()
{
	this->accept();
}
