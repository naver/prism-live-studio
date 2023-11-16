#pragma once

#include <obs.hpp>
#include <QJsonObject>
#include <string>
#include <QObject>
#include <QTimer>

enum class OperationType {
	None = 0,
	OT_InitApp = (1 << 0),
	OT_StartStreaming = (1 << 1),
	OT_StopStreaming = (1 << 2),
	OT_StartRecording = (1 << 3),
	OT_StopRecording = (1 << 4),
};

class PLSInfoCollector : public QObject {
	Q_OBJECT
public:
	explicit PLSInfoCollector(QObject *parent = nullptr);
	~PLSInfoCollector() override;
	static void logMsg(std::string type, OBSOutput output = nullptr);
	static QString getFilterList(obs_source_t *source);
};
