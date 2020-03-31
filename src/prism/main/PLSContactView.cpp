#include "PLSContactView.hpp"
#include "ui_PLSContactView.h"
#include <QTextFrame>
#include <QFileDialog>
#include <QStandardPaths>
#include "PLSFileItemView.hpp"
#include <QFontMetrics>
#include "pls-common-define.hpp"
#include "frontend-api.h"
#include "log/log.h"
#include <QTimer>
#include "pls-app.hpp"

#define TEXT_EDIT_TOP_MARGIN 13
#define TEXT_EDIT_LEFT_MARGIN 15
#define TEXT_EDIT_RIGHT_MARGIN 10

#define TAG_LEFT_MARGIN 7
#define TAG_RIGHT_MARGIN 20
#define TAG_LIST_WIDGET_WIDTH 376
#define TAG_TOP_MARGIN 10
#define TAG_HEIGHT 24
#define TAG_EXTRA_MARGIN 38

static const int maxFileNumber = 5;
static const int textEditLengthLimit = 5000;
static const qint64 maxTotalFileSize = 20 * 1024 * 1024;
static const qint64 maxSingleFileSize = 10 * 1024 * 1024;

#define CONFIG_BASIC_WINDOW_MODULE "BasicWindow"
#define CONFIG_CONTACT_EMAIL_MODULE "ContactEmail"

PLSContactView::PLSContactView(QWidget *parent) : PLSDialogView(parent), ui(new Ui::PLSContactView)
{
	setResizeEnabled(false);
	ui->setupUi(this->content());
	this->setFixedSize(QSize(480, 489));
	setupLineEdit();
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
	QString usercode = pls_get_prism_usercode().prepend(" ");
	ui->identificationLabel->setText(tr("contact.email.report.ID.prefix").append(usercode));
	initConnect();
}

PLSContactView::~PLSContactView()
{
	delete ui;
}

void PLSContactView::updateItems()
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
	for (int i = 0; i < m_fileLists.size(); i++) {
		PLSFileItemView *itemView = new PLSFileItemView(i);
		itemView->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
		QObject::connect(itemView, &PLSFileItemView::deleteItem, this, &PLSContactView::deleteItem);
		QFileInfo fileInfo = m_fileLists.at(i);
		QString fileName = fileInfo.fileName();
		QFontMetrics fontMetric(itemView->fileNameLabelFont());
		int maxWidth = TAG_LIST_WIDGET_WIDTH - TAG_LEFT_MARGIN - TAG_RIGHT_MARGIN - TAG_EXTRA_MARGIN;
		int fileNameWidth = fontMetric.boundingRect(fileName).width();
		if (fileNameWidth > maxWidth) {
			fileNameWidth = maxWidth;
		}
		QString str = fontMetric.elidedText(fileName, Qt::ElideRight, maxWidth);
		itemView->setFileName(str);
		int sizeHintWidth = fileNameWidth + TAG_EXTRA_MARGIN;
		QListWidgetItem *listItem = new QListWidgetItem;
		listItem->setSizeHint(QSize(sizeHintWidth, TAG_HEIGHT));
		ui->tagListWidget->addItem(listItem);
		ui->tagListWidget->setItemWidget(listItem, itemView);
	}
}

void PLSContactView::deleteItem(int index)
{
	PLS_UI_STEP(CONTACT_US_MODULE, " PLSContactView deleteItem Button", ACTION_CLICK);
	m_fileLists.removeAt(index);
	ui->inquryTipLabel->setText("");
	updateItems();
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
		ui->emailLineEdit->setText(QString(value));
	}
}

void PLSContactView::initConnect()
{
	QObject::connect(ui->emailLineEdit, &QLineEdit::editingFinished, this, &PLSContactView::on_emailLineEdit_editingFinished);
	QObject::connect(ui->emailLineEdit, &QLineEdit::textChanged, this, &PLSContactView::on_emailLineEdit_textChanged);
	QObject::connect(ui->textEdit, &QTextEdit::textChanged, this, &PLSContactView::on_textEdit_textChanged);
	QObject::connect(ui->fileButton, &QPushButton::clicked, this, &PLSContactView::on_fileButton_clicked);
	QObject::connect(ui->sendButton, &QPushButton::clicked, this, &PLSContactView::on_sendButton_clicked);
	QObject::connect(ui->cancelButton, &QPushButton::clicked, this, &PLSContactView::on_cancelButton_clicked);
}

bool PLSContactView::checkAddFileValid(const QFileInfo &fileInfo)
{
	bool valid = true;
	ui->inquryTipLabel->clear();
	if (m_fileLists.size() >= maxFileNumber) {
		valid = false;
		ui->inquryTipLabel->setText(tr("contact.report.file.max.count"));
	} else if (!checkTotalFileSizeValid(fileInfo)) {
		valid = false;
		ui->inquryTipLabel->setText(tr("contact.report.file.max.20M"));
	} else if (!checkSingleFileSizeValid(fileInfo)) {
		valid = false;
		ui->inquryTipLabel->setText(tr("contact.report.file.max.10M"));
	} else if (!checkFileFormatValid(fileInfo)) {
		valid = false;
		ui->inquryTipLabel->setText(tr("contact.report.file.format.error"));
	}
	return valid;
}

bool PLSContactView::checkTotalFileSizeValid(const QFileInfo &fileInfo)
{
	qint64 totalSize = fileInfo.size();
	for (QFileInfo info : m_fileLists) {
		totalSize += info.size();
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
				       << "log"
				       << "zip";
		if (!allFilterExtensionList.contains(fileInfo.suffix().toLower())) {
			valid = false;
		}
	}
	return valid;
}

void PLSContactView::on_fileButton_clicked()
{
	PLS_UI_STEP(CONTACT_US_MODULE, " PLSContactView fileButton Button", ACTION_CLICK);
	if (m_fileLists.size() >= maxFileNumber) {
		ui->inquryTipLabel->setText(tr("contact.report.file.max.count"));
		return;
	}
	QString dir = QStandardPaths::standardLocations(QStandardPaths::DesktopLocation).first();
	QString filter("Image Files(*.bmp *.jpg *.gif *.png);;Video Files(*.avi *.mp4 *.mpv *.mov);;Text Files(*.txt);;Log Files(*.log);;Zip Files(*.zip)");
	QStringList paths = QFileDialog::getOpenFileNames(this, QString(), dir, filter);
	if (paths.size() > 0) {
		for (const QString &path : paths) {
			QFileInfo fileInfo(path);
			if (checkAddFileValid(fileInfo)) {
				m_fileLists.append(fileInfo);
			}
		}
		updateItems();
		ui->tagListWidget->scrollToBottom();
	}
}

void PLSContactView::on_sendButton_clicked()
{
	PLS_UI_STEP(CONTACT_US_MODULE, " PLSContactView sendButton Button", ACTION_CLICK);
	//background question,dont't care the request result.
	QString textContent = ui->textEdit->toPlainText();
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
