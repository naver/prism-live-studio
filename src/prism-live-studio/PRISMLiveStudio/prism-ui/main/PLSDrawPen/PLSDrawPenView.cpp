#include "PLSDrawPenView.h"
#include "PLSBasic.h"
#include "ui_PLSDrawPenView.h"
#include "window-basic-main.hpp"
#include "PLSDrawPenMgr.h"
#include "prism-ui/log/module_names.h"
#include <liblog.h>
#include <QButtonGroup>
#include <QRadioButton>
#include <QBitmap>
#include <QPainter>

const QString ANALOG_DRAWPEN_DRAW_KEY = "draw";

static const std::vector<std::string> shapeTips{"drawpen.toolbar.arrow.toolTip", "drawpen.toolbar.line.toolTip", "drawpen.toolbar.rect.toolTip", "drawpen.toolbar.round.toolTip",
						"drawpen.toolbar.triangle.toolTip"};

PLSDrawPenView::PLSDrawPenView(QWidget *parent) : QFrame(parent)
{
	ui = pls_new<Ui::PLSDrawPenView>();
	pls_add_css(this, {"PLSDrawPenView"});
	ui->setupUi(this);
	ui->label_separate->setObjectName("label_separate");
	ui->label_separate_2->setObjectName("label_separate");
	ui->label_separate_3->setObjectName("label_separate");
	ui->label_separate_4->setObjectName("label_separate");
	pls_flush_style_recursive(this);

	drawGroup = pls_new<QButtonGroup>(ui->frame_Content);
	drawGroup->addButton(ui->pushButton_Pen, (int)DrawTypeIndex::DTI_PEN);
	drawGroup->addButton(ui->pushButton_Highlighter, (int)DrawTypeIndex::DTI_HIGHLIGHHTER);
	drawGroup->addButton(ui->pushButton_GlowPen, (int)DrawTypeIndex::DTI_GLOW_PEN);
	drawGroup->addButton(ui->pushButton_CurShape, (int)DrawTypeIndex::DTI_SHAPE);
	drawGroup->addButton(ui->pushButton_Rubber, (int)DrawTypeIndex::DTI_RUBBER);
	connect(drawGroup, QOverload<int, bool>::of(&QButtonGroup::idToggled), this, &PLSDrawPenView::drawGroupButtonChanged);

	ui->pushButton_Pen->setToolTip(QTStr("drawpen.toolbar.pen.toolTip"));
	ui->pushButton_Highlighter->setToolTip(QTStr("drawpen.toolbar.highlighter.toolTip"));
	ui->pushButton_GlowPen->setToolTip(QTStr("drawpen.toolbar.glowpen.toolTip"));
	ui->pushButton_CurShape->setToolTip(QTStr("drawpen.toolbar.arrow.toolTip"));
	ui->pushButton_ShapeOpen->setToolTip(QTStr("drawpen.toolbar.shape.seclect.toolTip"));
	ui->pushButton_Rubber->setToolTip(QTStr("drawpen.toolbar.earse.toolTip"));
	ui->pushButton_Width->setToolTip(QTStr("drawpen.toolbar.line.width.toolTip"));
	ui->pushButton_Color->setToolTip(QTStr("drawpen.toolbar.color.toolTip"));
	ui->pushButton_Undo->setToolTip(QTStr("drawpen.toolbar.undo.toolTip"));
	ui->pushButton_Redo->setToolTip(QTStr("drawpen.toolbar.redo.toolTip"));
	ui->pushButton_Clear->setToolTip(QTStr("drawpen.toolbar.clear.toolTip"));
	ui->pushButton_Visible->setToolTip(QTStr("drawpen.toolbar.visible.toolTip"));

	ui->pushButton_CurShape->setProperty("style", 0);
	ui->pushButton_Width->setProperty("style", 1);
	ui->pushButton_Color->setProperty("style", 0);
	ui->pushButton_ShapeOpen->setProperty("open", true);

	ui->pushButton_ShapeOpen->setCheckable(true);
	ui->pushButton_Width->setCheckable(true);
	ui->pushButton_Color->setCheckable(true);

	ui->pushButton_Undo->setDisabled(true);
	ui->pushButton_Redo->setDisabled(true);

	connect(PLSDrawPenMgr::Instance(), &PLSDrawPenMgr::UndoDisabled, this, &PLSDrawPenView::OnUndoDisabled);
	connect(PLSDrawPenMgr::Instance(), &PLSDrawPenMgr::RedoDisabled, this, &PLSDrawPenView::OnRedoDisabled);
}

PLSDrawPenView::~PLSDrawPenView()
{
	disconnect(PLSDrawPenMgr::Instance(), &PLSDrawPenMgr::UndoDisabled, this, &PLSDrawPenView::OnUndoDisabled);
	disconnect(PLSDrawPenMgr::Instance(), &PLSDrawPenMgr::RedoDisabled, this, &PLSDrawPenView::OnRedoDisabled);
	pls_delete(ui);
}

bool PLSDrawPenView::IsDrawPenMode() const
{
	if (this->isVisible() && drawVisible)
		return true;
	return false;
}

void PLSDrawPenView::createCustomGroup(QButtonGroup *&group, QGridLayout *&gLayout, QString name, int row, int colum) const
{
	gLayout->setSpacing(0);
	gLayout->setContentsMargins(0, 0, 0, 0);
	gLayout->setAlignment(Qt::AlignHCenter);
	for (int i = 0; i < row * colum; i++) {
		QAbstractButton *button = pls_new<QPushButton>();
		QString suffix = name + "_" + QString::number(i);
		button->setObjectName(suffix);
		button->setAutoExclusive(true);
		button->setCheckable(true);
		button->setFixedSize(28, 28);
		group->addButton(button, i);
		gLayout->addWidget(button, int(i) / colum, int(i) % colum);
	}
}

void PLSDrawPenView::setViewEnabled(bool visible)
{
	bool undoDisabled = PLSDrawPenMgr::Instance()->UndoEmpty() || !visible;
	bool redoDisabled = PLSDrawPenMgr::Instance()->RedoEmpty() || !visible;
	ui->pushButton_Undo->setDisabled(undoDisabled);
	ui->pushButton_Redo->setDisabled(redoDisabled);

	bool rubber = PLSDrawPenMgr::Instance()->GetCurrentDrawType() == DrawType::DT_RUBBER;
	bool widthDisabled = rubber || !visible;
	bool colorDisabled = rubber || !visible;
	ui->pushButton_Width->setDisabled(widthDisabled);
	ui->pushButton_Color->setDisabled(colorDisabled);

	ui->widget_draw->setEnabled(visible);
	ui->pushButton_Clear->setEnabled(visible);
	ui->pushButton_Visible->setProperty("visibled", QVariant(visible));

	pls_flush_style(ui->pushButton_Visible);
}

void PLSDrawPenView::shapeGroupButtonChangedInternal(int index)
{
	PLSDrawPenMgr::Instance()->SetCurrentShapeType(ShapeType(index));

	if (shapeTips.size() > index)
		ui->pushButton_CurShape->setToolTip(QTStr(shapeTips.at(index).c_str()));
	ui->pushButton_CurShape->setProperty("style", index);
	pls_flush_style(ui->pushButton_CurShape);
	pls_send_analog(AnalogType::ANALOG_DRAWPEN, {{ANALOG_DRAWPEN_DRAW_KEY, "Shape_" + QString::number(index)}});
}

void PLSDrawPenView::UpdateView(OBSScene scene, bool reset)
{
	if (reset) {
		PLSDrawPenMgr::Instance()->ResetProperties();
	}
	PLSDrawPenMgr::Instance()->UpdateCurrentDrawPen(scene);
	DrawType type = PLSDrawPenMgr::Instance()->GetCurrentDrawType();
	switch (type) {
	case DrawType::DT_PEN:
		ui->pushButton_Pen->setChecked(true);
		break;
	case DrawType::DT_HIGHLIGHTER:
		ui->pushButton_Highlighter->setChecked(true);
		break;
	case DrawType::DT_GLOW_PEN:
		ui->pushButton_GlowPen->setChecked(true);
		break;
	case DrawType::DT_2DSHAPE:
		ui->pushButton_CurShape->setChecked(true);
		break;
	case DrawType::DT_RUBBER:
		ui->pushButton_Rubber->setChecked(true);
		break;
	default:
		break;
	}

	shapeGroupButtonChangedInternal((int)PLSDrawPenMgr::Instance()->GetCurrentShapeType());
	widthGroupButtonChanged(PLSDrawPenMgr::Instance()->GetLineWidthIndex());
	colorGroupButtonChanged(PLSDrawPenMgr::Instance()->GetColorIndex());

	drawVisible = PLSDrawPenMgr::Instance()->DrawVisible();
	setViewEnabled(drawVisible);
}

void PLSDrawPenView::drawGroupButtonChanged(int index, bool)
{
	auto type = (DrawTypeIndex)index;
	ui->pushButton_Width->setDisabled(false);
	ui->pushButton_Color->setDisabled(false);
	switch (type) {
	case DrawTypeIndex::DTI_PEN:
		onPenClicked();
		break;
	case DrawTypeIndex::DTI_HIGHLIGHHTER:
		onHighlighterClicked();
		break;
	case DrawTypeIndex::DTI_GLOW_PEN:
		onGlowPenClicked();
		break;
	case DrawTypeIndex::DTI_SHAPE:
		onShapeClicked();
		break;
	case DrawTypeIndex::DTI_RUBBER:
		onRubberClicked();
		ui->pushButton_Width->setDisabled(true);
		ui->pushButton_Color->setDisabled(true);
		break;
	default:
		break;
	}
}

void PLSDrawPenView::shapeGroupButtonChanged(int index)
{
	ui->pushButton_CurShape->setChecked(true);
	PLSDrawPenMgr::Instance()->SetCurrentDrawType(DrawType::DT_2DSHAPE);
	shapeGroupButtonChangedInternal(index);
}

void PLSDrawPenView::widthGroupButtonChanged(int index)
{
	PLSDrawPenMgr::Instance()->SetLineWidthIndex(index);
	ui->pushButton_Width->setProperty("style", index);
	pls_flush_style(ui->pushButton_Width);
	if (widthPopup)
		widthPopup->hide();
	pls_send_analog(AnalogType::ANALOG_DRAWPEN, {{ANALOG_DRAWPEN_DRAW_KEY, "Line_" + QString::number(index)}});
}

void PLSDrawPenView::colorGroupButtonChanged(int index)
{
	PLSDrawPenMgr::Instance()->SetColorIndex(index);
	ui->pushButton_Color->setProperty("style", index);
	pls_flush_style(ui->pushButton_Color);
	if (colorPopup)
		colorPopup->hide();
	pls_send_analog(AnalogType::ANALOG_DRAWPEN, {{ANALOG_DRAWPEN_DRAW_KEY, "Color_" + QString::number(index)}});
}

void PLSDrawPenView::onPenClicked() const
{
	if (ui->pushButton_Pen->isChecked()) {
		PLSDrawPenMgr::Instance()->SetCurrentDrawType(DrawType::DT_PEN);
		pls_send_analog(AnalogType::ANALOG_DRAWPEN, {{ANALOG_DRAWPEN_DRAW_KEY, "Pen"}});
	}
	PLS_UI_STEP(DRAWPEN_MODULE, "DrawPen Selected Pen Type", ACTION_CLICK);
}

void PLSDrawPenView::onHighlighterClicked() const
{
	if (ui->pushButton_Highlighter->isChecked()) {
		PLSDrawPenMgr::Instance()->SetCurrentDrawType(DrawType::DT_HIGHLIGHTER);
		pls_send_analog(AnalogType::ANALOG_DRAWPEN, {{ANALOG_DRAWPEN_DRAW_KEY, "Highlighter"}});
	}
	PLS_UI_STEP(DRAWPEN_MODULE, "DrawPen Selected Highlighter Type", ACTION_CLICK);
}

void PLSDrawPenView::onGlowPenClicked() const
{
	if (ui->pushButton_GlowPen->isChecked()) {
		PLSDrawPenMgr::Instance()->SetCurrentDrawType(DrawType::DT_GLOW_PEN);
		pls_send_analog(AnalogType::ANALOG_DRAWPEN, {{ANALOG_DRAWPEN_DRAW_KEY, "GlowPen"}});
	}
	PLS_UI_STEP(DRAWPEN_MODULE, "DrawPen Selected GlowPen Type", ACTION_CLICK);
}

void PLSDrawPenView::onShapeClicked() const
{
	if (ui->pushButton_CurShape->isChecked()) {
		PLSDrawPenMgr::Instance()->SetCurrentDrawType(DrawType::DT_2DSHAPE);
		int index = ui->pushButton_CurShape->property("style").toInt();
		PLSDrawPenMgr::Instance()->SetCurrentShapeType(ShapeType(index));
		pls_send_analog(AnalogType::ANALOG_DRAWPEN, {{ANALOG_DRAWPEN_DRAW_KEY, "Shape_" + QString::number(index)}});
	}
	PLS_UI_STEP(DRAWPEN_MODULE, "DrawPen Selected Shape Type", ACTION_CLICK);
}

void PLSDrawPenView::onRubberClicked() const
{
	if (ui->pushButton_Rubber->isChecked()) {
		PLSDrawPenMgr::Instance()->SetCurrentDrawType(DrawType::DT_RUBBER);
		pls_send_analog(AnalogType::ANALOG_DRAWPEN, {{ANALOG_DRAWPEN_DRAW_KEY, "Rubber"}});
	}
	PLS_UI_STEP(DRAWPEN_MODULE, "DrawPen Selected Rubber Type", ACTION_CLICK);
}

void PLSDrawPenView::on_pushButton_ShapeOpen_clicked()
{
	if (!shapePopup) {
		shapePopup = pls_new<QWidget>(this);
		shapePopup->setWindowFlags(Qt::Popup | Qt::NoDropShadowWindowHint | Qt::FramelessWindowHint);
		shapePopup->setWindowModality(Qt::NonModal);
		shapePopup->setObjectName("popup");
		shapePopup->installEventFilter(this);

		auto shapeGroup = pls_new<QButtonGroup>(shapePopup);
		auto shapeLayout = pls_new<QGridLayout>(shapePopup);
		createCustomGroup(shapeGroup, shapeLayout, "shape", 1, 5);
		connect(shapeGroup, QOverload<int>::of(&QButtonGroup::idClicked), this, [this](int index) {
			shapePopup->hide();
			shapeGroupButtonChanged(index);
#if defined(Q_OS_MACOS)
			/// fix: on mac, after selected the shape button. the selected button status is
			/// still highlight when open next time.
			QPointer<PLSDrawPenView> thisPointer = this;
			QMetaObject::invokeMethod(
				thisPointer,
				[thisPointer]() {
					if (!thisPointer || !thisPointer->shapePopup)
						return;

					pls_delete(thisPointer->shapePopup);
					thisPointer->shapePopup = NULL;
				},
				Qt::QueuedConnection);
#endif
		});
		shapePopup->setLayout(shapeLayout);
	}

	if (!shapePopup)
		return;

	if (ui->pushButton_ShapeOpen->isChecked()) {
		shapePopup->resize(162, 50);
		QPoint offset(-95, 4);
		QPoint p = ui->pushButton_ShapeOpen->mapToGlobal(QPoint(0, ui->pushButton_ShapeOpen->size().height())) + offset;
		shapePopup->move(p);
		shapePopup->show();
		ui->pushButton_ShapeOpen->setProperty("open", false);
		pls_flush_style(ui->pushButton_ShapeOpen);
	}
}

void PLSDrawPenView::on_pushButton_Width_clicked()
{
	if (!widthPopup) {
		widthPopup = pls_new<QWidget>(this);
		widthPopup->setWindowFlags(Qt::Popup | Qt::NoDropShadowWindowHint | Qt::FramelessWindowHint);
		widthPopup->setWindowModality(Qt::NonModal);
		widthPopup->setObjectName("popup");
		widthPopup->installEventFilter(this);
		auto widthGroup = pls_new<QButtonGroup>(widthPopup);
		auto widthLayout = pls_new<QGridLayout>(widthPopup);
		createCustomGroup(widthGroup, widthLayout, "line_width", 1, 5);
		connect(widthGroup, QOverload<int>::of(&QButtonGroup::idClicked), this, [this](int index) {
			widthPopup->hide();
			widthGroupButtonChanged(index);
#if defined(Q_OS_MACOS)
			/// fix: on mac, after selected the width button. the selected button status is
			/// still highlight when open next time.
			QPointer<PLSDrawPenView> thisPointer = this;
			QMetaObject::invokeMethod(
				thisPointer,
				[thisPointer]() {
					if (!thisPointer || !thisPointer->widthPopup)
						return;

					pls_delete(thisPointer->widthPopup);
					thisPointer->widthPopup = NULL;
				},
				Qt::QueuedConnection);
#endif
		});
		widthPopup->setLayout(widthLayout);
	}

	if (!widthPopup)
		return;

	if (ui->pushButton_Width->isChecked()) {
		widthPopup->resize(162, 50);
		QPoint offset(-67, 4);
		QPoint p = ui->pushButton_Width->mapToGlobal(QPoint(0, ui->pushButton_Width->size().height())) + offset;
		widthPopup->move(p);
		widthPopup->show();
	}
}

void PLSDrawPenView::on_pushButton_Color_clicked()
{
	if (!colorPopup) {
		colorPopup = pls_new<QWidget>(this);
		colorPopup->setWindowFlags(Qt::Popup | Qt::NoDropShadowWindowHint | Qt::FramelessWindowHint);
		colorPopup->setWindowModality(Qt::NonModal);
		colorPopup->setObjectName("popup");
		colorPopup->installEventFilter(this);
		auto colorGroup = pls_new<QButtonGroup>(colorPopup);
		auto colorLayout = pls_new<QGridLayout>(colorPopup);
		createCustomGroup(colorGroup, colorLayout, "color", 2, 6);
		connect(colorGroup, QOverload<int>::of(&QButtonGroup::idClicked), this, [this](int index) {
			colorPopup->hide();
			colorGroupButtonChanged(index);
#if defined(Q_OS_MACOS)
			/// fix: on mac, after selected the color button. the selected button status is
			/// still highlight when open next time.
			QPointer<PLSDrawPenView> thisPointer = this;
			QMetaObject::invokeMethod(
				thisPointer,
				[thisPointer]() {
					if (!thisPointer || !thisPointer->colorPopup)
						return;

					pls_delete(thisPointer->colorPopup);
					thisPointer->colorPopup = NULL;
				},
				Qt::QueuedConnection);
#endif
		});
		colorPopup->setLayout(colorLayout);
	}
	if (!colorPopup)
		return;

	if (ui->pushButton_Color->isChecked()) {
		colorPopup->resize(190, 78);
		QPoint offset(-81, 4);
		QPoint p = ui->pushButton_Color->mapToGlobal(QPoint(0, ui->pushButton_Color->size().height())) + offset;
		colorPopup->move(p);
		colorPopup->show();
	}
}

void PLSDrawPenView::on_pushButton_Undo_clicked() const
{
	PLSDrawPenMgr::Instance()->UndoStroke();
	PLS_UI_STEP(DRAWPEN_MODULE, "DrawPen Undo", ACTION_CLICK);
	pls_send_analog(AnalogType::ANALOG_DRAWPEN, {{ANALOG_DRAWPEN_DRAW_KEY, "Undo"}});
}

void PLSDrawPenView::on_pushButton_Redo_clicked() const
{
	PLSDrawPenMgr::Instance()->RedoStroke();
	PLS_UI_STEP(DRAWPEN_MODULE, "DrawPen Redo", ACTION_CLICK);
	pls_send_analog(AnalogType::ANALOG_DRAWPEN, {{ANALOG_DRAWPEN_DRAW_KEY, "Redo"}});
}

void PLSDrawPenView::on_pushButton_Clear_clicked() const
{
	PLSDrawPenMgr::Instance()->ClearStrokes();
	PLS_UI_STEP(DRAWPEN_MODULE, "DrawPen Clear", ACTION_CLICK);
	pls_send_analog(AnalogType::ANALOG_DRAWPEN, {{ANALOG_DRAWPEN_DRAW_KEY, "Clear"}});
}

void PLSDrawPenView::on_pushButton_Visible_clicked()
{
	drawVisible = !PLSDrawPenMgr::Instance()->DrawVisible();
	PLSDrawPenMgr::Instance()->OnDrawVisible(drawVisible);
	setViewEnabled(drawVisible);
	PLS_UI_STEP(DRAWPEN_MODULE, "DrawPen  " + drawVisible ? "Visible" : "Invisible", ACTION_CLICK);
	pls_send_analog(AnalogType::ANALOG_DRAWPEN, {{ANALOG_DRAWPEN_DRAW_KEY, drawVisible ? "Visible" : "Invisible"}});
}

void PLSDrawPenView::OnUndoDisabled(bool disabled)
{
	ui->pushButton_Undo->setDisabled(disabled);
}

void PLSDrawPenView::OnRedoDisabled(bool disabled)
{
	ui->pushButton_Redo->setDisabled(disabled);
}

void PLSDrawPenView::showEvent(QShowEvent *event)
{
	App()->getMainView()->updateSideBarButtonStyle(ConfigId::DrawPenConfig, true);
	PLSDrawPenMgr::Instance()->UpdateCursorPixmap();
	PLSBasic::instance()->OnToolAreaVisible();
}

void PLSDrawPenView::hideEvent(QHideEvent *event)
{
	App()->getMainView()->updateSideBarButtonStyle(ConfigId::DrawPenConfig, false);
	PLSBasic::instance()->OnToolAreaVisible();
}

void PLSDrawPenView::resizeEvent(QResizeEvent *event)
{
	int w = this->width();
	int maxW = ui->frame_Content->maximumWidth();
	int minW = ui->frame_Content->minimumWidth();
	if (w >= maxW) {
		int ten = 10;
		int twenty = 20;
		int margin = 46;
		ui->widget->layout()->setSpacing(twenty);
		ui->widget_draw->layout()->setSpacing(twenty);
		ui->widget_options->layout()->setSpacing(twenty);
		ui->horizontalLayout_5->setSpacing(ten);
		ui->horizontalLayout_3->setSpacing(ten);
		ui->horizontalLayout_4->setSpacing(ten);
		ui->horizontalLayout_7->setSpacing(ten);
		ui->widget->layout()->setContentsMargins(margin, 0, 0, 0);
		ui->widget_options->layout()->setContentsMargins(0, 0, margin, 0);
	} else if (w >= minW) {
		int six = 6;
		int sixteen = 16;
		int margin = 38;
		ui->widget->layout()->setSpacing(sixteen);
		ui->widget_draw->layout()->setSpacing(sixteen);
		ui->widget_options->layout()->setSpacing(sixteen);
		ui->horizontalLayout_5->setSpacing(six);
		ui->horizontalLayout_3->setSpacing(six);
		ui->horizontalLayout_4->setSpacing(six);
		ui->horizontalLayout_7->setSpacing(six);
		ui->widget->layout()->setContentsMargins(margin, 0, 0, 0);
		ui->widget_options->layout()->setContentsMargins(0, 0, margin, 0);
	}
}

bool PLSDrawPenView::eventFilter(QObject *watcher, QEvent *event)
{
	if (watcher == shapePopup && event->type() == QEvent::Hide) {
		if (!ui->pushButton_ShapeOpen->rect().contains(ui->pushButton_ShapeOpen->mapFromGlobal(QCursor::pos()))) {
			ui->pushButton_ShapeOpen->setChecked(false);
		}

		ui->pushButton_ShapeOpen->setProperty("open", true);
		pls_flush_style(ui->pushButton_ShapeOpen);
	}

	if (watcher == widthPopup && event->type() == QEvent::Hide) {
		QRect rect = ui->pushButton_Width->rect();
		if (!rect.contains(ui->pushButton_Width->mapFromGlobal(QCursor::pos()))) {
			ui->pushButton_Width->setChecked(false);
		}
	}
	if (watcher == colorPopup && event->type() == QEvent::Hide) {
		QRect rect = ui->pushButton_Color->rect();
		if (!rect.contains(ui->pushButton_Color->mapFromGlobal(QCursor::pos()))) {
			ui->pushButton_Color->setChecked(false);
		}
	}

	return QFrame::eventFilter(watcher, event);
}
