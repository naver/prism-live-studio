#ifndef PLSDRAWPENVIEW_H
#define PLSDRAWPENVIEW_H

#include <QFrame>
#include <QButtonGroup>
#include <QGridLayout>
#include "obs.hpp"

namespace Ui {
class PLSDrawPenView;
}

enum class DrawTypeIndex { DTI_PEN = 0, DTI_HIGHLIGHHTER, DTI_GLOW_PEN, DTI_SHAPE, DTI_RUBBER };

class PLSDrawPenView : public QFrame {
	Q_OBJECT

public:
	explicit PLSDrawPenView(QWidget *parent = nullptr);
	~PLSDrawPenView() final;

	bool IsDrawPenMode() const;
	void UpdateView(OBSScene scene, bool reset = false);

private:
	void createCustomGroup(QButtonGroup *&group, QGridLayout *&gLayout, QString name, int row, int colum) const;
	void setViewEnabled(bool visible);
	void shapeGroupButtonChangedInternal(int index);

private slots:
	void drawGroupButtonChanged(int index, bool);
	void shapeGroupButtonChanged(int index);
	void widthGroupButtonChanged(int index);
	void colorGroupButtonChanged(int index);

	void onPenClicked() const;
	void onHighlighterClicked() const;
	void onGlowPenClicked() const;
	void onShapeClicked() const;
	void onRubberClicked() const;

	void on_pushButton_ShapeOpen_clicked();
	void on_pushButton_Width_clicked();
	void on_pushButton_Color_clicked();
	void on_pushButton_Undo_clicked() const;
	void on_pushButton_Redo_clicked() const;
	void on_pushButton_Clear_clicked() const;
	void on_pushButton_Visible_clicked();

	void OnUndoDisabled(bool disabled);
	void OnRedoDisabled(bool disabled);

protected:
	void showEvent(QShowEvent *event) override;
	void hideEvent(QHideEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;
	bool eventFilter(QObject *watcher, QEvent *event) override;

private:
	Ui::PLSDrawPenView *ui;

	bool drawVisible = true;
	QButtonGroup *drawGroup = nullptr;
	QWidget *shapePopup = nullptr;
	QWidget *widthPopup = nullptr;
	QWidget *colorPopup = nullptr;
};

#endif // PLSDRAWPENVIEW_H
