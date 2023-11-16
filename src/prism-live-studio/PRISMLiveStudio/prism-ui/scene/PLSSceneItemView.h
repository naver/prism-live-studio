#ifndef PLSSCENEITEMVIEW_H
#define PLSSCENEITEMVIEW_H

#include "qt-display.hpp"
#include "PLSLabel.h"

#include <QFrame>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <iostream>
#include <vector>
#include <QResizeEvent>
#include <mutex>
#include <obs.hpp>

template<typename PLSRef> struct SignalContainer {
	PLSRef ref;
	std::vector<std::shared_ptr<OBSSignal>> handlers;
};

enum class DisplayMethod {
	DynamicRealtimeView = 0,
	ThumbnailView,
	TextView,
};
enum class MouseState {
	None,
	Normal,
	Hover,
	Pressed,
	Disabled,
};
enum class BadgeType {
	None,
	Edit,
	Live,
	Record,
	Rehearsal,
	Normal,
};

struct texture_info {
	gs_texture_t *texture = nullptr;
	double dpi = 1.0;
};

namespace Ui {
class PLSSceneItemView;
}

class PLSSceneItemView;
class PLSSceneDisplay : public OBSQTDisplay {
	Q_OBJECT
public:
	explicit PLSSceneDisplay(QWidget *parent = nullptr);
	~PLSSceneDisplay() final;
	PLSSceneDisplay(const PLSSceneDisplay &) = delete;
	PLSSceneDisplay &operator=(const PLSSceneDisplay &) = delete;
	void SetRenderFlag(bool state);
	void SetCurrentFlag(bool state);
	bool GetCurrentFlag();
	void SetSceneData(OBSScene scene);
	void SetDragingState(bool state);
	bool GetDragingState();
	bool GetRenderState();
	void visibleSlot(bool visible);
	void CustomCreateDisplay();
	void SetRefreshThumbnail(bool refresh);
	bool GetRefreshThumbnail();
	void SetSceneDisplayMethod(DisplayMethod displayMethod);
	bool TestSceneDisplayMethod(DisplayMethod displayMethod);
	void AddRenderCallback();
	void RemoveRenderCallback();
	void DrawOverlay();
	void DrawRadiusOverlay() const;
	void DrawDeleteBtn();
	void DrawSelectedBorder();
	void DrawBadgeIcons();
	void DrawSceneBackground() const;
	gs_texture_t *GetBtnTexture();
	gs_texture_t *GetBadgeTexture();
	void DrawThumbnail();
	void SaveSceneTexture(uint32_t cx, uint32_t cy);
	void DrawDefaultThumbnail() const;
	void SetBadgeType(BadgeType badgeType);
	BadgeType GetBadgeType();
	void GetRadiusTexture() const;
	QSize GetWidgetSize() override;
	void ResizeDisplay();

protected:
	void mousePressEvent(QMouseEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;
	void enterEvent(QEnterEvent *event) override;
	void leaveEvent(QEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void changeEvent(QEvent *event) override;

private:
	static void RenderScene(void *data, uint32_t cx, uint32_t cy);
	void UpdateMouseState(MouseState state);
	MouseState GetMouseState();
	static QImage GetSceneDisplayImagePath(void *data, uint32_t cx, uint32_t cy);
	static gs_texture_t *GetSceneDiaplayTexture(void *data, gs_texrender_t *texrender, uint32_t cx, uint32_t cy, uint32_t &width, uint32_t &height, enum gs_color_format &format,
						    bool needScale = false);
private slots:
	void OnDisplayCreated();
	void CaptureImageFinished(const QImage &image);

signals:
	void MouseLeftButtonClicked();
	void RenderChanged(bool state);
	void CaptureImageFinishedSignal(const QImage &image);
	void CaptureSceneImageSignal(const QImage &image);
	void DeleteBtnClicked();

private:
	bool render{false};
	bool isDraging{false};
	bool toplevelVisible{true};
	bool displayAdd{false};
	//Need to be protected for accessed by multi threads.
	bool current{false};
	MouseState mouseState{MouseState::None};
	bool btn_visible{false};
	std::atomic_bool isHover{false};
	std::mutex mutex;

	OBSScene scene;
	PLSSceneItemView *parent{};
	QPoint startPos{};

	// scene display method
	bool refreshThumbnail{true};
	DisplayMethod displayMethod{DisplayMethod::TextView};
	obs_display_t *curDisplay{nullptr};
	// static thumbnail texture
	gs_texture_t *item_texture{nullptr};
	// border radius texture
	static texture_info radius_texture;
	// default scene thumbnail texture
	static gs_texture_t *default_texture;
	// scene display reference count
	static int sceneDisplayCount;

	// Different button states texture
	static texture_info tex_del_btn_normal;
	static texture_info tex_del_btn_hover;
	static texture_info tex_del_btn_pressed;
	static texture_info tex_del_btn_disable;

	// live/record status textures
	static texture_info tex_edit_info;
	static texture_info tex_live_info;
	static texture_info tex_record_info;
	static texture_info tex_normal_info;

	BadgeType badgeType;
	QRectF rect_delete_btn;
};

class PLSSceneItemView : public QFrame {
	Q_OBJECT
public:
	explicit PLSSceneItemView(const QString &name, OBSScene scene, DisplayMethod displayMethod, QWidget *parent = nullptr);
	virtual ~PLSSceneItemView();

	void SetData(OBSScene scene);
	void SetSignalHandler(const SignalContainer<OBSScene> &handler);
	void SetCurrentFlag(bool state = true);
	void SetName(const QString &name);
	void SetRenderFlag(bool state) const;
	bool GetRenderFlag() const;
	void CustomCreateDisplay() const;
	void SetSceneDisplayMethod(DisplayMethod displayMethod);
	DisplayMethod GetSceneDisplayMethod() const;
	void ResizeCustom();

	OBSScene GetData() const;
	QString GetName() const;
	SignalContainer<OBSScene> GetSignalHandler() const;
	bool GetCurrentFlag() const;
	void RefreshSceneThumbnail() const;
	void RepaintDisplay();

	// set status badge
	void SetStatusBadge();

public slots:
	void OnRenameOperation();
	void OnCurrentItemChanged(bool state) const;

protected:
	void mousePressEvent(QMouseEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void mouseDoubleClickEvent(QMouseEvent *event) override;
	bool eventFilter(QObject *object, QEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;
	void enterEvent(QEnterEvent *event) override;
	void leaveEvent(QEvent *event) override;

private slots:
	void OnMouseButtonClicked();
	void OnModifyButtonClicked();
	void OnDeleteButtonClicked();
	void OnFinishingEditName(bool cancel);
	void SetContentMargins(bool state) const;
	void OnCaptureImageFinished(const QImage &image);

private:
	void setEnterPropertyState(bool state, QWidget *widget) const;
	void CreateDrag(const QPoint &startPos, const QImage &image);
	QString GetNameElideString() const;

	// grid mode
	void RenameWithGridMode() const;
	void EnterEventWithGridMode() const;
	void LeaveEventWithGridMode() const;
	std::string GetBadgeIconPath(BadgeType type) const;
	void DrawScreenShot(QPixmap &pixmap, const QImage &image);

	// list mode
	void RenameWithListMode() const;
	void EnterEventWithListMode() const;
	void LeaveEventWithListMode() const;

	void FlushBadgeStyle(BadgeType type) const;

signals:
	void CurrentItemChanged(bool state);
	void MouseButtonClicked(PLSSceneItemView *item);
	void ModifyButtonClicked(PLSSceneItemView *item);
	void DeleteButtonClicked(PLSSceneItemView *item);
	void FinishingEditName(const QString &name, PLSSceneItemView *item);

private:
	Ui::PLSSceneItemView *ui;
	OBSScene scene;
	QString name;
	bool current{false};
	bool isFinishEditing{true};
	SignalContainer<OBSScene> handler;
	QPoint startPos;
	DisplayMethod displayMethod{DisplayMethod::TextView};
	BadgeType badgeType;
};

#endif // PLSSCENEITEMVIEW_H
