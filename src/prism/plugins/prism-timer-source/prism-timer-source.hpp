#ifndef PRISMSTICKERSOURCE_HPP
#define PRISMSTICKERSOURCE_HPP

#include <obs-module.h>
#include <stdio.h>
#include <util/dstr.h>
#include <graphics/image-file.h>
#include <util/platform.h>
#include <vector>
#include <QString>
#include <QPair>
#include <QThread>
#include <QObject>

enum class MusicStatus { Noraml, Started, Paused, Resume, Stoped };
enum class TemplateType { One = 0, Two, Three, Four };
enum class TimerType { Current = 0, Live, Countdown, Stopwatch };

struct obs_source_audio;

struct PLSColorData {
	struct color_data {
		const char *name{};
		const char *path{};
		const char *colorValue;
	};

	static PLSColorData *instance();

	void initColorDatas();

	std::vector<color_data> m_vecColors_t2{};
	std::vector<color_data> m_vecColors_t4{};
	std::vector<QPair<const char *, const char *>> m_vecMusicPath{};
};

struct timer_data {
	/* source and settings don't need call release method */
	obs_source_t *source{};
	obs_data_t *settings{};
	obs_properties_t *m_props{};

	gs_texture_t *web_source_tex = nullptr;
	obs_source_t *web = nullptr;
	obs_source_t *sub_music = nullptr;

	obs_hotkey_id start_hotkey;
	obs_hotkey_id cancel_hotkey;
	MusicStatus mStaus = MusicStatus::Noraml;
};

class AudioConvertThread : public QThread {
public:
	AudioConvertThread();

public:
	void startConvertAudio(obs_source_t *source, obs_source_audio &audio);

protected:
	void run() override;
};

class timer_source : public QObject {
public:
	timer_data config;
	timer_source(obs_source_t *source, obs_data_t *settings);
	~timer_source();

	void timer_source_destroy();
	void timer_source_update();
	void dispatahNoramlJsToWeb();
	void dispatahControlJsToWeb();
	QByteArray toJsonStr();
	void init_music_source();
	void onlyStopMusicIfNeeded();
	void timer_source::webDidReceiveMessage(const char *msg);

	void updateControlButtons(MusicStatus status, bool isForce = false, bool isNeedUpdate = true);
	MusicStatus getStatus(const QString &name, bool &isGot);
	QString getControlEvent();

	void propertiesEditStart();
	void propertiesEditEnd(bool isSaveClick);
	void getControlButtonTextAndEnable(int idx, QString &text, bool &enable);
	bool getStartButtonHightlight();

	void updateOKButtonEnable();

	void updateWebAudioShow(bool isShow);
	long long getCountTime();

	bool getListenButtonEnable();
	const char *getColorTitle();

	void didClickDefaultsButton();

	void resetTextToDefaultWhenEmpty();

public:
	bool isChangeControlByHand = false;
	bool isPropertiesOpened = false;
	AudioConvertThread *audioThread{};
};

#endif // PRISMSTICKERSOURCE_HPP
