#ifndef PLSSCENEITEMVIEW_H
#define PLSSCENEITEMVIEW_H

#include "qt-display.hpp"

#include <QFrame>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <iostream>
#include <vector>
#include <QResizeEvent>

template<typename PLSRef> struct SignalContainer {
	PLSRef ref;
	std::vector<std::shared_ptr<OBSSignal>> handlers;
};

enum class DisplayMethod { DynamicRealtimeView = 0, ThumbnailView, TextView };

namespace Ui {
class PLSSceneItemView;
}

class PLSSceneItemView;
class PLSSceneDisplay : public PLSQTDisplay {
	Q_OBJECT
public:
	explicit PLSSceneDisplay(QWidget *parent = nullptr);
	~PLSSceneDisplay();
	void SetRenderFlag(bool state);
	void SetCurrentFlag(bool state);
	void SetSceneData(OBSScene scene);
	void SetDragingState(bool state);
	bool GetRenderState() const;
	void visibleSlot(bool visible) override;
	void CustomCreateDisplay();
	void SetRefreshThumbnail(bool refresh);
	void SetSceneDisplayMethod(DisplayMethod displayMethod);
	void AddRenderCallback();
	void RemoveRenderCallback();

protected:
	void mousePressEvent(QMouseEvent *event) override;
	void enterEvent(QEvent *event) override;
	void leaveEvent(QEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;

private:
	static void RenderScene(void *data, uint32_t cx, uint32_t cy);

private slots:
	void OnDisplayCreated();
	void CaptureImageFinished(const QImage &image);
	void CaptureSceneImageFinished(const QImage &image);

signals:
	void MouseLeftButtonClicked();
	void RenderChanged(bool state);
	void CaptureImageFinishedSignal(const QImage &image);
	void CaptureSceneImageSignal(const QImage &image);

private:
	bool ready{false};
	bool render{false};
	bool current{false};
	bool isDraging{false};
	bool toplevelVisible{true};
	bool displayAdd{false};

	OBSScene scene;
	PLSSceneItemView *parent{};
	QPoint startPos{};

	// scene display method
	bool refreshThumbnail{true};
	DisplayMethod displayMethod{0};
	obs_display_t *curDisplay{nullptr};
};

class PLSSceneItemView : public QFrame {
	Q_OBJECT
public:
	explicit PLSSceneItemView(const QString &name, OBSScene scene, DisplayMethod displayMethod, QWidget *parent = nullptr);
	~PLSSceneItemView();
	void SetData(OBSScene scene);
	void SetSignalHandler(SignalContainer<OBSScene> handler);
	void SetCurrentFlag(bool state = true);
	void SetName(const QString &name);
	void SetRenderFlag(bool state);
	void CustomCreateDisplay();
	void SetSceneDisplayMethod(DisplayMethod displayMethod);
	DisplayMethod GetSceneDisplayMethod();
	void ResizeCustom();

	OBSScene GetData();
	QString GetName();
	SignalContainer<OBSScene> GetSignalHandler();
	bool GetCurrentFlag();
	void RefreshSceneThumbnail();

public slots:
	void OnRenameOperation();
	void OnCurrentItemChanged(bool state);

protected:
	virtual void mousePressEvent(QMouseEvent *event) override;
	virtual void mouseMoveEvent(QMouseEvent *event) override;
	virtual void mouseDoubleClickEvent(QMouseEvent *event) override;
	virtual bool eventFilter(QObject *object, QEvent *event) override;
	virtual void resizeEvent(QResizeEvent *event) override;
	virtual void enterEvent(QEvent *event) override;
	virtual void leaveEvent(QEvent *event) override;

private slots:
	void OnMouseButtonClicked();
	void OnModifyButtonClicked();
	void OnDeleteButtonClicked();
	void OnFinishingEditName(bool cancel);
	void SetContentMargins(bool state);
	void OnCaptureImageFinished(const QImage &image);
	void OnRefreshSceneThumbnail(const QImage &image);

private:
	void setEnterPropertyState(bool state, QWidget *widget);
	void CreateDrag(const QPoint &startPos, const QImage &image);
	QString GetNameElideString();

	// grid mode
	void RenameWithGridMode();
	void EnterEventWithGridMode();
	void LeaveEventWithGridMode();

	// list mode
	void RenameWithListMode();
	void EnterEventWithListMode();
	void LeaveEventWithListMode();

signals:
	void CurrentItemChanged(bool state);
	void MouseButtonClicked(PLSSceneItemView *item);
	void ModifyButtonClicked(PLSSceneItemView *item);
	void DeleteButtonClicked(PLSSceneItemView *item);
	void FinishingEditName(const QString &name, PLSSceneItemView *item);

private:
	Ui::PLSSceneItemView *ui;
	QPushButton *deleteBtn{};
	OBSScene scene;
	QString name;
	bool current{false};
	bool isFinishEditing{true};
	SignalContainer<OBSScene> handler;
	QPoint startPos;
	DisplayMethod displayMethod{0};
};

#endif // PLSSCENEITEMVIEW_H
