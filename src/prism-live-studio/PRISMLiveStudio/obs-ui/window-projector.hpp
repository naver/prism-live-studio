#pragma once

#include <obs.hpp>
#include "qt-display.hpp"
#include "multiview.hpp"

enum class ProjectorType {
	Source,
	Scene,
	Preview,
	StudioProgram,
	Multiview,
	ChatBrowser,
};

class QMouseEvent;
class PLSDialogView;

class OBSProjector : public OBSQTDisplay {
	Q_OBJECT

private:
	OBSWeakSourceAutoRelease weakSource;
	std::vector<OBSSignal> sigs;

	static void OBSRenderMultiview(void *data, uint32_t cx, uint32_t cy);
	static void OBSRender(void *data, uint32_t cx, uint32_t cy);
	static void OBSSourceRenamed(void *data, calldata_t *params);
	static void OBSSourceDestroyed(void *data, calldata_t *params);
	void mousePressEvent(QMouseEvent *event) override;
	void mouseDoubleClickEvent(QMouseEvent *event) override;
	void closeEvent(QCloseEvent *event) override;

	bool isAlwaysOnTop;
	bool isAlwaysOnTopOverridden = false;
	int savedMonitor = -1;
	ProjectorType type = ProjectorType::Source;

	Multiview *multiview = nullptr;

	bool ready = false;

	void UpdateMultiview();
	void UpdateProjectorTitle(QString name);

	QRect prevGeometry;
	void SetMonitor(int monitor);

	QScreen *screen = nullptr;
	static QList<OBSProjector *> multiviewProjectors;
	bool isWindow;

signals:
	void notifyClose();
private slots:
	void EscapeTriggered();
	void OpenFullScreenProjector();
	void ResizeToContent();
	void OpenWindowedProjector();
	void AlwaysOnTopToggled(bool alwaysOnTop);
	void ScreenRemoved(QScreen *screen_);
	void RenameProjector(QString oldName, QString newName);

public:
	OBSProjector(QWidget *widget, obs_source_t *source_, int monitor, ProjectorType type_);
	~OBSProjector();

	OBSSource GetSource();
	ProjectorType GetProjectorType();
	int GetMonitor();
	static void UpdateMultiviewProjectors();
	void SetHideCursor();

	bool IsAlwaysOnTop() const;
	bool IsAlwaysOnTopOverridden() const;
	void SetIsAlwaysOnTop(bool isAlwaysOnTop, bool isOverridden);

	static void setParentDialogTitleBarButtons(PLSDialogView *parentDialg, bool isFullScreen);
	bool isVerticalPreview = false;
};
