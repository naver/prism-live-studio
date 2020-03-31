#include "PLSColorFilterView.h"
#include "ui_PLSColorFilterView.h"

#include "pls-common-define.hpp"
#include "obs-properties.h"
#include "qt-wrappers.hpp"
#include "frontend-api.h"
#include "json-data-handler.hpp"
#include "log.h"
#include "log/module_names.h"
#include "action.h"

#include <QDir>
#include <QLabel>
#include <QImage>
#include <QStyle>
#include <QScrollArea>
#include <QScrollBar>
#include <QPainter>

#define ORIGINAL_THUMBNAIL_PNG "original_thumbnail.png"
#define ORIGINAL_PNG "original.png"
#define N1_THUMBNAIL_PNG "01_N1_thumbnail.png"
#define N1_PNG "01_N1.png"
#define N1_SHORT_TITLE "N1"
#define COLOR_FILTER_TEXT_FONT_SIZE 11
#define COLOR_FILTER_TEXT_COLOR "#FFFFFF"
#define COLOR_FILTER_TEXT_MARGIN_TOP 53

PLSColorFilterView::PLSColorFilterView(obs_source_t *source_, QWidget *parent) : QFrame(parent), source(source_), ui(new Ui::PLSColorFilterView)
{
	ui->setupUi(this);
	ui->horizontalLayout->setAlignment(Qt::AlignLeft);

	this->resize(FILTERS_DISPLAY_VIEW_MAX_WIDTH, COLOR_FILTERS_IMAGE_FIXED_WIDTH);
	LoadDefaultColorFilter();

	leftScrollBtn = new QPushButton(this);
	leftScrollBtn->setObjectName(OBJECT_NAME_LEFT_SCROLL_BTN);
	leftScrollBtn->move(5, this->y() + 15);
	connect(leftScrollBtn, &QPushButton::clicked, this, &PLSColorFilterView::OnLeftScrollBtnClicked);

	rightScrollBtn = new QPushButton(this);
	rightScrollBtn->setObjectName(OBJECT_NAME_RIGHT_SCROLL_BTN);
	rightScrollBtn->move(this->width() - 25, this->y() + 15);
	connect(rightScrollBtn, &QPushButton::clicked, this, &PLSColorFilterView::OnRightScrollBtnClicked);
}

PLSColorFilterView::~PLSColorFilterView()
{
	delete ui;
}

void PLSColorFilterView::ResetColorFilter()
{
	QString defaultFilter = pls_get_color_filter_dir_path().append(ORIGINAL_PNG);
	UpdateFilterItemStyle(defaultFilter);
	auto bar = ui->scrollArea->horizontalScrollBar();
	bar->setValue(0);
	UpdateColorFilterSource(defaultFilter);
}

OBSSource PLSColorFilterView::GetSource()
{
	return source;
}

void PLSColorFilterView::showEvent(QShowEvent *event)
{
	QFrame::showEvent(event);
	GetDefaultColorFilterPath();
}

void PLSColorFilterView::OnLeftScrollBtnClicked()
{
	auto bar = ui->scrollArea->horizontalScrollBar();
	bar->setValue(bar->value() - GetScrollAreaWidth());
}

void PLSColorFilterView::OnRightScrollBtnClicked()
{
	auto bar = ui->scrollArea->horizontalScrollBar();
	bar->setValue(bar->value() + GetScrollAreaWidth());
}

void PLSColorFilterView::OnColorFilterImageMousePressd(PLSColorFilterImage *image)
{
	if (!image) {
		return;
	}
	PLS_UI_STEP(MAINFILTER_MODULE, image->GetPath().toStdString().c_str(), ACTION_CLICK);
	UpdateFilterItemStyle(image);
	UpdateColorFilterSource(image->GetPath());
}

int PLSColorFilterView::GetScrollAreaWidth()
{
	int value = this->width() / (COLOR_FILTERS_IMAGE_FIXED_WIDTH + 10);
	return value * COLOR_FILTERS_IMAGE_FIXED_WIDTH;
}

void PLSColorFilterView::CreateColorFilterImage(const QString &name, const QString &shortTitle, const QString &filterPath, const QString &thumbnailPath)
{
	PLSColorFilterImage *filterImage = new PLSColorFilterImage(name, shortTitle, filterPath, thumbnailPath, this);
	if (0 == GetDefaultColorFilterPath().compare(filterPath)) {
		filterImage->SetClickedState(true);
	}

	connect(filterImage, &PLSColorFilterImage::MousePressd, this, &PLSColorFilterView::OnColorFilterImageMousePressd);
	QImage image;
	if (image.load(thumbnailPath)) {
		QPixmap pixmap = QPixmap::fromImage(image).scaled(QSize(this->width(), this->height()), Qt::KeepAspectRatio, Qt::SmoothTransformation);
		filterImage->SetPixMap(pixmap);
		filterImage->setObjectName(OBJECT_NAME_COLOR_FILTER_LABEL);
		ImageVec.push_back(filterImage);

		if (0 == thumbnailPath.compare(pls_get_color_filter_dir_path().append(ORIGINAL_THUMBNAIL_PNG))) {
			ui->horizontalLayout->insertWidget(0, filterImage);
		} else {
			ui->horizontalLayout->addWidget(filterImage);
		}
	}
}

void PLSColorFilterView::UpdateFilterItemStyle(PLSColorFilterImage *image)
{
	if (!image) {
		return;
	}

	for (auto &item : ImageVec) {
		if (!item) {
			continue;
		}
		if (image == item) {
			item->SetClickedState(true);
		} else {
			item->SetClickedState(false);
		}
	}
}

void PLSColorFilterView::UpdateFilterItemStyle(const QString &imagePath)
{
	for (auto &item : ImageVec) {
		if (!item) {
			continue;
		}
		if (imagePath == item->GetPath()) {
			item->SetClickedState(true);
		} else {
			item->SetClickedState(false);
		}
	}
}

void PLSColorFilterView::UpdateColorFilterSource(const QString &path)
{
	obs_properties_t *props = obs_source_properties(source);
	QString name = obs_source_get_name(source);
	obs_property_t *property = obs_properties_first(props);
	while (property) {
		obs_property_type type = obs_property_get_type(property);
		if (OBS_PROPERTY_PATH == type) {
			const char *name = obs_property_name(property);
			OBSData settings = obs_source_get_settings(source);
			if (path == pls_get_color_filter_dir_path() + ORIGINAL_PNG) {
				emit OriginalPressed(true);
			} else {
				emit OriginalPressed(false);
			}
			obs_data_set_string(settings, name, QT_TO_UTF8(path));

			obs_source_update(source, settings);
			break;
		}
		obs_property_next(&property);
	}
	obs_properties_destroy(props);
}

void PLSColorFilterView::LoadDefaultColorFilter()
{
	QDir dir(pls_get_color_filter_dir_path());

	// origin thumbnail + N1
	CreateColorFilterImage(ORIGINAL_PNG, "", pls_get_color_filter_dir_path() + ORIGINAL_PNG, pls_get_color_filter_dir_path() + ORIGINAL_THUMBNAIL_PNG);
	CreateColorFilterImage(N1_PNG, N1_SHORT_TITLE, pls_get_color_filter_dir_path() + N1_PNG, pls_get_color_filter_dir_path() + N1_THUMBNAIL_PNG);

	QByteArray array;
	int index = 1;
	if (PLSJsonDataHandler::getJsonArrayFromFile(array, pls_get_color_filter_dir_path() + COLOR_FILTER_JSON_FILE)) {
		QJsonObject objectJson = QJsonDocument::fromJson(array).object();
		const QJsonArray &jsonArray = objectJson.value(HTTP_ITEMS).toArray();
		for (auto item : jsonArray) {
			QString name = item[HTTP_ITEM_ID].toString();
			QString title = item[TITLE].toString();
			QString shortTitle = item[SHORT_TITLE].toString();
			QString number{};
			if (index < COLOR_FILTER_ORDER_NUMBER) {
				number = number.append("0").append(QString::number(index++));
			} else {
				number = number.append(QString::number(index++));
			}
			QString colorFilterName = number + "_" + name + "_" + shortTitle + COLOR_FILTER_IMAGE_FORMAT_PNG;
			QString path = pls_get_color_filter_dir_path() + colorFilterName;
			QString thumbnailName = pls_get_color_filter_dir_path() + number + "_" + name + "_" + shortTitle + COLOR_FILTER_THUMBNAIL + COLOR_FILTER_IMAGE_FORMAT_PNG;

			CreateColorFilterImage(colorFilterName, shortTitle, path, thumbnailName);
		}
	}

	SetDefaultCurrentColorFilter();
}

QString PLSColorFilterView::GetDefaultColorFilterPath()
{
	QString path{};
	obs_properties_t *props = obs_source_properties(this->source);
	if (!props) {
		return path;
	}
	QString name = obs_source_get_name(this->source);
	obs_property_t *property = obs_properties_first(props);
	while (property) {
		obs_property_type type = obs_property_get_type(property);
		if (OBS_PROPERTY_PATH == type) {
			const char *name = obs_property_name(property);
			OBSData settings = obs_source_get_settings(source);
			path = obs_data_get_string(settings, name);
			break;
		}
		obs_property_next(&property);
	}
	obs_properties_destroy(props);

	if (path.isEmpty() || pls_get_color_filter_dir_path() + ORIGINAL_PNG == path) {
		emit OriginalPressed(true);
		return pls_get_color_filter_dir_path().append(ORIGINAL_PNG);
	} else {
		emit OriginalPressed(false);
	}
	return path;
}

void PLSColorFilterView::SetDefaultCurrentColorFilter()
{
	bool isFind = false;
	for (auto &image : ImageVec) {
		if (image && image->GetClickedState()) {
			isFind = true;
			break;
		}
	}

	if (!isFind) {
		UpdateFilterItemStyle(pls_get_color_filter_dir_path().append(ORIGINAL_PNG));
	}
}

PLSColorFilterImage::PLSColorFilterImage(const QString &name_, const QString &shortTitle_, const QString &path_, const QString &thumbnail_, QWidget *parent /*= nullptr*/)
	: QPushButton(parent), name(name_), shortTitle(shortTitle_), path(path_), thumbnail(thumbnail_)
{
	connect(this, &PLSColorFilterImage::ClickedStateChanged, this, &PLSColorFilterImage::SetClickedStyle);
}

QString PLSColorFilterImage::GetPath()
{
	return path;
}

QString PLSColorFilterImage::GetThumbnailPath()
{
	return thumbnail;
}

void PLSColorFilterImage::SetClickedState(bool clicked)
{
	if (this->clicked != clicked) {
		this->clicked = clicked;
		emit ClickedStateChanged(clicked);
	}
}

void PLSColorFilterImage::SetPixMap(const QPixmap &pixmap)
{
	this->pixmap = pixmap;
	update();
}

bool PLSColorFilterImage::GetClickedState()
{
	return clicked;
}

void PLSColorFilterImage::SetClickedStyle(bool clicked)
{
	this->setProperty(STATUS_CLICKED, clicked);
	this->style()->unpolish(this);
	this->style()->polish(this);
}

void PLSColorFilterImage::mousePressEvent(QMouseEvent *event)
{
	emit MousePressd(this);
	QPushButton::mousePressEvent(event);
}

void PLSColorFilterImage::paintEvent(QPaintEvent *event)
{
	if (!pixmap.isNull()) {
		QPainter painter(this);
		painter.setRenderHints(QPainter::Antialiasing, true);
		painter.drawPixmap(this->rect(), pixmap.scaled(this->width(), this->height(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));

		if (!name.contains(ORIGINAL_PNG)) {
			QFont font;
			font.setPixelSize(COLOR_FILTER_TEXT_FONT_SIZE);
			font.setBold(true);
			painter.setFont(font);
			painter.setPen(QColor(COLOR_FILTER_TEXT_COLOR));

			int widthOfTitle = painter.fontMetrics().width(shortTitle);
			painter.drawText(this->width() / 2 - widthOfTitle / 2, COLOR_FILTER_TEXT_MARGIN_TOP, shortTitle);
		}
	}

	QPushButton::paintEvent(event);
}
