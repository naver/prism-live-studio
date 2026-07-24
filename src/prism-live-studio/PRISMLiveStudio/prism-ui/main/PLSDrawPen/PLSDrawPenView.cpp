#include "PLSDrawPenView.h"
#include "PLSBasic.h"
#include "ui_PLSDrawPenView.h"
#include "window-basic-main.hpp"
#include "PLSDrawPenMgr.h"
#include "prism-ui/log/module_names.h"
#include "pls-performance.h"
#include <liblog.h>
#include <QButtonGroup>
#include <QRadioButton>
#include <QBitmap>
#include <QPainter>

static const std::vector<std::string> shapeTips{"drawpen.toolbar.arrow.toolTip", "drawpen.toolbar.line.toolTip", "drawpen.toolbar.rect.toolTip", "drawpen.toolbar.round.toolTip",
						"drawpen.toolbar.triangle.toolTip"};

PLSDrawPenView::PLSDrawPenView(QWidget *parent) : QFrame(parent)
{
	pls_add_css(this, {"PLSDrawPenView"});

	ui = pls_new<Ui::PLSDrawPenView>();
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

	ui->pushButton_Exit->setText(QTStr("drawpen.toolbar.exit.text"));

	ui->pushButton_CurShape->setProperty("style", 0);
	ui->pushButton_Color->setProperty("style", 0);
	ui->pushButton_ShapeOpen->setProperty("open", true);

	ui->pushButton_ShapeOpen->setCheckable(true);
	ui->pushButton_Width->setCheckable(true);
	ui->pushButton_Color->setCheckable(true);

	ui->pushButton_Undo->setDisabled(true);
	ui->pushButton_Redo->setDisabled(true);

	pls_uistep_v2_set_name(ui->pushButton_Pen, "NormalPen");
	pls_uistep_v2_set_name(ui->pushButton_Highlighter, "Highlighter");
	pls_uistep_v2_set_name(ui->pushButton_GlowPen, "GlowPen");
	pls_uistep_v2_set_name(ui->pushButton_CurShape, "CurShape");
	pls_uistep_v2_set_name(ui->pushButton_ShapeOpen, "ShapeOpen");
	pls_uistep_v2_set_name(ui->pushButton_Rubber, "Rubber");
	pls_uistep_v2_set_name(ui->pushButton_Width, "Width");
	pls_uistep_v2_set_name(ui->pushButton_Color, "Color");
	pls_uistep_v2_set_name(ui->pushButton_Undo, "Undo");
	pls_uistep_v2_set_name(ui->pushButton_Redo, "Redo");
	pls_uistep_v2_set_name(ui->pushButton_Clear, "Clear");
	pls_uistep_v2_set_name(ui->pushButton_Visible, "Visible");
	pls_uistep_v2_set_name(ui->pushButton_Exit, "Exit");

	connect(PLSDrawPenMgr::Instance(), &PLSDrawPenMgr::UndoDisabled, this, &PLSDrawPenView::OnUndoDisabled);
	connect(PLSDrawPenMgr::Instance(), &PLSDrawPenMgr::RedoDisabled, this, &PLSDrawPenView::OnRedoDisabled);

	pls_uistep_v2_set_title(this, QStringLiteral("DrawPen View"));
	pls_uistep_v2_set_value(ui->pushButton_Visible, [this]() { return ui->pushButton_Visible->property("visibled").toBool() ? QStringLiteral("Hide") : QStringLiteral("Show"); });
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
		pls_uistep_v2_set_title(button, QStringLiteral("DrawPen View"));
		pls_uistep_v2_set_value(button, suffix);
	}
}

void PLSDrawPenView::updateGroupState(QWidget *parentWidget, int selected)
{
	if (!parentWidget) {
		assert(false);
		return;
	}

	QGridLayout *gridLayout = dynamic_cast<QGridLayout *>(parentWidget->layout());
	if (!gridLayout) {
		assert(false);
		return;
	}

	int rowCount = gridLayout->rowCount();
	int columnCount = gridLayout->columnCount();
	for (int row = 0; row < rowCount; ++row) {
		for (int col = 0; col < columnCount; ++col) {
			QLayoutItem *item = gridLayout->itemAtPosition(row, col);
			if (!item) {
				assert(false);
				continue;
			}

			QWidget *widget = item->widget();
			if (!widget) {
				assert(false);
				continue;
			}

			QPushButton *btn = dynamic_cast<QPushButton *>(widget);
			if (!btn) {
				assert(false);
				continue;
			}

			int index = row * columnCount + col;
			QString newState = (index == selected) ? "selected" : "";
			if (btn->property("state") != newState) {
				btn->setProperty("state", newState);
				pls_flush_style(btn);
			}
		}
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
}

void PLSDrawPenView::UpdateView(OBSScene scene, bool reset)
{
	PLS_PERFORMANCE_FUNCTION();
	isUpdatingView = true;

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

	isUpdatingView = false;
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
	if (!isUpdatingView) {
		PLS_UI_ACTION("PLSDrawPenView shape select start, index: %d", index);
	}
	ui->pushButton_CurShape->setChecked(true);
	PLSDrawPenMgr::Instance()->SetCurrentDrawType(DrawType::DT_2DSHAPE);
	shapeGroupButtonChangedInternal(index);
	if (!isUpdatingView) {
		PLS_UI_ACTION("PLSDrawPenView shape select end, index: %d", index);
	}
}

void PLSDrawPenView::widthGroupButtonChanged(int index)
{
	if (!isUpdatingView) {
		PLS_UI_ACTION("PLSDrawPenView thickness select start, index: %d", index);
	}
	PLSDrawPenMgr::Instance()->SetLineWidthIndex(index);

	if (widthPopup) {
		widthPopup->hide();
	}

	if (!isUpdatingView) {
		PLS_UI_ACTION("PLSDrawPenView thickness select end, index: %d", index);
	}
}

void PLSDrawPenView::colorGroupButtonChanged(int index)
{
	if (!isUpdatingView) {
		PLS_UI_ACTION("PLSDrawPenView color select start, index: %d", index);
	}
	PLSDrawPenMgr::Instance()->SetColorIndex(index);
	ui->pushButton_Color->setProperty("style", index);
	pls_flush_style(ui->pushButton_Color);
	if (colorPopup) {
		colorPopup->hide();
	}
	if (!isUpdatingView) {
		PLS_UI_ACTION("PLSDrawPenView color select end, index: %d", index);
	}
}

void PLSDrawPenView::onPenClicked() const
{
	if (ui->pushButton_Pen->isChecked()) {
		if (!isUpdatingView) {
			PLS_UI_ACTION("PLSDrawPenView pen update start");
		}
		PLSDrawPenMgr::Instance()->SetCurrentDrawType(DrawType::DT_PEN);
		if (!isUpdatingView) {
			PLS_UI_ACTION("PLSDrawPenView pen update end");
		}
	}
}

void PLSDrawPenView::onHighlighterClicked() const
{
	if (ui->pushButton_Highlighter->isChecked()) {
		if (!isUpdatingView) {
			PLS_UI_ACTION("PLSDrawPenView highlight update start");
		}
		PLSDrawPenMgr::Instance()->SetCurrentDrawType(DrawType::DT_HIGHLIGHTER);
		if (!isUpdatingView) {
			PLS_UI_ACTION("PLSDrawPenView highlight update end");
		}
	}
}

void PLSDrawPenView::onGlowPenClicked() const
{
	if (ui->pushButton_GlowPen->isChecked()) {
		if (!isUpdatingView) {
			PLS_UI_ACTION("PLSDrawPenView glow update start");
		}
		PLSDrawPenMgr::Instance()->SetCurrentDrawType(DrawType::DT_GLOW_PEN);
		if (!isUpdatingView) {
			PLS_UI_ACTION("PLSDrawPenView glow update end");
		}
	}
}

void PLSDrawPenView::onShapeClicked() const
{
	if (ui->pushButton_CurShape->isChecked()) {
		if (!isUpdatingView) {
			PLS_UI_ACTION("PLSDrawPenView shape update start");
		}
		PLSDrawPenMgr::Instance()->SetCurrentDrawType(DrawType::DT_2DSHAPE);
		int index = ui->pushButton_CurShape->property("style").toInt();
		PLSDrawPenMgr::Instance()->SetCurrentShapeType(ShapeType(index));
		if (!isUpdatingView) {
			PLS_UI_ACTION("PLSDrawPenView shape update end");
		}
	}
}

void PLSDrawPenView::onRubberClicked() const
{
	if (ui->pushButton_Rubber->isChecked()) {
		if (!isUpdatingView) {
			PLS_UI_ACTION("PLSDrawPenView eraser update start");
		}
		PLSDrawPenMgr::Instance()->SetCurrentDrawType(DrawType::DT_RUBBER);
		if (!isUpdatingView) {
			PLS_UI_ACTION("PLSDrawPenView eraser update end");
		}
	}
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
		PLS_UI_ACTION("PLSDrawPenView shape menu show start");
		auto selected = get_shape_index(PLSDrawPenMgr::Instance()->GetCurrentShapeType());
		updateGroupState(shapePopup, selected);

		shapePopup->resize(162, 50);
		QPoint offset(-95, 4);
		QPoint p = ui->pushButton_ShapeOpen->mapToGlobal(QPoint(0, ui->pushButton_ShapeOpen->size().height())) + offset;
		shapePopup->move(p);
		shapePopup->show();
		ui->pushButton_ShapeOpen->setProperty("open", false);
		pls_flush_style(ui->pushButton_ShapeOpen);
		PLS_UI_ACTION("PLSDrawPenView shape menu show end");
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

	PLS_UI_ACTION("PLSDrawPenView thickness menu show start");
	auto selected = PLSDrawPenMgr::Instance()->GetLineWidthIndex();
	updateGroupState(widthPopup, selected);

	widthPopup->resize(162, 50);
	QPoint offset(-67, 4);
	QPoint p = ui->pushButton_Width->mapToGlobal(QPoint(0, ui->pushButton_Width->size().height())) + offset;
	widthPopup->move(p);
	widthPopup->show();
	PLS_UI_ACTION("PLSDrawPenView thickness menu show end");
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

	PLS_UI_ACTION("PLSDrawPenView color menu show start");
	auto selected = PLSDrawPenMgr::Instance()->GetColorIndex();
	updateGroupState(colorPopup, selected);

	colorPopup->resize(190, 78);
	QPoint offset(-81, 4);
	QPoint p = ui->pushButton_Color->mapToGlobal(QPoint(0, ui->pushButton_Color->size().height())) + offset;
	colorPopup->move(p);
	colorPopup->show();
	PLS_UI_ACTION("PLSDrawPenView color menu show end");
}

void PLSDrawPenView::on_pushButton_Undo_clicked() const
{
	PLS_UI_ACTION("PLSDrawPenView undo update start");
	PLSDrawPenMgr::Instance()->UndoStroke();
	PLS_UI_ACTION("PLSDrawPenView undo update end");
}

void PLSDrawPenView::on_pushButton_Redo_clicked() const
{
	PLS_UI_ACTION("PLSDrawPenView redo update start");
	PLSDrawPenMgr::Instance()->RedoStroke();
	PLS_UI_ACTION("PLSDrawPenView redo update end");
}

void PLSDrawPenView::on_pushButton_Clear_clicked() const
{
	PLSDrawPenMgr::Instance()->ClearStrokes();
}

void PLSDrawPenView::on_pushButton_Visible_clicked()
{
	drawVisible = !PLSDrawPenMgr::Instance()->DrawVisible();
	if (drawVisible) {
		pls_on_drawpen_event(PLSDrawPenMgr::Instance()->GetCurrentScene(), ACTION_SHOW);
	} else {
		pls_on_drawpen_event(PLSDrawPenMgr::Instance()->GetCurrentScene(), ACTION_HIDE);
	}

	PLS_UI_ACTION("PLSDrawPenView visible update start, visible: %s", drawVisible ? "true" : "false");
	PLSDrawPenMgr::Instance()->OnDrawVisible(drawVisible);
	setViewEnabled(drawVisible);
	PLS_UI_ACTION("PLSDrawPenView visible update end, visible: %s", drawVisible ? "true" : "false");

	pls_on_drawpen_updated(PLSDrawPenMgr::Instance()->GetCurrentScene());
}

void PLSDrawPenView::on_pushButton_Exit_clicked()
{
	PLS_UI_ACTION("PLSDrawPenView exit button click");
	pls_async_call(this, []() { PLSBasic::instance()->OnDrawPenClicked(); });
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
