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
	connect(this, &PLSSceneDisplay::RenderChanged, this, &PLSSceneDisplay::OnRenderChanged);
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
		emit RenderChanged(state);
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

void PLSSceneDisplay::resizeEvent(QResizeEvent *event)
{
	obs_display_resize(GetDisplay(), event->size().width(), event->size().height());
}

void PLSSceneDisplay::OnRenderChanged(bool state)
{
	if (state) {
		AddRenderCallback();
	} else {
		RemoveRenderCallback();
	}
}

void PLSSceneDisplay::OnDisplayCreated()
{
	if (render) {
		AddRenderCallback();
	}
}

void PLSSceneDisplay::AddRenderCallback()
{
	obs_display_add_draw_callback(GetDisplay(), RenderScene, this);
	obs_display_set_background_color(GetDisplay(), 0x161616);

	OBSSource source = obs_scene_get_source(scene);
	if (source) {
		obs_source_inc_showing(source);
	}
	render = true;
}

void PLSSceneDisplay::RemoveRenderCallback()
{
	obs_display_remove_draw_callback(GetDisplay(), RenderScene, this);
	OBSSource source = obs_scene_get_source(scene);
	if (source) {
		obs_source_dec_showing(source);
	}
	render = false;
}

PLSSceneItemView::PLSSceneItemView(const QString &name_, OBSScene scene_, QWidget *parent) : name(name_), scene(scene_), QFrame(parent), ui(new Ui::PLSSceneItemView)
{
	ui->setupUi(this);
	this->setAcceptDrops(true);
	this->setAttribute(Qt::WA_DeleteOnClose);

	this->SetName(name_);
	ui->nameLabel->setToolTip(GetName());
	ui->nameLineEdit->hide();
	ui->nameLineEdit->installEventFilter(this);
	ui->nameLabel->installEventFilter(this);
	ui->verticalLayout->setContentsMargins(0, 0, 0, 0);

	ui->modifyBtn->hide();
	ui->imageLabel->hide();

	ui->display->hide();
	ui->display->SetSceneData(scene);

	deleteBtn = new QPushButton(ui->widget);
	deleteBtn->hide();
	deleteBtn->setObjectName(OBJECT_NMAE_DELETE_BUTTON);
	connect(this, &PLSSceneItemView::CurrentItemChanged, this, &PLSSceneItemView::OnCurrentItemChanged);
	connect(ui->modifyBtn, &QPushButton::clicked, this, &PLSSceneItemView::OnModifyButtonClicked);
	connect(deleteBtn, &QPushButton::clicked, this, &PLSSceneItemView::OnDeleteButtonClicked);
	connect(ui->display, &PLSSceneDisplay::MouseLeftButtonClicked, this, &PLSSceneItemView::OnMouseButtonClicked);
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
	ui->nameLabel->setText(GetNameElideString());
}

void PLSSceneItemView::SetRenderFlag(bool state)
{
	ui->display->SetRenderFlag(state);
	ui->image->setProperty(PROPERTY_NAME_SHOW_IMAGE, !state);
	RefreshUi(ui->image);
	if (!state) {
		ui->display->hide();
		ui->imageLabel->show();
	} else {
		ui->display->show();
		ui->imageLabel->hide();
	}
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

void PLSSceneItemView::OnRenameOperation()
{
	isFinishEditing = false;
	ui->nameLineEdit->setContentsMargins(0, 0, 0, 0);
	ui->nameLineEdit->setAlignment(Qt::AlignLeft);
	ui->nameLineEdit->setText(GetName());
	ui->nameLineEdit->setFocus();
	ui->nameLineEdit->selectAll();
	ui->nameLineEdit->show();
	ui->nameLabel->hide();
	ui->modifyBtn->hide();
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
	if (!startPos.isNull()) {
		int distance = (mapToParent(event->pos()) - startPos).manhattanLength();
		if (distance < QApplication::startDragDistance())
			return;
		CreateDrag(startPos);
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
	if (object == ui->nameLineEdit && LineEditCanceled(event)) {
		OnFinishingEditName();
		return true;
	}

	if (object == ui->nameLineEdit && LineEditChanged(event) && !isFinishEditing) {
		isFinishEditing = true;
		OnFinishingEditName();
		return true;
	}

	if (object == ui->nameLabel && event->type() == QEvent::Resize) {
		ui->nameLabel->setText(GetNameElideString());
		return true;
	}

	return QFrame::eventFilter(object, event);
}

void PLSSceneItemView::resizeEvent(QResizeEvent *event)
{
	ui->imageLabel->resize(ui->widget->size());
	QFrame::resizeEvent(event);
}

void PLSSceneItemView::enterEvent(QEvent *event)
{
	deleteBtn->setGeometry(this->width() - deleteBtn->width() - 4, 4, deleteBtn->width(), deleteBtn->height());
	deleteBtn->show();
	setEnterPropertyState(true, ui->editWidget);
	setEnterPropertyState(true, ui->nameLabel);

	if (isFinishEditing) {
		ui->modifyBtn->show();
		setEnterPropertyState(true, ui->modifyBtn);
	}
	setEnterPropertyState(true, ui->widget);
	PLS_ACTION_LOG(MAINFRAME_MODULE, QT_TO_UTF8(this->GetName()), "Mouse Enter");
	QFrame::enterEvent(event);
}

void PLSSceneItemView::leaveEvent(QEvent *event)
{
	deleteBtn->hide();
	ui->modifyBtn->hide();
	if (!this->current) {
		setEnterPropertyState(false, ui->editWidget);
		setEnterPropertyState(false, ui->modifyBtn);
		setEnterPropertyState(false, ui->nameLabel);
	}
	setEnterPropertyState(false, ui->widget);
	PLS_ACTION_LOG(MAINFRAME_MODULE, QT_TO_UTF8(this->GetName()), "Mouse Leave");
	QFrame::leaveEvent(event);
}

void PLSSceneItemView::OnMouseButtonClicked()
{
	emit MouseButtonClicked(this);
}

void PLSSceneItemView::OnModifyButtonClicked()
{
	QString controls = this->GetName().append(" Modify");
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

void PLSSceneItemView::OnMouseEnterEvent()
{
	ui->nameLabel->setText(GetNameElideString());
	deleteBtn->setGeometry(this->width() - deleteBtn->width() - 4, 4, deleteBtn->width(), deleteBtn->height());
	deleteBtn->show();
	setEnterPropertyState(true, ui->editWidget);
	setEnterPropertyState(true, ui->nameLabel);

	if (isFinishEditing) {
		ui->modifyBtn->show();
		setEnterPropertyState(true, ui->modifyBtn);
	}
	setEnterPropertyState(true, ui->widget);
	PLS_ACTION_LOG(MAINFRAME_MODULE, QT_TO_UTF8(this->GetName()), "Mouse Enter");
}

void PLSSceneItemView::OnMouseLeaveEvent()
{
	deleteBtn->hide();
	ui->modifyBtn->hide();
	if (!this->current) {
		setEnterPropertyState(false, ui->editWidget);
		setEnterPropertyState(false, ui->modifyBtn);
		setEnterPropertyState(false, ui->nameLabel);
	}
	setEnterPropertyState(false, ui->widget);
	PLS_ACTION_LOG(MAINFRAME_MODULE, QT_TO_UTF8(this->GetName()), "Mouse Leave");
}

void PLSSceneItemView::OnFinishingEditName()
{
	ui->nameLineEdit->hide();
	ui->nameLabel->show();
	ui->nameLabel->setText(GetNameElideString());
	emit FinishingEditName(ui->nameLineEdit->text(), this);
}

void PLSSceneItemView::setEnterPropertyState(bool state, QWidget *widget)
{
	widget->setProperty(STATUS_ENTER, state);
	RefreshUi(widget);
}

void PLSSceneItemView::CreateDrag(const QPoint &startPos)
{
	QMimeData *mimeData = new QMimeData;
	QString data =
		QString::number(this->width()).append(":").append(QString::number(this->height())).append(":").append(QString::number(startPos.x())).append(":").append(QString::number(startPos.y()));
	mimeData->setData(SCENE_DRAG_MIME_TYPE, QByteArray(data.toStdString().c_str()));

	QDrag *drag = new QDrag(this);
	drag->setMimeData(mimeData);

	QPixmap pixmap = QGuiApplication::primaryScreen()->grabWindow(ui->widget->winId());
	drag->setHotSpot(QPoint(pixmap.width() / 2, pixmap.height() / 2));
	drag->setPixmap(pixmap);
	drag->exec();
}

QString PLSSceneItemView::GetNameElideString()
{
	QFontMetrics fontWidth(ui->nameLabel->font());
	if (fontWidth.width(name) > ui->nameLabel->width())
		return fontWidth.elidedText(name, Qt::ElideRight, ui->nameLabel->width());
	return name;
}

void PLSSceneItemView::OnCurrentItemChanged(bool state)
{
	setEnterPropertyState(state, ui->editWidget);
	setEnterPropertyState(state, ui->modifyBtn);
	setEnterPropertyState(state, ui->nameLabel);

	if (!state) {
		ui->widget->setContentsMargins(0, 0, 0, 0);
	} else {
		ui->widget->setContentsMargins(2, 0, 2, 0);
	}

	ui->widget->setProperty(STATUS_CLICKED, state);
	RefreshUi(ui->widget);
}

void PLSSceneDisplay::RenderScene(void *data, uint32_t cx, uint32_t cy)
{
	PLSSceneDisplay *window = reinterpret_cast<PLSSceneDisplay *>(data);
	if (!window || !window->ready) {
		return;
	}

	if (window->current) {
		window->SetDisplayBackgroundColor(QColor(0, 0, 0));
	} else {
		window->SetDisplayBackgroundColor(QColor(22, 22, 22));
	}
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
}
