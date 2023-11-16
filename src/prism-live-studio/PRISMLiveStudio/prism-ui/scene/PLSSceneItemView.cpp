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
#include "platform.hpp"
#include "libutils-api.h"
#include <pls/pls-graphics-api.h>
#include <pls/pls-obs-api.h>
#include "PLSChannelDataAPI.h"
#include "PLSSceneDataMgr.h"

#ifdef Q_OS_WIN
#include <Windows.h>
#endif // f Q_OS_WIN

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
#include <vector>
#include "obs.hpp"
using namespace common;
#define AUTO_LOCKER std::lock_guard<std::mutex> locker(mutex);
constexpr auto BADGE_TYPE = "badge_type";

extern QString getTempImageFilePath(const QString &suffix);

// border clor
static const float border_color_r = 239.f / 255.f;
static const float border_color_g = 252.f / 255.f;
static const float border_color_b = 53.f / 255.f;
static const float border_color_a = 1.0f;

// background normal color
static const float bg_color_normal_r = 17.f / 255.f;
static const float bg_color_normal_g = 17.f / 255.f;
static const float bg_color_normal_b = 17.f / 255.f;
static const float bg_color_normal_a = 1.0f;

// background hover color
static const float bg_color_hover_r = 0.f / 255.f;
static const float bg_color_hover_g = 0.f / 255.f;
static const float bg_color_hover_b = 0.f / 255.f;
static const float bg_color_hover_a = 1.0f;

static const double border_width = 2.0f;

texture_info PLSSceneDisplay::tex_del_btn_normal = texture_info();
texture_info PLSSceneDisplay::tex_del_btn_hover = texture_info();
texture_info PLSSceneDisplay::tex_del_btn_pressed = texture_info();
texture_info PLSSceneDisplay::tex_del_btn_disable = texture_info();
texture_info PLSSceneDisplay::radius_texture = texture_info();
gs_texture_t *PLSSceneDisplay::default_texture = nullptr;

texture_info PLSSceneDisplay::tex_edit_info = texture_info();
texture_info PLSSceneDisplay::tex_live_info = texture_info();
texture_info PLSSceneDisplay::tex_record_info = texture_info();
texture_info PLSSceneDisplay::tex_normal_info = texture_info();

int PLSSceneDisplay::sceneDisplayCount = 0;

static void RefreshUi(QWidget *widget)
{
	widget->style()->unpolish(widget);
	widget->style()->polish(widget);
}

PLSSceneDisplay::PLSSceneDisplay(QWidget *parent_) : OBSQTDisplay(parent_, Qt::Window)
{
	sceneDisplayCount++;
	displayMethod = static_cast<DisplayMethod>(config_get_int(App()->GlobalConfig(), "BasicWindow", "SceneDisplayMethod"));
	this->setAcceptDrops(true);
	this->setWindowFlags(Qt::FramelessWindowHint | Qt::X11BypassWindowManagerHint);

	this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	show();

	auto adjustResize = [](QLabel *scr, QLabel *view, bool &handled) {
		QSize size = GetPixelSize(scr);
		view->setGeometry(0, 0, size.width(), size.height());
		handled = true;
	};

	connect(this, &OBSQTDisplay::DisplayCreated, this, &PLSSceneDisplay::OnDisplayCreated);
	connect(this, &OBSQTDisplay::AdjustResizeView, adjustResize);

	setMouseTracking(true);
}

static void destory_texture(gs_texture_t **texture)
{
	if (texture && *texture) {
		gs_texture_destroy(*texture);
		*texture = nullptr;
	}
}

PLSSceneDisplay::~PLSSceneDisplay()
{
	if (sceneDisplayCount - 1 == 0) {
		obs_enter_graphics();
		destory_texture(&radius_texture.texture);
		destory_texture(&tex_del_btn_normal.texture);
		destory_texture(&tex_del_btn_hover.texture);
		destory_texture(&tex_del_btn_pressed.texture);
		destory_texture(&tex_del_btn_disable.texture);
		destory_texture(&default_texture);

		destory_texture(&tex_edit_info.texture);
		destory_texture(&tex_live_info.texture);
		destory_texture(&tex_normal_info.texture);
		destory_texture(&tex_record_info.texture);

		obs_leave_graphics();
	}
	sceneDisplayCount--;
	RemoveRenderCallback();
	obs_enter_graphics();
	destory_texture(&item_texture);
	obs_leave_graphics();
}

void PLSSceneDisplay::SetRenderFlag(bool state)
{
	AUTO_LOCKER
	if (state != render) {
		render = state;
	}
}

void PLSSceneDisplay::SetCurrentFlag(bool state)
{
	AUTO_LOCKER
	if (state != current) {
		current = state;
	}
}

bool PLSSceneDisplay::GetCurrentFlag()
{
	AUTO_LOCKER
	return current;
}

void PLSSceneDisplay::SetSceneData(OBSScene _scene)
{
	scene = _scene;
}

void PLSSceneDisplay::SetDragingState(bool state)
{
	AUTO_LOCKER
	isDraging = state;
}

bool PLSSceneDisplay::GetDragingState()
{
	AUTO_LOCKER
	return isDraging;
}

bool PLSSceneDisplay::GetRenderState()
{
	AUTO_LOCKER
	return render;
}

void PLSSceneDisplay::visibleSlot(bool visible)
{
	toplevelVisible = visible;

	if (!TestSceneDisplayMethod(DisplayMethod::TextView))
		setVisible(visible);
}

void PLSSceneDisplay::CustomCreateDisplay()
{
#ifdef Q_OS_WIN
	CreateDisplay();

#else
	bool textMode = displayMethod == DisplayMethod::TextView;
	if (textMode) {
		return;
	}
	auto display = GetDisplay();
	if (!display) {
		QTimer::singleShot(0, this, [this]() { CreateDisplay(true); });
	}

	ResizeDisplay();
#endif // Q_OS_WINDOW
}

void PLSSceneDisplay::SetRefreshThumbnail(bool refresh)
{
	AUTO_LOCKER
	refreshThumbnail = refresh;
}

bool PLSSceneDisplay::GetRefreshThumbnail()
{
	AUTO_LOCKER
	return refreshThumbnail;
}

void PLSSceneDisplay::SetSceneDisplayMethod(DisplayMethod _displayMethod)
{
	AUTO_LOCKER
	displayMethod = _displayMethod;
}

bool PLSSceneDisplay::TestSceneDisplayMethod(DisplayMethod method)
{
	AUTO_LOCKER
	return (displayMethod == method);
}

void PLSSceneDisplay::mousePressEvent(QMouseEvent *event)
{
	this->setFocus();
	startPos = mapToParent(event->pos());
	if (rect_delete_btn.contains(event->pos())) {
		if (event->buttons() & Qt::LeftButton) {
			UpdateMouseState(MouseState::Pressed);
		}
	} else {
		emit MouseLeftButtonClicked();
	}
	OBSQTDisplay::mousePressEvent(event);
}

void PLSSceneDisplay::mouseReleaseEvent(QMouseEvent *event)
{
	if (event->button() == Qt::LeftButton && rect_delete_btn.contains(event->pos())) {
		UpdateMouseState(MouseState::Hover);
		emit DeleteBtnClicked();
	}
	OBSQTDisplay::mouseReleaseEvent(event);
}

void PLSSceneDisplay::enterEvent(QEnterEvent *event)
{
	obs_display_set_background_color(GetDisplay(), 0x333333);
	isHover.store(true);
	UpdateMouseState(MouseState::Normal);
	AUTO_LOCKER
	btn_visible = true;
	OBSQTDisplay::enterEvent(event);
}

void PLSSceneDisplay::leaveEvent(QEvent *event)
{
	obs_display_set_background_color(GetDisplay(), 0x222222);
	isHover.store(false);
	UpdateMouseState(MouseState::None);
	AUTO_LOCKER
	btn_visible = false;
	OBSQTDisplay::leaveEvent(event);
}

void PLSSceneDisplay::resizeEvent(QResizeEvent *)
{
	auto display = GetDisplay();
	if (!display) {
		CustomCreateDisplay();
	}
	int width = SCENE_DISPLAY_DEFAULT_WIDTH * devicePixelRatioF();
	int height = SCENE_DISPLAY_DEFAULT_HEIGHT * devicePixelRatioF();

	obs_display_resize(GetDisplay(), width, height);

	qreal margin = 4 * devicePixelRatioF();
	qreal btn_width = 16 * devicePixelRatioF();
	qreal btn_height = btn_width;

	rect_delete_btn.setX((qreal)this->width() - btn_width - margin);
	rect_delete_btn.setY(margin);
	rect_delete_btn.setWidth(btn_width);
	rect_delete_btn.setHeight(btn_height);
}

void PLSSceneDisplay::mouseMoveEvent(QMouseEvent *event)
{
	auto state = rect_delete_btn.contains(event->pos()) ? MouseState::Hover : MouseState::Normal;
	UpdateMouseState(state);
	OBSQTDisplay::mouseMoveEvent(event);
}

void PLSSceneDisplay::changeEvent(QEvent *event)
{
	if (event->type() == QEvent::EnabledChange) {
		if (!isEnabled()) {
			UpdateMouseState(MouseState::Disabled);
		}
	}
	OBSQTDisplay::changeEvent(event);
}

void PLSSceneDisplay::OnDisplayCreated()
{
	AddRenderCallback();
}

void PLSSceneDisplay::CaptureImageFinished(const QImage &image)
{
	emit CaptureImageFinishedSignal(image);
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
	obs_display_set_background_color(display, 0x222222);

	OBSSource source = obs_scene_get_source(scene);
	if (source) {
		obs_source_inc_showing(source);
	}

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

	displayAdd = false;
}

void PLSSceneDisplay::DrawOverlay()
{
	DrawSelectedBorder();
	DrawRadiusOverlay();
	DrawDeleteBtn();
	DrawBadgeIcons();
}

void PLSSceneDisplay::DrawRadiusOverlay() const
{
	GetRadiusTexture();
	gs_projection_push();
	gs_matrix_push();
	gs_matrix_identity();
	gs_blend_state_push();
	gs_blend_function(GS_BLEND_SRCALPHA, GS_BLEND_INVSRCALPHA);

	uint32_t tex_w = gs_texture_get_width(radius_texture.texture);
	uint32_t tex_h = gs_texture_get_height(radius_texture.texture);

	gs_rect rt = {0};
	gs_get_viewport(&rt);
	gs_ortho(0.0f, (float)tex_w, 0.0f, (float)tex_h, -100.0f, 100.0f);

	const gs_effect_t *effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);
	gs_technique_t *tech = gs_effect_get_technique(effect, "Draw");

	gs_technique_begin(tech);
	gs_technique_begin_pass(tech, 0);

	gs_effect_set_texture(gs_effect_get_param_by_name(effect, "image"), radius_texture.texture);
	gs_draw_sprite(radius_texture.texture, 0, 0, 0);

	gs_technique_end_pass(tech);
	gs_technique_end(tech);

	gs_blend_state_pop();
	gs_projection_pop();
	gs_matrix_pop();
}

void drawTexture(gs_texture_t *texture)
{
	if (!texture)
		return;

	uint32_t tex_w = gs_texture_get_width(texture);
	uint32_t tex_h = gs_texture_get_height(texture);

	gs_ortho(0.0f, (float)tex_w, 0.0f, (float)tex_h, -100.0f, 100.0f);

	const gs_effect_t *effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);
	gs_technique_t *tech = gs_effect_get_technique(effect, "Draw");

	gs_technique_begin(tech);
	gs_technique_begin_pass(tech, 0);

	gs_effect_set_texture(gs_effect_get_param_by_name(effect, "image"), texture);
	gs_draw_sprite(texture, 0, 0, 0);

	gs_technique_end_pass(tech);
	gs_technique_end(tech);
}

void PLSSceneDisplay::DrawDeleteBtn()
{
	gs_rect rt = {0};
	gs_get_viewport(&rt);

	gs_projection_push();
	gs_viewport_push();
	auto btn_width = static_cast<int>(16 * devicePixelRatioF());
	auto btn_height = btn_width;
	auto margin = static_cast<int>(4 * devicePixelRatioF());
	gs_set_viewport(rt.cx - btn_width - margin, margin, btn_width, btn_height);
	gs_matrix_push();
	gs_matrix_identity();
	gs_blend_state_push();
	gs_blend_function(GS_BLEND_SRCALPHA, GS_BLEND_INVSRCALPHA);

	drawTexture(GetBtnTexture());

	gs_blend_state_pop();
	gs_projection_pop();
	gs_viewport_pop();
	gs_matrix_pop();
}

static void DrawLine(float x1, float y1, float x2, float y2, float scaled_thickness)
{
	float ySide = 0.0f;
	if (y1 == y2) {
		ySide = (y1 < 0.5f ? 1.0f : -1.0f);
	}
	float xSide = 0.0f;
	if (x1 == x2) {
		xSide = (x1 < 0.5f ? 1.0f : -1.0f);
	}

	gs_render_start(true);

	gs_vertex2f(x1, y1);
	gs_vertex2f(x1 + (xSide * scaled_thickness), y1 + (ySide * scaled_thickness));
	gs_vertex2f(x2 + (xSide * scaled_thickness), y2 + (ySide * scaled_thickness));
	gs_vertex2f(x2, y2);
	gs_vertex2f(x1, y1);

	gs_vertbuffer_t *line = gs_render_save();

	gs_load_vertexbuffer(line);
	gs_draw(GS_TRISTRIP, 0, 0);
	gs_load_vertexbuffer(nullptr);
	gs_vertexbuffer_destroy(line);
	line = nullptr;
}

void PLSSceneDisplay::DrawSelectedBorder()
{
	if (!GetCurrentFlag())
		return;

	gs_viewport_push();
	gs_projection_push();

	gs_rect rt = {0};
	gs_get_viewport(&rt);

	gs_enable_depth_test(false);

	const gs_effect_t *solid = obs_get_base_effect(OBS_EFFECT_SOLID);
	gs_eparam_t *color = gs_effect_get_param_by_name(solid, "color");
	gs_technique_t *tech = gs_effect_get_technique(solid, "Solid");

	struct vec4 bg_color;
	vec4_set(&bg_color, border_color_r, border_color_g, border_color_b, border_color_a);

	gs_effect_set_vec4(color, &bg_color);

	gs_technique_begin(tech);
	gs_technique_begin_pass(tech, 0);

	gs_matrix_push();
	gs_matrix_identity();
	gs_matrix_scale3f(float(rt.cx), float(rt.cy), 1.0f);
	gs_ortho((float)0.0, (float)rt.cx, 0.0f, (float)rt.cy, -100.0f, 100.0f);

	auto thickness = (float)std::ceil(border_width * devicePixelRatioF()) / (float)rt.cx;
	DrawLine(0.0f, 0.0f, 1.0f, 0.0f, thickness);
	DrawLine(1.0f, 0.0f, 1.0f, 1.0f, thickness);
	DrawLine(1.0f, 1.0f, 0.0f, 1.0f, thickness);
	DrawLine(0.0f, 1.0f, 0.0f, 0.0f, thickness);

	gs_technique_end_pass(tech);
	gs_technique_end(tech);

	gs_matrix_pop();
	gs_projection_pop();
	gs_viewport_pop();
}

void PLSSceneDisplay::DrawBadgeIcons()
{
	gs_rect rt = {0};
	gs_get_viewport(&rt);

	gs_projection_push();
	gs_viewport_push();
	auto icon_width = static_cast<int>(31 * devicePixelRatioF());
	auto icon_height = static_cast<int>(16 * devicePixelRatioF());
	auto margin = static_cast<int>(4 * devicePixelRatioF());
	gs_set_viewport(margin, margin, icon_width, icon_height);
	gs_matrix_push();
	gs_matrix_identity();
	gs_blend_state_push();
	gs_blend_function(GS_BLEND_SRCALPHA, GS_BLEND_INVSRCALPHA);

	drawTexture(GetBadgeTexture());

	gs_blend_state_pop();
	gs_projection_pop();
	gs_viewport_pop();
	gs_matrix_pop();
}

void PLSSceneDisplay::DrawSceneBackground() const
{
	const gs_effect_t *solid = obs_get_base_effect(OBS_EFFECT_SOLID);
	gs_eparam_t *color = gs_effect_get_param_by_name(solid, "color");
	gs_technique_t *tech = gs_effect_get_technique(solid, "Solid");

	struct vec4 bg_color;
	if (isHover.load()) {
		vec4_set(&bg_color, bg_color_hover_r, bg_color_hover_g, bg_color_hover_b, bg_color_hover_a);
	} else {
		vec4_set(&bg_color, bg_color_normal_r, bg_color_normal_g, bg_color_normal_b, bg_color_normal_a);
	}

	gs_effect_set_vec4(color, &bg_color);

	gs_rect rt;
	gs_get_viewport(&rt);

	gs_matrix_push();
	gs_matrix_identity();
	gs_matrix_scale3f(float(rt.cx), float(rt.cy), 1.0f);
	gs_ortho((float)0.0, (float)rt.cx, 0.0f, (float)rt.cy, -100.0f, 100.0f);

	gs_technique_begin(tech);
	gs_technique_begin_pass(tech, 0);

	DrawLine(0.0f, 0.0f, 1.0f, 0.0f, 1.0f);

	gs_technique_end_pass(tech);
	gs_technique_end(tech);
	gs_matrix_pop();
}

void createTexure(gs_texture_t **tex, const QString file)
{
	std::string path;
	GetDataFilePath(file.toStdString().c_str(), path);
	if (*tex) {
		obs_enter_graphics();
		destory_texture(tex);
		obs_leave_graphics();
	}
	*tex = gs_texture_create_from_file(path.c_str());
}

gs_texture_t *PLSSceneDisplay::GetBtnTexture()
{
	switch (GetMouseState()) {
	case MouseState::Normal:
		if (!tex_del_btn_normal.texture) {
			createTexure(&tex_del_btn_normal.texture, "images/ic-scene-delete-normal.png");
		}
		return tex_del_btn_normal.texture;
	case MouseState::Hover:
		if (!tex_del_btn_hover.texture) {
			createTexure(&tex_del_btn_hover.texture, "images/ic-scene-delete-over.png");
		}
		return tex_del_btn_hover.texture;
	case MouseState::Pressed:
		if (!tex_del_btn_pressed.texture) {
			createTexure(&tex_del_btn_pressed.texture, "images/ic-scene-delete-click.png");
		}
		return tex_del_btn_pressed.texture;
	case MouseState::Disabled:
		if (!tex_del_btn_disable.texture) {
			createTexure(&tex_del_btn_disable.texture, "images/ic-scene-delete-disable.png");
		}
		return tex_del_btn_disable.texture;
	default:
		break;
	}
	return nullptr;
}

gs_texture_t *PLSSceneDisplay::GetBadgeTexture()
{
	switch (GetBadgeType()) {
	case BadgeType::None:
		return nullptr;
	case BadgeType::Edit:
		if (!tex_edit_info.texture) {
			createTexure(&tex_edit_info.texture, "images/ic-badge-edit.png");
		}
		return tex_edit_info.texture;
	case BadgeType::Live:
		if (!tex_live_info.texture) {
			createTexure(&tex_live_info.texture, "images/ic-badge-live-on.png");
		}
		return tex_live_info.texture;
	case BadgeType::Record:
		if (!tex_record_info.texture) {
			createTexure(&tex_record_info.texture, "images/ic-badge-rec-on.png");
		}
		return tex_record_info.texture;
	case BadgeType::Rehearsal:
	case BadgeType::Normal:
		if (!tex_normal_info.texture) {
			createTexure(&tex_normal_info.texture, "images/ic-badge-live-off.png");
		}
		return tex_normal_info.texture;
	default:
		break;
	}
	return nullptr;
}

void PLSSceneDisplay::DrawThumbnail()
{
	if (!item_texture)
		return;

	gs_matrix_push();
	gs_matrix_identity();
	gs_blend_state_push();
	gs_blend_function(GS_BLEND_SRCALPHA, GS_BLEND_INVSRCALPHA);

	gs_rect rt = {0};
	gs_get_viewport(&rt);

	const gs_effect_t *effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);
	gs_technique_t *tech = gs_effect_get_technique(effect, "Draw");

	gs_technique_begin(tech);
	gs_technique_begin_pass(tech, 0);

	gs_effect_set_texture(gs_effect_get_param_by_name(effect, "image"), item_texture);
	// Only render scene region
	gs_draw_sprite_subregion(item_texture, 0, 0, 0 /*rt.y todo*/, rt.cx, rt.cy);

	gs_technique_end_pass(tech);
	gs_technique_end(tech);

	gs_blend_state_pop();
	gs_matrix_pop();
}

void PLSSceneDisplay::SaveSceneTexture(uint32_t cx, uint32_t cy)
{
	uint32_t width;
	uint32_t height;
	enum gs_color_format format;
	gs_texrender_t *texrender = nullptr;
	gs_texture_t *cur_rt = GetSceneDiaplayTexture(this, texrender, cx, cy, width, height, format, true);
	uint32_t tex_width = gs_texture_get_width(cur_rt);
	uint32_t tex_height = gs_texture_get_height(cur_rt);
	gs_color_format fmt = gs_texture_get_color_format(cur_rt);

	if (item_texture) {
		uint32_t item_width = gs_texture_get_width(item_texture);
		uint32_t item_height = gs_texture_get_height(item_texture);
		gs_color_format item_fmt = gs_texture_get_color_format(item_texture);
		if (tex_height != item_height || tex_width != item_width || fmt != item_fmt) {
			gs_texture_destroy(item_texture);
			item_texture = nullptr;
		}
	}

	if (!item_texture) {
		item_texture = gs_texture_create(tex_width, tex_height, fmt, 1, nullptr, GS_DYNAMIC);
	}

	gs_copy_texture(item_texture, cur_rt);
	gs_texrender_destroy(texrender);
}

void PLSSceneDisplay::DrawDefaultThumbnail() const
{
	if (!default_texture) {
		std::string path;
		GetDataFilePath("images/img-scene-default.png", path);
		default_texture = gs_texture_create_from_file(path.c_str());
	}

	gs_rect rt = {0};
	gs_get_viewport(&rt);

	gs_viewport_push();
	gs_projection_push();
	auto margin = static_cast<int>(24 * devicePixelRatioF());
	auto scale_height = static_cast<int>(64 * devicePixelRatioF());
	gs_set_viewport(0, margin, rt.cx, scale_height);
	gs_matrix_push();
	gs_matrix_identity();
	gs_blend_state_push();
	gs_blend_function(GS_BLEND_SRCALPHA, GS_BLEND_INVSRCALPHA);

	drawTexture(default_texture);

	gs_blend_state_pop();
	gs_projection_pop();
	gs_reset_viewport();
	gs_viewport_pop();
	gs_matrix_pop();
}

void PLSSceneDisplay::SetBadgeType(BadgeType type)
{
	AUTO_LOCKER
	badgeType = type;
}

BadgeType PLSSceneDisplay::GetBadgeType()
{
	AUTO_LOCKER
	return badgeType;
}

void PLSSceneDisplay::GetRadiusTexture() const
{
	if (!radius_texture.texture) {
		createTexure(&radius_texture.texture, "images/img-radius.png");
	}
}

QSize PLSSceneDisplay::GetWidgetSize()
{
	return QSize(SCENE_DISPLAY_DEFAULT_WIDTH, SCENE_DISPLAY_DEFAULT_HEIGHT);
}

void PLSSceneDisplay::ResizeDisplay()
{
	int realWidth = SCENE_DISPLAY_DEFAULT_WIDTH * devicePixelRatioF();
	int realHeight = SCENE_DISPLAY_DEFAULT_HEIGHT * devicePixelRatioF();

	auto display = GetDisplay();
	uint32_t width, height;
	obs_display_size(display, &width, &height);
	if (width != realWidth || height != realHeight) {
		obs_display_resize(display, realWidth, realHeight);
	}
}

PLSSceneItemView::PLSSceneItemView(const QString &name_, OBSScene scene_, DisplayMethod displayMethod, QWidget *parent) : QFrame(parent), scene(scene_), name(name_)
{
	ui = pls_new<Ui::PLSSceneItemView>();
	ui->setupUi(this);

	setAttribute(Qt::WA_NativeWindow);

	this->setAcceptDrops(true);
	setProperty("showHandCursor", true);

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
	ui->renameBtn->hide();
	ui->label_badge->hide();

	ui->display->SetSceneData(scene);

	SetSceneDisplayMethod(displayMethod);

	connect(this, &PLSSceneItemView::CurrentItemChanged, this, &PLSSceneItemView::OnCurrentItemChanged);
	connect(ui->modifyBtn, &QPushButton::clicked, this, &PLSSceneItemView::OnModifyButtonClicked);
	connect(ui->renameBtn, &QPushButton::clicked, this, &PLSSceneItemView::OnModifyButtonClicked);
	connect(ui->display, &PLSSceneDisplay::DeleteBtnClicked, this, &PLSSceneItemView::OnDeleteButtonClicked);
	connect(ui->display, &PLSSceneDisplay::MouseLeftButtonClicked, this, &PLSSceneItemView::OnMouseButtonClicked);
	connect(ui->display, &PLSSceneDisplay::CaptureImageFinishedSignal, this, &PLSSceneItemView::OnCaptureImageFinished);
}

PLSSceneItemView::~PLSSceneItemView()
{
	pls_delete(ui);
}

void PLSSceneItemView::SetData(OBSScene scene_)
{
	scene = scene_;
}

void PLSSceneItemView::SetSignalHandler(const SignalContainer<OBSScene> &handler_)
{
	handler = handler_;
}

void PLSSceneItemView::SetCurrentFlag(bool state)
{
	if (current != state) {
		current = state;
		ui->display->SetCurrentFlag(state);
		emit CurrentItemChanged(state);
		QTimer::singleShot(0, this, [this]() { SetStatusBadge(); });
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

void PLSSceneItemView::SetRenderFlag(bool state) const
{
	ui->display->SetRenderFlag(state);

	if (state && displayMethod == DisplayMethod::ThumbnailView && ui->display->GetDisplay()) {
		ui->display->SetRefreshThumbnail(true);
		RefreshSceneThumbnail();
	} else {
		ui->display->SetRefreshThumbnail(!state || displayMethod == DisplayMethod::ThumbnailView);
	}
}

bool PLSSceneItemView::GetRenderFlag() const
{
	return ui->display->GetRenderState();
}

void PLSSceneItemView::CustomCreateDisplay() const
{
	ui->display->CustomCreateDisplay();
}

void PLSSceneItemView::SetSceneDisplayMethod(DisplayMethod displayMethod_)
{
	ui->display->SetSceneDisplayMethod(displayMethod_);

	this->displayMethod = displayMethod_;

	if (displayMethod_ == DisplayMethod::DynamicRealtimeView || displayMethod_ == DisplayMethod::ThumbnailView) {
		ui->listFrame->hide();
		ui->display->show();
		ui->editWidget->show();
	} else {
		ui->display->hide();
		ui->editWidget->hide();
		ui->listFrame->show();
	}
	QTimer::singleShot(0, this, [this]() { SetStatusBadge(); });
}

DisplayMethod PLSSceneItemView::GetSceneDisplayMethod() const
{
	return displayMethod;
}

void PLSSceneItemView::ResizeCustom()
{
	ui->display->resize(QSize(SCENE_DISPLAY_DEFAULT_WIDTH, SCENE_DISPLAY_DEFAULT_HEIGHT));
}

OBSScene PLSSceneItemView::GetData() const
{
	return scene;
}

QString PLSSceneItemView::GetName() const
{
	return name;
}

SignalContainer<OBSScene> PLSSceneItemView::GetSignalHandler() const
{
	return handler;
}

bool PLSSceneItemView::GetCurrentFlag() const
{
	return current;
}

void PLSSceneItemView::RefreshSceneThumbnail() const
{
	if (displayMethod == DisplayMethod::ThumbnailView) {
		ui->display->SetRefreshThumbnail(true);
		ui->display->AddRenderCallback();
		ui->display->show();
	} else if (displayMethod == DisplayMethod::DynamicRealtimeView) {
		ui->display->SetRefreshThumbnail(false);
		ui->display->AddRenderCallback();
		ui->display->show();
	} else if (displayMethod == DisplayMethod::TextView) {
		ui->display->hide();
		ui->display->RemoveRenderCallback();
	}
}

void PLSSceneItemView::RepaintDisplay()
{
	ui->display->repaint();
	ui->display->ResizeDisplay();
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
		PLS_UI_STEP(MAINSCENE_MODULE, QString("The user clicks on the scene: %1 by").arg(name).toStdString().c_str(), ACTION_LBUTTON_CLICK);
		startPos = mapToParent(event->pos());
	}

	if (displayMethod == DisplayMethod::TextView)
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
		if (displayMethod == DisplayMethod::DynamicRealtimeView || DisplayMethod::ThumbnailView == displayMethod) {
			ui->display->SetDragingState(true);
		} else {
			CreateDrag(startPos, QImage());
		}
	}
}

void PLSSceneItemView::mouseDoubleClickEvent(QMouseEvent *event)
{
	OBSBasic *main = OBSBasic::Get();
	if (main) {
		PLS_UI_STEP(MAINSCENE_MODULE, QString("The user double clicks on the scene: %1 by").arg(name).toStdString().c_str(), ACTION_DBCLICK);
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
		auto func = [this]() {
			ui->nameLabel->setText(GetNameElideString());
			ui->label->setText(GetNameElideString());
		};
		QMetaObject::invokeMethod(this, func, Qt::QueuedConnection);
		return true;
	}

	if ((object == ui->nameLabel || object == ui->modifyBtn || object == ui->renameBtn) && (event->type() == QEvent::MouseMove)) {
		return true;
	}
	return QFrame::eventFilter(object, event);
}

void PLSSceneItemView::resizeEvent(QResizeEvent *event)
{
	ui->label->resize(QSize(size().width() - 40, size().height()));
	ui->display->resize(QSize(SCENE_DISPLAY_DEFAULT_WIDTH, SCENE_DISPLAY_DEFAULT_HEIGHT));
	QFrame::resizeEvent(event);
}

void PLSSceneItemView::enterEvent(QEnterEvent *event)
{
	bool gridMode = (displayMethod == DisplayMethod::DynamicRealtimeView || displayMethod == DisplayMethod::ThumbnailView);
	gridMode ? EnterEventWithGridMode() : EnterEventWithListMode();

	//PLS_ACTION_LOG(MAINFRAME_MODULE, QT_TO_UTF8(this->GetName()), "Mouse Enter");
	QFrame::enterEvent(event);
}

void PLSSceneItemView::leaveEvent(QEvent *event)
{
	bool gridMode = (displayMethod == DisplayMethod::DynamicRealtimeView || displayMethod == DisplayMethod::ThumbnailView);
	gridMode ? LeaveEventWithGridMode() : LeaveEventWithListMode();

	//PLS_ACTION_LOG(MAINFRAME_MODULE, QT_TO_UTF8(this->GetName()), "Mouse Leave");
	QFrame::leaveEvent(event);
}

void PLSSceneItemView::OnMouseButtonClicked()
{
	emit MouseButtonClicked(this);
}

void PLSSceneItemView::OnModifyButtonClicked()
{
	QString controls = this->GetName().append(" Rename");
	OnRenameOperation();
	emit ModifyButtonClicked(this);
}

void PLSSceneItemView::OnDeleteButtonClicked()
{
	QString controls = this->GetName().append(" Delete");
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

static QImage CaptureImage(uint32_t width, uint32_t height, enum gs_color_format format, uint8_t **data, const uint32_t *linesize, QString sceneName)
{
	if (format != gs_color_format::GS_BGRA || sceneName.isEmpty()) {
		return QImage();
	}

	std::vector<uchar> buffer{0};
	buffer.resize((size_t)width * height * 4);

	if (width * 4 == linesize[0]) {
		memmove(buffer.data(), data[0], (size_t)linesize[0] * height);
	} else {
		for (uint32_t i = 0; i < height; i++) {
			memmove(buffer.data() + (size_t)i * width * 4, data[0] + (size_t)i * linesize[0], (size_t)width * 4);
		}
	}

	QImage image(buffer.data(), width, height, QImage::Format_ARGB32);
	QImage imageTarget = image.copy();

	return imageTarget;
}

void PLSSceneItemView::setEnterPropertyState(bool state, QWidget *widget) const
{
	widget->setProperty(STATUS_ENTER, state);
	pls_flush_style(widget);
}

void PLSSceneItemView::CreateDrag(const QPoint &startPos_, const QImage &image)
{
	if (startPos_.x() < 0 || startPos_.y() < 0) {
		return;
	}
	auto mimeData = pls_new<QMimeData>();
	QString data =
		QString::number(this->width()).append(":").append(QString::number(this->height())).append(":").append(QString::number(startPos_.x())).append(":").append(QString::number(startPos_.y()));
	mimeData->setData(SCENE_DRAG_MIME_TYPE, QByteArray::fromStdString(data.toStdString()));
	mimeData->setData(SCENE_DRAG_GRID_MODE, QByteArray::fromStdString(QString::number(static_cast<int>(displayMethod)).toStdString()));

	auto drag = pls_new<QDrag>(this->parentWidget());
	drag->setMimeData(mimeData);
	bool gridMode = (displayMethod == DisplayMethod::DynamicRealtimeView || displayMethod == DisplayMethod::ThumbnailView);

	QPixmap pixmap(ui->display->width(), ui->display->height());
	if (!image.isNull()) {
		DrawScreenShot(pixmap, image);
	} else {
		pixmap = this->grab(gridMode ? ui->display->rect() : this->rect());
	}

	drag->setHotSpot(QPoint(gridMode ? pixmap.width() / 2 : startPos_.x(), pixmap.height() / 2));
	drag->setPixmap(pixmap);
	drag->exec();
	drag->deleteLater();
}

QString PLSSceneItemView::GetNameElideString() const
{
	bool gridMode = (displayMethod == DisplayMethod::DynamicRealtimeView || displayMethod == DisplayMethod::ThumbnailView);

	const QLabel *label = nullptr;
	gridMode ? label = ui->nameLabel : label = ui->label;

	QFontMetrics fontWidth(label->font());
	if (fontWidth.horizontalAdvance(name) > label->width())
		return fontWidth.elidedText(name, Qt::ElideRight, label->width());
	return name;
}

void PLSSceneItemView::RenameWithGridMode() const
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

void PLSSceneItemView::EnterEventWithGridMode() const
{
	setEnterPropertyState(true, ui->editWidget);
	setEnterPropertyState(true, ui->nameLabel);

	if (isFinishEditing) {
		ui->modifyBtn->show();
		setEnterPropertyState(true, ui->modifyBtn);
	}
}

void PLSSceneItemView::LeaveEventWithGridMode() const
{
	ui->modifyBtn->hide();
	if (!this->current) {
		setEnterPropertyState(false, ui->editWidget);
		setEnterPropertyState(false, ui->modifyBtn);
		setEnterPropertyState(false, ui->nameLabel);
	}
}

std::string PLSSceneItemView::GetBadgeIconPath(BadgeType type) const
{
	std::string path;

	switch (type) {
	case BadgeType::None:
		break;
	case BadgeType::Edit:
		GetDataFilePath("images/ic-badge-edit.png", path);
		break;
	case BadgeType::Live:
		GetDataFilePath("images/ic-badge-live-on.png", path);
		break;
	case BadgeType::Record:
		GetDataFilePath("images/ic-badge-rec-on.png", path);
		break;
	case BadgeType::Rehearsal:
	case BadgeType::Normal:
		GetDataFilePath("images/ic-badge-live-off.png", path);
	default:
		break;
	}
	return path;
}

void PLSSceneItemView::DrawScreenShot(QPixmap &pixmap, const QImage &image)
{
	QPainter painter(&pixmap);
	painter.fillRect(ui->display->rect(), QColor("#333333"));

	int displayWidth = ui->display->width();
	int displayHeight = ui->display->height();
	int startMargin = 24;
	int borderWidth = 4;
	QRect imageRect;

	OBSBasic *main = reinterpret_cast<OBSBasic *>(App()->GetMainWindow());
	uint32_t cx = config_get_uint(main->Config(), "Video", "BaseCX");
	uint32_t cy = config_get_uint(main->Config(), "Video", "BaseCY");
	if (cx >= cy) {
		int realHeight = (displayWidth * cy) / cx;
		startMargin = (displayHeight - realHeight) / 2;
		imageRect.setRect(0, startMargin, ui->display->width(), ui->display->height() - 2 * startMargin);
	} else {
		int realWidth = (displayWidth * cx) / cy;
		startMargin = (displayWidth - realWidth) / 2;
		imageRect.setRect(startMargin, 0, ui->display->width() - 2 * startMargin, ui->display->height());
	}

	if (!GetRenderFlag()) {
		std::string defaultImagePath;
		GetDataFilePath("images/img-scene-default.png", defaultImagePath);
		painter.fillRect(imageRect, QColor("#111111"));
		imageRect.setRect(0, 24, ui->display->width(), 64);
		painter.drawImage(imageRect, QImage(defaultImagePath.c_str()));
	} else {
		QImage newImage = image.convertToFormat(QImage::Format_RGB888);
		painter.drawImage(imageRect, newImage);
	}
	std::string iconPath = GetBadgeIconPath(badgeType);
	if (!iconPath.empty()) {
		int leftTopMargin = 4;
		int badgeIconWidth = 31;
		int badgeIconHeight = 16;
		painter.drawImage(QRect(leftTopMargin, leftTopMargin, badgeIconWidth, badgeIconHeight), QImage(iconPath.c_str()));
	}

	int btnWidth = 16;
	QSvgRenderer renderer(QString(":/resource/images/btn-close-normal.svg"));
	QPixmap delPixmap(btnWidth, btnWidth);
	delPixmap.fill(Qt::transparent);
	QPainter delPainter(&delPixmap);
	renderer.render(&delPainter);

	int margin = 4;
	painter.drawPixmap(QRect(this->width() - btnWidth - margin, margin, btnWidth, btnWidth), delPixmap);

	QPen pen(QColor("#effc35"));
	pen.setWidth(borderWidth);
	painter.setPen(pen);
	painter.drawRect(ui->display->rect());
}

void PLSSceneItemView::RenameWithListMode() const
{
	setEnterPropertyState(false, ui->listFrame);
	ui->lineEdit->setText(GetName());
	ui->lineEdit->setFocus();
	ui->lineEdit->selectAll();
	ui->lineEdit->show();
	ui->label->hide();
	ui->renameBtn->hide();
}

void PLSSceneItemView::EnterEventWithListMode() const
{
	if (isFinishEditing) {
		ui->renameBtn->show();
		setEnterPropertyState(true, ui->renameBtn);
		setEnterPropertyState(true, ui->listFrame);
	}
}

void PLSSceneItemView::LeaveEventWithListMode() const
{
	setEnterPropertyState(false, ui->listFrame);
	setEnterPropertyState(false, ui->renameBtn);
	ui->renameBtn->hide();
}

void PLSSceneItemView::SetStatusBadge()
{
	BadgeType badgeType = BadgeType::None;
	do {
		auto main = (OBSBasic *)obs_frontend_get_main_window();
		bool studioMode = main->IsPreviewProgramMode();
		if (!studioMode) {
			badgeType = BadgeType::None;
			break;
		}

		bool live = PLSCHANNELS_API->isLiving();
		bool rehearsal = PLSCHANNELS_API->isRehearsaling();
		bool recording = PLSCHANNELS_API->isRecording();

		OBSSource previewSrc = main->GetCurrentSceneSource();
		OBSSource programSrc = main->GetProgramSource();

		OBSSource source = obs_scene_get_source(scene);

		if (source == previewSrc && source != programSrc) {
			badgeType = BadgeType::Edit;
			break;
		}

		if (source == programSrc) {
			if (live && !rehearsal) {
				badgeType = BadgeType::Live;
				break;
			}

			if (recording) {
				badgeType = BadgeType::Record;
				break;
			}

			if (rehearsal) {
				badgeType = BadgeType::Rehearsal;
				break;
			}

			badgeType = BadgeType::Normal;
		}
	} while (false);

	if (DisplayMethod::TextView == displayMethod) {
		FlushBadgeStyle(badgeType);
	} else {
		ui->display->SetBadgeType(badgeType);
	}
	this->badgeType = badgeType;
}

void PLSSceneItemView::FlushBadgeStyle(BadgeType type) const
{
	switch (type) {
	case BadgeType::None:
		ui->label_badge->hide();
		break;
	case BadgeType::Edit:
		ui->label_badge->setProperty(BADGE_TYPE, "edit");
		ui->label_badge->show();
		break;
	case BadgeType::Live:
		ui->label_badge->setProperty(BADGE_TYPE, "live");
		ui->label_badge->show();
		break;
	case BadgeType::Record:
		ui->label_badge->setProperty(BADGE_TYPE, "record");
		ui->label_badge->show();
		break;
	case BadgeType::Rehearsal:
		ui->label_badge->setProperty(BADGE_TYPE, "rehearsal");
		ui->label_badge->show();
		break;
	case BadgeType::Normal:
		ui->label_badge->setProperty(BADGE_TYPE, "normal");
		ui->label_badge->show();
		break;
	default:
		ui->label_badge->hide();
		break;
	}
	pls_flush_style(ui->label_badge);
}

void PLSSceneItemView::SetContentMargins(bool state) const
{
	Q_UNUSED(state)
	ui->label->setText(GetNameElideString());
}

void PLSSceneItemView::OnCaptureImageFinished(const QImage &image)
{
	CreateDrag(startPos, image);
}

void PLSSceneItemView::OnCurrentItemChanged(bool state) const
{
	setEnterPropertyState(state, ui->editWidget);
	setEnterPropertyState(state, ui->modifyBtn);
	setEnterPropertyState(state, ui->nameLabel);
	pls_flush_style(ui->label, STATUS_CLICKED, state);
}

static QImage CaptureSceneImage(QString sceneName)
{
	uint32_t width;
	uint32_t height;
	enum gs_color_format format;
	texture_map_info tmi;
	QImage image;
	if (auto ss = gs_device_canvas_map(&width, &height, &format, tmi.data, tmi.linesize); ss) {
		image = CaptureImage(width, height, format, tmi.data, tmi.linesize, sceneName);
	}
	return image;
}

void PLSSceneDisplay::RenderScene(void *data, uint32_t cx, uint32_t cy)
{
	auto window = static_cast<PLSSceneDisplay *>(data);
	if (!window) {
		return;
	}

	uint32_t targetCX;
	uint32_t targetCY;
	int x;
	int y;
	int newCX;
	int newCY;
	float scale;

	OBSSource source = obs_scene_get_source(window->scene);
	if (source) {
		targetCX = qMax(obs_source_get_width(source), 1u);
		targetCY = qMax(obs_source_get_height(source), 1u);
	} else {
		struct obs_video_info ovi;
		obs_get_video_info(&ovi);
		targetCX = ovi.base_width;
		targetCY = ovi.base_height;
	}

	GetScaleAndCenterPos(targetCX, targetCY, cx, cy, x, y, scale);

	newCX = int(scale * float(targetCX));
	newCY = int(scale * float(targetCY));

	if (!window->GetRenderState()) {
		startRegion(x, y, newCX, newCY, 0.0f, float(targetCX), 0.0f, float(targetCY));
		window->DrawSceneBackground();
		endRegion();

		window->DrawDefaultThumbnail();
		window->DrawOverlay();
		goto CAPTURE_SCENE_IMG;
	}

	if (!window->GetRefreshThumbnail() && window->TestSceneDisplayMethod(DisplayMethod::ThumbnailView)) {
		startRegion(x, y, newCX, newCY, 0.0f, float(targetCX), 0.0f, float(targetCY));
		window->DrawSceneBackground();
		window->DrawThumbnail();
		endRegion();
		window->DrawOverlay();
		goto CAPTURE_SCENE_IMG;
	}

	startRegion(x, y, newCX, newCY, 0.0f, float(targetCX), 0.0f, float(targetCY));
	window->DrawSceneBackground();
	endRegion();

	startRegion(x, y, newCX, newCY, 0.0f, float(targetCX), 0.0f, float(targetCY));

	if (source) {
		obs_source_video_render(source);

		if (window->TestSceneDisplayMethod(DisplayMethod::ThumbnailView)) {
			window->SaveSceneTexture(cx, cy);
			window->SetRefreshThumbnail(false);
		}
	}

	endRegion();

	window->DrawOverlay();

CAPTURE_SCENE_IMG:
	if (window->GetDragingState()) {
		QImage image = GetSceneDisplayImagePath(data, cx, cy);
		if (!image.isNull()) {
			window->SetDragingState(false);
			QMetaObject::invokeMethod(window, "CaptureImageFinished", Qt::QueuedConnection, Q_ARG(const QImage &, image));
		}
	}
}

void PLSSceneDisplay::UpdateMouseState(MouseState state)
{
	AUTO_LOCKER
	mouseState = state;
}

MouseState PLSSceneDisplay::GetMouseState()
{
	AUTO_LOCKER
	return mouseState;
}

QImage PLSSceneDisplay::GetSceneDisplayImagePath(void *data, uint32_t cx, uint32_t cy)
{
	auto window = static_cast<PLSSceneDisplay *>(data);
	if (!window) {
		return QImage();
	}

	uint32_t width = 0;
	uint32_t height = 0;
	enum gs_color_format format = GS_RGBA;
	gs_texrender_t *texrender = nullptr;
	gs_texture_t *texture = GetSceneDiaplayTexture(data, texrender, cx, cy, width, height, format);
	if (window->displayMethod == DisplayMethod::ThumbnailView) {
		obs_enter_graphics();
		gs_copy_texture(texture, window->item_texture);
		obs_leave_graphics();
	}
	if (!texture) {
		return QImage();
	}
	obs_enter_graphics();
	gs_stagesurf_t *stagesurf = gs_stagesurface_create(width, height, format);
	gs_stage_texture(stagesurf, texture);

	//COPY
	uint8_t *videoData = nullptr;
	uint32_t videoLinesize = 0;
	QImage image;
	if (gs_stagesurface_map(stagesurf, &videoData, &videoLinesize)) {
		image = QImage(width, height, QImage::Format::Format_RGBX8888);
		auto linesize = image.bytesPerLine();
		for (int y = 0; y < (int)height; y++)
			memcpy(image.scanLine(y), videoData + (y * videoLinesize), linesize);

		gs_stagesurface_unmap(stagesurf);
	}
	gs_texrender_destroy(texrender);
	gs_stagesurface_destroy(stagesurf);

	obs_leave_graphics();
	return image.copy();
}

gs_texture_t *PLSSceneDisplay::GetSceneDiaplayTexture(void *data, gs_texrender_t *texrender, uint32_t cx, uint32_t cy, uint32_t &targetCX, uint32_t &targetCY, gs_color_format &format, bool needScale)
{
	auto window = static_cast<PLSSceneDisplay *>(data);
	if (!window) {
		return nullptr;
	}

	obs_enter_graphics();

	OBSSource source = obs_scene_get_source(window->scene);
	if (source) {
		targetCX = obs_source_get_base_width(source);
		targetCY = obs_source_get_base_height(source);
	} else {
		obs_video_info ovi;
		obs_get_video_info(&ovi);
		targetCX = ovi.base_width;
		targetCY = ovi.base_height;
	}

	if (!targetCX || !targetCY) {
		PLS_WARN(MAINSCENE_MODULE, "Cannot get scene source width or height");
		obs_leave_graphics();
		return nullptr;
	}

	int x;
	int y;
	int newCX;
	int newCY;
	float scale;
	GetScaleAndCenterPos(targetCX, targetCY, cx, cy, x, y, scale);

	newCX = int(scale * float(targetCX));
	newCY = int(scale * float(targetCY));

#ifdef _WIN32
	enum gs_color_space space = obs_source_get_color_space(source, 0, nullptr);
	if (space == GS_CS_709_EXTENDED) {
		/* Convert for JXR */
		space = GS_CS_709_SCRGB;
	}
#else
	/* Tonemap to SDR if HDR */
	const enum gs_color_space space = GS_CS_SRGB;
#endif
	format = gs_get_format_from_space(space);
	if (format != gs_color_format::GS_RGBA) {
		PLS_WARN(MAINSCENE_MODULE, "Cannot get GS_RGBA color format");
		obs_leave_graphics();
		return nullptr;
	}
	texrender = gs_texrender_create(format, GS_ZS_NONE);
	int texX = needScale ? newCX : targetCX;
	int texY = needScale ? newCY : targetCY;

	if (gs_texrender_begin_with_color_space(texrender, texX, texY, space)) {
		vec4 zero;
		vec4_zero(&zero);

		gs_clear(GS_CLEAR_COLOR, &zero, 0.0f, 0);
		gs_ortho(0.0f, (float)targetCX, 0.0f, (float)targetCY, -100.0f, 100.0f);

		gs_blend_state_push();
		gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);

		if (source) {
			obs_source_inc_showing(source);
			obs_source_video_render(source);
			obs_source_dec_showing(source);
		} else {
			obs_render_main_texture();
		}

		gs_blend_state_pop();
		gs_texrender_end(texrender);
	}

	gs_texture_t *texture = gs_texrender_get_texture(texrender);
	obs_leave_graphics();
	return texture;
}
