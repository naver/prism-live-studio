#include "PLSSceneItemView.h"
#include "ui_PLSSceneItemView.h"
#include "PLSScrollAreaContent.h"

#include "display-helpers.hpp"
#include "window-basic-main.hpp"
#include "qt-wrappers.hpp"
#include "liblog.h"
#include "action.h"
#include "log/module_names.h"
#include "pls-common-define.hpp"

#include <QMouseEvent>
#include <QWindow>
#include <QLabel>
#include <QVariant>
#include <QDebug>
#include <QStyle>
#include <QLineEdit>
#include <QMimeData>
#include <QDrag>
#include <QPixmap>
#include <QResizeEvent>
#include <QScreen>
#include <QPainter>
#include <QDir>
#include <ctime>
#include "obs.hpp"

static void RefreshUi(QWidget *widget)
{
	widget->style()->unpolish(widget);
	widget->style()->polish(widget);
}

static inline void startRegion(int vX, int vY, int vCX, int vCY, float oL, float oR, float oT, float oB)
{
	gs_projection_push();
	gs_viewport_push();
	gs_set_viewport(vX, vY, vCX, vCY);
	gs_ortho(oL, oR, oT, oB, -100.0f, 100.0f);
}

static inline void endRegion()
{
	gs_viewport_pop();
	gs_projection_pop();
}

PLSSceneDisplay::PLSSceneDisplay(QWidget *parent_) : PLSQTDisplay(parent_, Qt::Window)
{
	ready = true;
	displayMethod = static_cast<DisplayMethod>(config_get_int(App()->GlobalConfig(), "BasicWindow", "SceneDisplayMethod"));
	SetCreateImmediately(displayMethod == DisplayMethod::ThumbnailView);
	this->setAcceptDrops(true);
	this->setWindowFlags(Qt::FramelessWindowHint | Qt::X11BypassWindowManagerHint);

	this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	show();

	auto adjustResize = [this](QLabel *scr, QLabel *view, bool &handled) {
		QSize size = GetPixelSize(scr);
		view->setGeometry(0, 0, size.width(), size.height());
		handled = true;
	};

	connect(this, &PLSQTDisplay::DisplayCreated, this, &PLSSceneDisplay::OnDisplayCreated);
	connect(this, &PLSQTDisplay::AdjustResizeView, adjustResize);
}

PLSSceneDisplay::~PLSSceneDisplay()
{
	if (render)
		RemoveRenderCallback();
}

void PLSSceneDisplay::SetRenderFlag(bool state)
{
	if (state != render) {
		render = state;

		if (!render) {
			setVisible(false);
		} else if (toplevelVisible) {
			setVisible(true);
		}
	}
}

void PLSSceneDisplay::SetCurrentFlag(bool state)
{
	if (state != current) {
		current = state;
	}
}

void PLSSceneDisplay::SetSceneData(OBSScene scene)
{
	this->scene = scene;
}

void PLSSceneDisplay::SetDragingState(bool state)
{
	isDraging = state;
}

bool PLSSceneDisplay::GetRenderState() const
{
	return render;
}

void PLSSceneDisplay::visibleSlot(bool visible)
{
	toplevelVisible = visible;

	if (!visible) {
		setVisible(false);
	} else if (render) {
		setVisible(true);
	}
}

void PLSSceneDisplay::CustomCreateDisplay()
{
	CreateDisplay();
}

void PLSSceneDisplay::SetRefreshThumbnail(bool refresh)
{
	refreshThumbnail = refresh;
}

void PLSSceneDisplay::SetSceneDisplayMethod(DisplayMethod displayMethod)
{
	this->displayMethod = displayMethod;
}

void PLSSceneDisplay::mousePressEvent(QMouseEvent *event)
{
	if (event->buttons() & Qt::LeftButton) {
		if (render) {
			obs_display_set_background_color(GetDisplay(), 0x000000);
		}
		this->setFocus();
		startPos = mapToParent(event->pos());
		emit MouseLeftButtonClicked();
	}
	PLSQTDisplay::mousePressEvent(event);
}

void PLSSceneDisplay::enterEvent(QEvent *event)
{
	if (render) {
		obs_display_set_background_color(GetDisplay(), 0x000000);
	}
	PLSQTDisplay::enterEvent(event);
}

void PLSSceneDisplay::leaveEvent(QEvent *event)
{
	if (render) {
		obs_display_set_background_color(GetDisplay(), 0x161616);
	}

	PLSQTDisplay::leaveEvent(event);
}

void PLSSceneDisplay::resizeEvent(QResizeEvent *)
{
	double dpi = PLSDpiHelper::getDpi(this);
	int width = PLSDpiHelper::calculate(dpi, SCENE_DISPLAY_DEFAULT_WIDTH);
	int height = PLSDpiHelper::calculate(dpi, SCENE_DISPLAY_DEFAULT_HEIGHT);

	obs_display_resize(GetDisplay(), width, height);
}

void PLSSceneDisplay::OnDisplayCreated()
{
	AddRenderCallback();
}

void PLSSceneDisplay::CaptureImageFinished(const QImage &image)
{
	emit CaptureImageFinishedSignal(image);
}

void PLSSceneDisplay::CaptureSceneImageFinished(const QImage &image)
{
	emit CaptureSceneImageSignal(image);
}

void PLSSceneDisplay::AddRenderCallback()
{
	obs_display_t *display = GetDisplay();
	if (!display) {
		return;
	}

	if (display == curDisplay && displayAdd) {
		return;
	}
	curDisplay = display;

	obs_display_add_draw_callback(display, RenderScene, this);
	obs_display_set_background_color(display, 0x161616);

	OBSSource source = obs_scene_get_source(scene);
	if (source) {
		obs_source_inc_showing(source);
	}
	render = true;
	displayAdd = true;
}

void PLSSceneDisplay::RemoveRenderCallback()
{
	if (!displayAdd) {
		return;
	}
	obs_display_remove_draw_callback(curDisplay, RenderScene, this);
	OBSSource source = obs_scene_get_source(scene);
	if (source) {
		obs_source_dec_showing(source);
	}
	render = false;
	displayAdd = false;
}

PLSSceneItemView::PLSSceneItemView(const QString &name_, OBSScene scene_, DisplayMethod displayMethod, QWidget *parent) : name(name_), scene(scene_), QFrame(parent), ui(new Ui::PLSSceneItemView)
{
	ui->setupUi(this);
	this->setAcceptDrops(true);
	this->setAttribute(Qt::WA_DeleteOnClose);

	this->SetName(name_);
	ui->nameLabel->setToolTip(GetName());
	ui->nameLineEdit->hide();
	ui->nameLineEdit->installEventFilter(this);
	ui->lineEdit->installEventFilter(this);
	ui->label->installEventFilter(this);
	ui->nameLabel->installEventFilter(this);
	ui->modifyBtn->installEventFilter(this);
	ui->renameBtn->installEventFilter(this);
	ui->verticalLayout->setContentsMargins(0, 0, 0, 0);

	ui->lineEdit->hide();
	ui->modifyBtn->hide();
	ui->imageLabel->hide();
	ui->renameBtn->hide();

	ui->display->SetSceneData(scene);

	deleteBtn = new QPushButton(ui->widget);
	deleteBtn->hide();
	deleteBtn->setObjectName(OBJECT_NMAE_DELETE_BUTTON);

	SetSceneDisplayMethod(displayMethod);

	connect(this, &PLSSceneItemView::CurrentItemChanged, this, &PLSSceneItemView::OnCurrentItemChanged);
	connect(ui->modifyBtn, &QPushButton::clicked, this, &PLSSceneItemView::OnModifyButtonClicked);
	connect(ui->renameBtn, &QPushButton::clicked, this, &PLSSceneItemView::OnModifyButtonClicked);
	connect(deleteBtn, &QPushButton::clicked, this, &PLSSceneItemView::OnDeleteButtonClicked);
	connect(ui->display, &PLSSceneDisplay::MouseLeftButtonClicked, this, &PLSSceneItemView::OnMouseButtonClicked);
	connect(ui->display, &PLSSceneDisplay::CaptureImageFinishedSignal, this, &PLSSceneItemView::OnCaptureImageFinished);
	connect(ui->display, &PLSSceneDisplay::CaptureSceneImageSignal, this, &PLSSceneItemView::OnRefreshSceneThumbnail);
}

PLSSceneItemView::~PLSSceneItemView()
{
	delete ui;
}

void PLSSceneItemView::SetData(OBSScene scene_)
{
	scene = scene_;
}

void PLSSceneItemView::SetSignalHandler(SignalContainer<OBSScene> handler_)
{
	handler = handler_;
}

void PLSSceneItemView::SetCurrentFlag(bool state)
{
	if (current != state) {
		current = state;
		ui->display->SetCurrentFlag(state);
		emit CurrentItemChanged(state);
	}
}

void PLSSceneItemView::SetName(const QString &name_)
{
	name = name_;
	ui->nameLabel->setToolTip(name_);
	ui->label->setToolTip(name_);
	ui->nameLabel->setText(GetNameElideString());
	ui->label->setText(GetNameElideString());
}

void PLSSceneItemView::SetRenderFlag(bool state)
{
	ui->display->SetRenderFlag(state);

	if (!state && displayMethod == DisplayMethod::DynamicRealtimeView) {
		QSvgRenderer renderer(QString(":/images/img-scene-default.svg"));
		double dpi = PLSDpiHelper::getDpi(this);
		QPixmap imagePixmap(PLSDpiHelper::calculate(dpi, 112), PLSDpiHelper::calculate(dpi, 64));
		imagePixmap.fill(QColor("#222222"));
		QPainter imagePainter(&imagePixmap);
		renderer.render(&imagePainter);
		ui->image->setPixmap(imagePixmap);
	}

	if (state && displayMethod == DisplayMethod::ThumbnailView && ui->display->GetDisplay()) {
		ui->imageLabel->setVisible(true);
		ui->display->SetRefreshThumbnail(true);
		RefreshSceneThumbnail();
	} else {
		ui->imageLabel->setVisible(!state);
		ui->display->SetRefreshThumbnail(!state || displayMethod == DisplayMethod::ThumbnailView);
	}
}

void PLSSceneItemView::CustomCreateDisplay()
{
	ui->display->CustomCreateDisplay();
}

void PLSSceneItemView::SetSceneDisplayMethod(DisplayMethod displayMethod)
{
	ui->display->SetSceneDisplayMethod(displayMethod);
	ui->display->SetCreateImmediately(displayMethod == DisplayMethod::ThumbnailView);

	this->displayMethod = displayMethod;

	if (displayMethod == DisplayMethod::DynamicRealtimeView || displayMethod == DisplayMethod::ThumbnailView) {
		ui->listFrame->hide();
		ui->widget->show();
		ui->editWidget->show();
	} else {
		ui->widget->hide();
		ui->editWidget->hide();
		ui->listFrame->show();
	}
}

DisplayMethod PLSSceneItemView::GetSceneDisplayMethod()
{
	return displayMethod;
}

void PLSSceneItemView::ResizeCustom()
{
	ui->display->resize(QSize(PLSDpiHelper::calculate(this, SCENE_DISPLAY_DEFAULT_WIDTH), PLSDpiHelper::calculate(this, SCENE_DISPLAY_DEFAULT_HEIGHT)));
}

OBSScene PLSSceneItemView::GetData()
{
	return scene;
}

QString PLSSceneItemView::GetName()
{
	return name;
}

SignalContainer<OBSScene> PLSSceneItemView::GetSignalHandler()
{
	return handler;
}

bool PLSSceneItemView::GetCurrentFlag()
{
	return current;
}

void PLSSceneItemView::RefreshSceneThumbnail()
{
	if (displayMethod == DisplayMethod::ThumbnailView) {
		ui->display->setVisible(!ui->display->GetDisplay());
		ui->imageLabel->setVisible(true);
		ui->display->SetRefreshThumbnail(true);
		ui->display->AddRenderCallback();
	} else if (displayMethod == DisplayMethod::DynamicRealtimeView) {
		ui->imageLabel->setVisible(false);
		ui->display->SetRefreshThumbnail(false);
		ui->display->setVisible(true);
		ui->display->AddRenderCallback();
	} else if (displayMethod == DisplayMethod::TextView) {
		ui->display->RemoveRenderCallback();
	}
}

void PLSSceneItemView::OnRenameOperation()
{
	if (ui->nameLineEdit->isVisible() || ui->lineEdit->isVisible()) {
		return;
	}

	isFinishEditing = false;
	bool gridMode = (displayMethod == DisplayMethod::DynamicRealtimeView || displayMethod == DisplayMethod::ThumbnailView);
	gridMode ? RenameWithGridMode() : RenameWithListMode();
}

void PLSSceneItemView::mousePressEvent(QMouseEvent *event)
{
	if (event->buttons() & Qt::LeftButton) {
		startPos = mapToParent(event->pos());
		PLS_UI_STEP(MAINSCENE_MODULE, QT_TO_UTF8(this->GetName()), ACTION_LBUTTON_CLICK);

	} else if (event->buttons() & Qt::RightButton) {
		PLS_UI_STEP(MAINSCENE_MODULE, QT_TO_UTF8(this->GetName()), ACTION_RBUTTON_CLICK);
	}
	OnMouseButtonClicked();

	QFrame::mousePressEvent(event);
}

void PLSSceneItemView::mouseMoveEvent(QMouseEvent *event)
{
	QFrame::mouseMoveEvent(event);
	if (!startPos.isNull() && (event->buttons() & Qt::LeftButton)) {
		int distance = (mapToParent(event->pos()) - startPos).manhattanLength();
		if (distance < QApplication::startDragDistance())
			return;

		if (ui->display->GetRenderState() && displayMethod == DisplayMethod::DynamicRealtimeView) {
			ui->display->SetDragingState(true);
		} else {
			CreateDrag(startPos, QImage());
		}
	}
}

void PLSSceneItemView::mouseDoubleClickEvent(QMouseEvent *event)
{
	PLSBasic *main = PLSBasic::Get();
	if (main) {
		main->OnScenesItemDoubleClicked();
	}

	QFrame::mouseDoubleClickEvent(event);
}

bool PLSSceneItemView::eventFilter(QObject *object, QEvent *event)
{
	if ((object == ui->nameLineEdit || object == ui->lineEdit) && LineEditCanceled(event)) {
		isFinishEditing = true;
		OnFinishingEditName(true);
		return true;
	}

	if ((object == ui->nameLineEdit || object == ui->lineEdit) && LineEditChanged(event) && !isFinishEditing) {
		isFinishEditing = true;
		OnFinishingEditName(false);
		return true;
	}

	if ((object == ui->nameLabel || object == ui->label) && event->type() == QEvent::Resize) {
		QMetaObject::invokeMethod(
			this,
			[=]() {
				ui->nameLabel->setText(GetNameElideString());
				ui->label->setText(GetNameElideString());
			},
			Qt::QueuedConnection);
		return true;
	}

	if ((object == ui->nameLabel || object == ui->modifyBtn || object == ui->renameBtn) && (event->type() == QEvent::MouseMove)) {
		return true;
	}
	return QFrame::eventFilter(object, event);
}

void PLSSceneItemView::resizeEvent(QResizeEvent *event)
{
	ui->imageLabel->resize(ui->widget->size());
	ui->label->resize(QSize(size().width() - PLSDpiHelper::calculate(this, 40), size().height()));
	ui->display->resize(QSize(PLSDpiHelper::calculate(this, SCENE_DISPLAY_DEFAULT_WIDTH), PLSDpiHelper::calculate(this, SCENE_DISPLAY_DEFAULT_HEIGHT)));
	QFrame::resizeEvent(event);
}

void PLSSceneItemView::enterEvent(QEvent *event)
{
	bool gridMode = (displayMethod == DisplayMethod::DynamicRealtimeView || displayMethod == DisplayMethod::ThumbnailView);
	gridMode ? EnterEventWithGridMode() : EnterEventWithListMode();

	PLS_ACTION_LOG(MAINFRAME_MODULE, QT_TO_UTF8(this->GetName()), "Mouse Enter");
	QFrame::enterEvent(event);
}

void PLSSceneItemView::leaveEvent(QEvent *event)
{
	bool gridMode = (displayMethod == DisplayMethod::DynamicRealtimeView || displayMethod == DisplayMethod::ThumbnailView);
	gridMode ? LeaveEventWithGridMode() : LeaveEventWithListMode();

	PLS_ACTION_LOG(MAINFRAME_MODULE, QT_TO_UTF8(this->GetName()), "Mouse Leave");
	QFrame::leaveEvent(event);
}

void PLSSceneItemView::OnMouseButtonClicked()
{
	emit MouseButtonClicked(this);
}

void PLSSceneItemView::OnModifyButtonClicked()
{
	QString controls = this->GetName().append(" Rename");
	PLS_UI_STEP(MAINSCENE_MODULE, controls.toStdString().c_str(), ACTION_LBUTTON_CLICK);
	OnRenameOperation();
	emit ModifyButtonClicked(this);
}

void PLSSceneItemView::OnDeleteButtonClicked()
{
	QString controls = this->GetName().append(" Delete");
	PLS_UI_STEP(MAINSCENE_MODULE, QT_TO_UTF8(controls), ACTION_LBUTTON_CLICK);
	emit DeleteButtonClicked(this);
}

void PLSSceneItemView::OnFinishingEditName(bool cancel)
{
	bool gridMode = (displayMethod == DisplayMethod::DynamicRealtimeView || displayMethod == DisplayMethod::ThumbnailView);
	if (this->rect().contains(mapFromGlobal(QCursor::pos()))) {
		gridMode ? EnterEventWithGridMode() : EnterEventWithListMode();
	}
	if (gridMode) {
		ui->nameLineEdit->hide();
		ui->nameLabel->show();
		ui->nameLabel->setText(GetNameElideString());
		if (!cancel)
			emit FinishingEditName(ui->nameLineEdit->text(), this);
	} else {
		ui->lineEdit->hide();
		ui->label->show();
		ui->label->setText(GetNameElideString());
		if (!cancel)
			emit FinishingEditName(ui->lineEdit->text(), this);
	}
}
void PLSSceneItemView::setEnterPropertyState(bool state, QWidget *widget)
{
	widget->setProperty(STATUS_ENTER, state);
	RefreshUi(widget);
}

static QImage CaptureImage(uint32_t width, uint32_t height, enum gs_color_format format, uint8_t **data, uint32_t *linesize, QString sceneName)
{
	if (format != gs_color_format::GS_BGRA || sceneName.isEmpty()) {
		return QImage();
	}

	uchar *buffer = (uchar *)malloc(width * height * 4);
	if (!buffer) {
		return QImage();
	}

	memset(buffer, 0, width * height * 4);

	if (width * 4 == linesize[0]) {
		memmove(buffer, data[0], linesize[0] * height);
	} else {
		for (uint32_t i = 0; i < height; i++) {
			memmove(buffer + i * width * 4, data[0] + i * linesize[0], width * 4);
		}
	}

	QImage image(buffer, width, height, QImage::Format_ARGB32);
	QImage imageTarget = image.copy();

	free(buffer);
	return imageTarget;
}

void PLSSceneItemView::CreateDrag(const QPoint &startPos, const QImage &image)
{
	QMimeData *mimeData = new QMimeData;
	QString data =
		QString::number(this->width()).append(":").append(QString::number(this->height())).append(":").append(QString::number(startPos.x())).append(":").append(QString::number(startPos.y()));
	mimeData->setData(SCENE_DRAG_MIME_TYPE, QByteArray::fromStdString(data.toStdString()));
	mimeData->setData(SCENE_DRAG_GRID_MODE, QByteArray::fromStdString(QString::number(static_cast<int>(displayMethod)).toStdString()));

	QDrag *drag = new QDrag(this);
	drag->setMimeData(mimeData);
	bool gridMode = (displayMethod == DisplayMethod::DynamicRealtimeView || displayMethod == DisplayMethod::ThumbnailView);

	QPixmap pixmap(ui->widget->width(), ui->widget->height());
	if (!image.isNull()) {
		QPainter painter(&pixmap);

		PLSDpiHelper dpiHelper;
		painter.fillRect(ui->widget->rect(), QColor("#333333"));
		int topMargin = dpiHelper.calculate(this, 24);
		int borderWidth = dpiHelper.calculate(this, 4);
		painter.drawImage(QRect(0, topMargin, ui->widget->width(), ui->widget->height() - 2 * topMargin), image);

		QSvgRenderer renderer(QString(":/images/btn-close-normal.svg"));
		QPixmap delPixmap(deleteBtn->width(), deleteBtn->height());
		delPixmap.fill(Qt::transparent);
		QPainter delPainter(&delPixmap);
		renderer.render(&delPainter);

		int margin = PLSDpiHelper::calculate(this, 4);
		painter.drawPixmap(QRect(this->width() - deleteBtn->width() - margin, margin, deleteBtn->width(), deleteBtn->height()), delPixmap);

		QPen pen(QColor("#effc35"));
		pen.setWidth(borderWidth);
		painter.setPen(pen);
		painter.drawRect(ui->widget->rect());
	} else {
		pixmap = this->grab(gridMode ? ui->widget->rect() : this->rect());
	}

	drag->setHotSpot(QPoint(gridMode ? pixmap.width() / 2 : startPos.x(), pixmap.height() / 2));
	drag->setPixmap(pixmap);
	drag->exec();
}

QString PLSSceneItemView::GetNameElideString()
{
	bool gridMode = (displayMethod == DisplayMethod::DynamicRealtimeView || displayMethod == DisplayMethod::ThumbnailView);

	QLabel *label = nullptr;
	gridMode ? label = ui->nameLabel : label = ui->label;

	QFontMetrics fontWidth(label->font());
	if (fontWidth.width(name) > label->width())
		return fontWidth.elidedText(name, Qt::ElideRight, label->width());
	return name;
}

void PLSSceneItemView::RenameWithGridMode()
{
	ui->nameLineEdit->setContentsMargins(0, 0, 0, 0);
	ui->nameLineEdit->setAlignment(Qt::AlignLeft);
	ui->nameLineEdit->setText(GetName());
	ui->nameLineEdit->setFocus();
	ui->nameLineEdit->selectAll();
	ui->nameLineEdit->show();
	ui->nameLabel->hide();
	ui->modifyBtn->hide();
}

void PLSSceneItemView::EnterEventWithGridMode()
{
	int margin = PLSDpiHelper::calculate(this, 4);
	deleteBtn->setGeometry(this->width() - deleteBtn->width() - margin, margin, deleteBtn->width(), deleteBtn->height());
	deleteBtn->show();
	setEnterPropertyState(true, ui->editWidget);
	setEnterPropertyState(true, ui->nameLabel);

	if (isFinishEditing) {
		ui->modifyBtn->show();
		setEnterPropertyState(true, ui->modifyBtn);
	}
	setEnterPropertyState(true, ui->widget);
}

void PLSSceneItemView::LeaveEventWithGridMode()
{
	deleteBtn->hide();
	ui->modifyBtn->hide();
	if (!this->current) {
		setEnterPropertyState(false, ui->editWidget);
		setEnterPropertyState(false, ui->modifyBtn);
		setEnterPropertyState(false, ui->nameLabel);
	}
	setEnterPropertyState(false, ui->widget);
}

void PLSSceneItemView::RenameWithListMode()
{
	setEnterPropertyState(false, ui->listFrame);
	ui->lineEdit->setText(GetName());
	ui->lineEdit->setFocus();
	ui->lineEdit->selectAll();
	ui->lineEdit->show();
	ui->label->hide();
	ui->renameBtn->hide();
}

void PLSSceneItemView::EnterEventWithListMode()
{
	if (isFinishEditing) {
		ui->renameBtn->show();
		setEnterPropertyState(true, ui->renameBtn);
		setEnterPropertyState(true, ui->listFrame);
	}
}

void PLSSceneItemView::LeaveEventWithListMode()
{
	setEnterPropertyState(false, ui->listFrame);
	setEnterPropertyState(false, ui->renameBtn);
	ui->renameBtn->hide();
}

void PLSSceneItemView::SetContentMargins(bool state)
{
	double dpi = PLSDpiHelper::getDpi(this);

	if (!state) {
		ui->widget->setContentsMargins(0, 0, 0, 0);
	} else {
		ui->widget->setContentsMargins(PLSDpiHelper::calculate(dpi, 2), 0, PLSDpiHelper::calculate(dpi, 2), 0);
	}

	ui->widget->setProperty(STATUS_CLICKED, state);
	RefreshUi(ui->widget);
	ui->label->setText(GetNameElideString());
}

void PLSSceneItemView::OnCaptureImageFinished(const QImage &image)
{
	CreateDrag(startPos, image);
}

void PLSSceneItemView::OnRefreshSceneThumbnail(const QImage &image)
{
	if (image.isNull()) {
		return;
	}
	ui->image->setFixedWidth(ui->imageLabel->width());
	ui->image->setPixmap(QPixmap::fromImage(image));
	ui->imageLabel->setVisible(true);
}

void PLSSceneItemView::OnCurrentItemChanged(bool state)
{
	setEnterPropertyState(state, ui->editWidget);
	setEnterPropertyState(state, ui->modifyBtn);
	setEnterPropertyState(state, ui->nameLabel);
	pls_flush_style(ui->label, STATUS_CLICKED, state);

	QMetaObject::invokeMethod(this, "SetContentMargins", Qt::QueuedConnection, Q_ARG(bool, state));
}

static QImage CaptureSceneImage(QString sceneName)
{
	uint32_t width, height;
	enum gs_color_format format;
	texture_map_info tmi;
	QImage image;
	if (auto ss = gs_device_canvas_map(&width, &height, &format, tmi.data, tmi.linesize); ss) {
		image = CaptureImage(width, height, format, tmi.data, tmi.linesize, sceneName);
		gs_device_canvas_unmap(ss);
	}
	return image;
}

void PLSSceneDisplay::RenderScene(void *data, uint32_t cx, uint32_t cy)
{
	PLSSceneDisplay *window = reinterpret_cast<PLSSceneDisplay *>(data);
	if (!window || !window->ready || !window->render) {
		return;
	}

	if (!window->refreshThumbnail && window->displayMethod == DisplayMethod::ThumbnailView) {
		return;
	}

	if (window->current) {
		window->SetDisplayBackgroundColor(QColor(0, 0, 0));
	} else {
		window->SetDisplayBackgroundColor(QColor(22, 22, 22));
	}

	if (!obs_get_system_initialized())
		return;

	OBSSource source = obs_scene_get_source(window->scene);

	uint32_t targetCX;
	uint32_t targetCY;
	int x, y;
	int newCX, newCY;
	float scale;

	if (source) {
		targetCX = std::max(obs_source_get_width(source), 1u);
		targetCY = std::max(obs_source_get_height(source), 1u);
	} else {
		struct obs_video_info ovi;
		obs_get_video_info(&ovi);
		targetCX = ovi.base_width;
		targetCY = ovi.base_height;
	}

	GetScaleAndCenterPos(targetCX, targetCY, cx, cy, x, y, scale);

	newCX = int(scale * float(targetCX));
	newCY = int(scale * float(targetCY));

	startRegion(x, y, newCX, newCY, 0.0f, float(targetCX), 0.0f, float(targetCY));

	if (source) {
		obs_source_video_render(source);
	}

	endRegion();

	if (window->isDraging) {
		QImage image = CaptureSceneImage(obs_source_get_name(source));
		if (!image.isNull()) {
			window->isDraging = false;
			QMetaObject::invokeMethod(window, "CaptureImageFinished", Qt::QueuedConnection, Q_ARG(const QImage &, image));
		}
	} else if (window->displayMethod == DisplayMethod::ThumbnailView) {
		QImage image = CaptureSceneImage(obs_source_get_name(source));
		if (!image.isNull()) {
			window->refreshThumbnail = false;
			QMetaObject::invokeMethod(window, "CaptureSceneImageFinished", Qt::QueuedConnection, Q_ARG(const QImage &, image));
		}
	}
}
