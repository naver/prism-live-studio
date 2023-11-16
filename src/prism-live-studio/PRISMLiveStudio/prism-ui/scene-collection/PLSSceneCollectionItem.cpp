#include "PLSSceneCollectionItem.h"
#include "ui_PLSSceneCollectionItem.h"
#include "pls-common-define.hpp"
#include "frontend-api.h"
#include <QFileInfo>
#include <QDateTime>
#include <QPushButton>
#include <QPainter>
#include <QToolTip>
#include <QMouseEvent>
#include "log/module_names.h"
#include "action.h"
#include "log/log.h"
#include "obs-app.hpp"
#include "libutils-api.h"
#include "libui.h"

using namespace common;

PLSSceneCollectionItem::PLSSceneCollectionItem(const QString &name, const QString &path, bool current_, bool textMode_, QWidget *parent)
	: QFrame(parent), fileName(name), filePath(path), current(current_), textMode(textMode_)
{
	ui = pls_new<Ui::PLSSceneCollectionItem>();
	ui->setupUi(this);
	setMouseTracking(true);
	ui->horizontalLayout->setContentsMargins(12, 0, 12, 0);

	ui->nameLabel->setVisible(false);
	ui->timeLabel->setVisible(false);
	timeVisible = !textMode;

	ui->applyBtn->setToolTip(QTStr("Scene.Collection.Apply"));
	ui->renameBtn->setToolTip(QTStr("Scene.Collection.Rename"));
	ui->deleteBtn->setToolTip(QTStr("Scene.Collection.Delete"));

	UpdateModifiedTimeStamp();
	SetMouseStatus(PROPERTY_VALUE_MOUSE_STATUS_NORMAL);
	SetButtonVisible(false);
	SetCurrentStyles(current);
	setProperty("showHandCursor", true);

	connect(ui->applyBtn, &QPushButton::clicked, this, &PLSSceneCollectionItem::OnApplyBtnClicked);
	connect(ui->deleteBtn, &QPushButton::clicked, this, &PLSSceneCollectionItem::OnDeleteBtnClicked);
	connect(ui->renameBtn, &QPushButton::clicked, this, &PLSSceneCollectionItem::OnRenameBtnClicked);
	connect(ui->advBtn, &QPushButton::clicked, this, &PLSSceneCollectionItem::OnAdvBtnClicked);
}

PLSSceneCollectionItem::~PLSSceneCollectionItem()
{
	pls_delete(ui);
}

QString PLSSceneCollectionItem::GetFileName() const
{
	return fileName;
}

QString PLSSceneCollectionItem::GetFilePath() const
{
	return filePath;
}

void PLSSceneCollectionItem::UpdateModifiedTimeStamp()
{
	if (textMode) {
		return;
	}

	QFileInfo info(filePath);
	qint64 timeStamp = info.lastModified().toSecsSinceEpoch();
	long long duration = QDateTime::currentSecsSinceEpoch() - timeStamp;

	int minute = 60;
	int hour = minute * 60;
	int day = 24 * hour;

	if (duration > day * 7) {
		timeVisible = false;
		return;
	}

	timeVisible = true;
	if (duration > day) {
		auto saveDay = duration / day;
		timeText = QTStr("Scene.Collection.Save.Day").arg(saveDay);
	} else if (duration > hour) {
		auto saveHour = duration / hour;
		timeText = QTStr("Scene.Collection.Save.Hour").arg(saveHour);

	} else {
		auto saveMinute = duration / minute;
		timeText = QTStr("Scene.Collection.Save.Minute").arg(saveMinute);
	}
}

void PLSSceneCollectionItem::Rename(const QString &name, const QString &path)
{
	fileName = name;
	filePath = path;

	UpdateModifiedTimeStamp();
}

void PLSSceneCollectionItem::SetDeleteButtonDisable(bool disable) const
{
	ui->deleteBtn->setDisabled(disable);
}

void PLSSceneCollectionItem::Update(const PLSSceneCollectionData &data)
{
	fileName = data.fileName;
	filePath = data.filePath;

	SetDeleteButtonDisable(data.delButtonDisable);
	SetCurrentStyles(data.current);
	UpdateModifiedTimeStamp();
}

void PLSSceneCollectionItem::ClearDropLine()
{
	lineType = DropLine::DropLineNone;
	update();
}

void PLSSceneCollectionItem::DrawDropLine(DropLine type)
{
	lineType = type;
	update();
}

void PLSSceneCollectionItem::mousePressEvent(QMouseEvent *event)
{
	if (textMode) {
		emit applyClicked(fileName, filePath, true);
	}

	QFrame::mousePressEvent(event);
}

void PLSSceneCollectionItem::mouseDoubleClickEvent(QMouseEvent *event)
{
	emit applyClicked(fileName, filePath, false);
	QFrame::mouseDoubleClickEvent(event);
}

void PLSSceneCollectionItem::mouseMoveEvent(QMouseEvent *event)
{
	QRect rect(leftMargin, 0, nameRealWidth, height());
	pls_used(rect);
	if (rect.contains(event->pos())) {
		QToolTip::showText(QCursor::pos(), fileName, this);
	}
	QFrame::mouseMoveEvent(event);
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
void PLSSceneCollectionItem::enterEvent(QEnterEvent *event)
#else
void PLSSceneCollectionItem::enterEvent(QEvent *event)
#endif
{
	SetMouseStatus(PROPERTY_VALUE_MOUSE_STATUS_HOVER);
	SetButtonVisible(true);
	QFrame::enterEvent(event);
}

void PLSSceneCollectionItem::leaveEvent(QEvent *event)
{
	QToolTip::hideText();

	if (!advMenuShow) {
		SetMouseStatus(PROPERTY_VALUE_MOUSE_STATUS_NORMAL);
		SetButtonVisible(false);
	}
	QFrame::leaveEvent(event);
}

void PLSSceneCollectionItem::paintEvent(QPaintEvent *event)
{
	QFrame::paintEvent(event);

	auto font = ui->nameLabel->font();
	font.setBold(current);
	QFontMetrics metrics(font);
	int nameWidth = metrics.horizontalAdvance(fileName);

	QPainter painter(this);
	painter.setFont(font);

	painter.setPen(QColor("#ffffff"));
	if (current) {
		painter.setPen(QColor("#effc35"));
	}

	QFontMetrics titleMetrics(ui->timeLabel->font());
	int timeWidth = titleMetrics.horizontalAdvance(timeText);

	SetLeftRightMargin();

	int availWidth = 0;
	if (timeVisible) {
		availWidth = this->width() - leftMargin - rightMargin - 8 - timeWidth - GetButtonWidth();
	} else {
		availWidth = this->width() - leftMargin - rightMargin - GetButtonWidth();
	}

	QTextOption option;
	option.setWrapMode(QTextOption::NoWrap);
	option.setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	if (availWidth > nameWidth) {
		nameRealWidth = nameWidth;
		painter.drawText(QRect(leftMargin, 0, nameWidth, height()), fileName, option);
	} else {
		nameRealWidth = availWidth;
		painter.drawText(QRect(leftMargin, 0, availWidth, height()), metrics.elidedText(fileName, Qt::ElideRight, availWidth), option);
	}

	if (timeVisible) {
		painter.setPen(QColor("#666666"));
		painter.setFont(ui->timeLabel->font());

		int minWidth = std::min(nameWidth, availWidth);
		painter.drawText(QRect(leftMargin + minWidth + 8, 1, timeWidth, height()), timeText, option);
	}

	DrawDropLine(&painter);
}

void PLSSceneCollectionItem::OnApplyBtnClicked()
{
	emit applyClicked(fileName, filePath, false);
}

void PLSSceneCollectionItem::OnExportBtnClicked()
{
	emit exportClicked(fileName, filePath);
}

void PLSSceneCollectionItem::OnRenameBtnClicked()
{
	emit renameClicked(fileName, filePath);
}

void PLSSceneCollectionItem::OnDuplicateBtnClicked()
{
	emit duplicateClicked(fileName, filePath);
}

void PLSSceneCollectionItem::OnDeleteBtnClicked()
{
	emit deleteClicked(fileName, filePath);
}

void PLSSceneCollectionItem::OnAdvBtnClicked()
{
	QMenu popup(this);
	popup.setWindowFlags(popup.windowFlags() | Qt::NoDropShadowWindowHint);
	popup.setObjectName("advButtonMenu");

	auto duplicateAction = pls_new<QAction>(QTStr("Scene.Collection.Duplicate"), &popup);
	auto exportAction = pls_new<QAction>(QTStr("Scene.Collection.Export"), &popup);

	connect(duplicateAction, &QAction::triggered, this, &PLSSceneCollectionItem::OnDuplicateBtnClicked);
	connect(exportAction, &QAction::triggered, this, &PLSSceneCollectionItem::OnExportBtnClicked);

	popup.addAction(duplicateAction);
	popup.addAction(exportAction);

	pls_push_modal_view(&popup);
	advMenuShow = true;
	popup.exec(QCursor::pos());
	advMenuShow = false;
	pls_pop_modal_view(&popup);
	QApplication::postEvent(this, new QEvent(QEvent::Leave));
}

void PLSSceneCollectionItem::SetMouseStatus(const char *status)
{
	pls_flush_style(this, PROPERTY_NAME_MOUSE_STATUS, status);
	pls_flush_style(ui->nameLabel, PROPERTY_NAME_MOUSE_STATUS, status);
	pls_flush_style(ui->timeLabel, PROPERTY_NAME_MOUSE_STATUS, status);
}

void PLSSceneCollectionItem::SetButtonVisible(bool visible)
{
	buttonVisible = visible && !textMode;
	ui->applyBtn->setVisible(buttonVisible);
	ui->renameBtn->setVisible(buttonVisible);
	ui->deleteBtn->setVisible(buttonVisible);
	ui->advBtn->setVisible(buttonVisible);

	if (buttonVisible) {
		ui->horizontalLayout_2->setSpacing(10);
		ui->horizontalLayout->setSpacing(14);
		ui->horizontalLayout->setContentsMargins(18, 0, 0, 0);
	} else {
		ui->horizontalLayout_2->setSpacing(0);
		ui->horizontalLayout->setSpacing(0);
		ui->horizontalLayout->setContentsMargins(0, 0, 0, 0);
	}
}

void PLSSceneCollectionItem::SetCurrentStyles(bool current_)
{
	current = current_;
	pls_flush_style(ui->nameLabel, "current", current);
}

int PLSSceneCollectionItem::GetButtonWidth()
{
	if (!buttonVisible) {
		return 0;
	}
	return ui->applyBtn->width() + ui->renameBtn->width() + ui->deleteBtn->width() + ui->advBtn->width() + 14 + 14 + 10 + 18;
}

void PLSSceneCollectionItem::SetLeftRightMargin()
{
	leftMargin = 12;
	rightMargin = leftMargin;
	if (buttonVisible) {
		rightMargin = 9;
		return;
	}
	if (timeVisible) {
		rightMargin = 12;
		return;
	}
	if (textMode) {
		rightMargin = 10;
	} else {
		rightMargin = 22;
	}
}

void PLSSceneCollectionItem::DrawDropLine(QPainter *painter)
{
	if (!painter || lineType == DropLine::DropLineNone)
		return;

	painter->save();
	QPen pen;
	pen.setWidth(1);
	pen.setColor(QColor("#effc35"));
	painter->setPen(pen);
	if (DropLine::DropLineBottom == lineType) {
		painter->drawLine(rect().bottomLeft(), rect().bottomRight());
	} else {
		painter->drawLine(rect().topLeft(), rect().topRight());
	}
	painter->restore();
}
