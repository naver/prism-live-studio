#pragma once

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include "pls-common-define.hpp"

class PLSLabel;
class PLSTimerDisplay : public QWidget {
	Q_OBJECT
public:
	enum class TimerType {
		TimerLive = 0,
		TimerRecord,
		TimerRehearsal,
	};
	enum class EventType {
		None = 0,
		LiveStatusChanged,
		RecordStatusChanged,
		StudioModeSatusChanged,
	};
	explicit PLSTimerDisplay(TimerType type, bool showTime, QWidget *parent);

	void SetTimerType(TimerType type);
	void OnStatus(bool isStarted);
	void OnEvents(EventType eventType, bool on);
	uint getStartTime() const { return startTime; };

protected:
	void timerEvent(QTimerEvent *e) override;

	QString FormatTimeString(uint sec) const;
	void UpdateProperty(bool isStarted);
	void OnTimerActivate(bool activate);

private:
	bool started = false;
	bool showTime = false;
	uint startTime = 0; // UNIX time in seconds
	int timerID = 0;
	QLabel *title;
	QLabel *time;
};

class PLSBasic;
class QHBoxLayout;
class PLSPreviewEditLabel : public QFrame {
	Q_OBJECT
public:
	explicit PLSPreviewEditLabel(bool center, QWidget *parent = nullptr);
	void SetSceneText(const QString &sceneName);
	void SetSceneName(const QString &sceneName);
	void SetDisplayText(int maxWidth);
	void SetSceneNameVisible(bool visible);
	void SetStudioModeStatus(bool isStudioMode);
	void SetDrawPenModeStatus(bool isDrawPen, bool isStudioMode);

private:
	QLabel *editLabel = nullptr;
	QLabel *sceneNameLabel = nullptr;
	QLabel *drawPenLabel = nullptr;
	QString sceneName;
};

class PLSPreviewLiveLabel : public QFrame {
	Q_OBJECT
public:
	explicit PLSPreviewLiveLabel(bool center, bool showTime = false, QWidget *parent = nullptr);
	void SetSceneText(const QString &sceneName);
	void SetSceneName(const QString &sceneName);
	void SetDisplayText(int maxWidth);
	void SetSceneNameVisible(bool visible);
	void SetLiveStatus(bool isStarted);
	void SetRecordStatus(bool isStarted);
	void SetStudioModeStatus(bool isStudioMode);
	uint GetStartTime() const;

protected:
	void resizeEvent(QResizeEvent *event) override;

signals:
	void resizeSignal();

private:
	QLabel *sceneNameLabel = nullptr;
	PLSTimerDisplay *liveUI = nullptr;
	QLabel *spaceIcon = nullptr;
	PLSTimerDisplay *recordUI = nullptr;
	QString sceneName;
};

class PLSPreviewProgramTitle : public QFrame {
	Q_OBJECT
	friend class PLSBasic;

public:
	explicit PLSPreviewProgramTitle(bool studioPortraitLayout, QWidget *parent = nullptr);

	void SetStudioPortraitLayout(bool studioPortraitLayout);

	void setEditTitle(const QString &sceneName);
	void setLiveTitle(const QString &sceneName);
	void setApplyEnabled(bool enable);
	void toggleShowHide(bool visible);
	void OnPreviewSceneChanged(const QString &sceneName);
	QFrame *GetEditArea();
	QFrame *GetLiveArea();
	QFrame *GetTotalArea();
	void OnLiveStatus(bool isStarted);
	void OnRecordStatus(bool isStarted);
	void OnStudioModeStatus(bool isStudioMode);
	void OnDrawPenModeStatus(bool isDrawPen, bool isStudioMode);
	uint getStartTime() const;
	void CustomResize();
	void CustomResizeAsync();
	void ShowSceneNameLabel(bool show);

private:
	void setStudioPortraitLayoutUI();
	QPushButton *CreateApplyBtn(const QString &objName, QWidget *parent);
	void CreateUI();

private:
	QWidget *parent = nullptr;
	PLSPreviewLiveLabel *horLiveArea = nullptr;
	PLSPreviewLiveLabel *porLiveArea = nullptr;

	PLSPreviewEditLabel *horEditArea = nullptr;
	PLSPreviewEditLabel *porEditArea = nullptr;

	QFrame *horizontalArea = nullptr;
	QPushButton *porApplyBtn = nullptr;
	QPushButton *horApplyBtn = nullptr;

	bool studioPortraitLayout = false;
};