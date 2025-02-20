#include "pls-common-define.hpp"
#include "frontend-api.h"
#include "PLSAction.h"
#include "PLSApp.h"
#include "pls/pls-source.h"
#include "log/log.h"
#define SOURCEANALOG "SourceAnalog"

extern QString getMotionStyleString(int style);
using AnalogConvertDataFunc = std::function<QVariant(const QVariant &data)>;
struct SourceAnalogData {
	enum class DataType {
		String = 1,
		Int,
		Bool,
		Double,
	};
	explicit SourceAnalogData(DataType type, const QString &_settingKey, const QString &_analogKey, const AnalogConvertDataFunc &func)
		: dataType(type), settingKey(_settingKey), analogKey(_analogKey), convertDataFunc(func)
	{
	}
	explicit SourceAnalogData(DataType type, const QString &_settingKey) : SourceAnalogData(type, _settingKey, "", nullptr) {}
	explicit SourceAnalogData(DataType type, const QString &_settingKey, const AnalogConvertDataFunc &func) : SourceAnalogData(type, _settingKey, "", func) {}
	explicit SourceAnalogData(DataType type, const QString &_settingKey, const QString &_analogKey) : SourceAnalogData(type, _settingKey, _analogKey, nullptr) {}

	DataType dataType;
	QString settingKey;
	QString analogKey;
	AnalogConvertDataFunc convertDataFunc = nullptr;
};
static QVariant chatV2Convert(const QVariant &data)
{
	auto id = data.toInt();
	switch (id % 10) {
	case 0:
		return QStringLiteral("Text");
	case 1:
		return QStringLiteral("Box");
	case 2:
		return QStringLiteral("Bubble");
	case 3:
		return QStringLiteral("Neon");
	case 4:
		return QStringLiteral("Dynamic");
	default:
		return QStringLiteral("");
	}
}
static QVariant timerClockTemplateConvert(const QVariant &data)
{
	switch (data.toInt()) {
	case 0:
		return QStringLiteral("basic");
	case 1:
		return QStringLiteral("round");
	case 2:
		return QStringLiteral("flip");
	case 3:
		return QStringLiteral("message");
	case 4:
		return QStringLiteral("tech");
	case 5:
		return QStringLiteral("second");
	default:
		assert("need add new type");
		return QStringLiteral("");
	}
}
static QVariant timerClockMoldConvert(const QVariant &data)
{
	switch (data.toInt()) {
	case 0:
		return QStringLiteral("clock");
	case 1:
		return QStringLiteral("liveTimer");
	case 2:
		return QStringLiteral("flip");
	case 3:
		return QStringLiteral("countDown");
	case 4:
		return QStringLiteral("countUp");
	default:
		assert("need add new type");
		return QStringLiteral("");
	}
}

static QMap<QString, QList<SourceAnalogData>> sourceKeyAndAnalogDatas = {
	{common::PRISM_CHATV2_SOURCE_ID,
	 {SourceAnalogData(SourceAnalogData::DataType::Int, "Chat.Template.List", "templateName", chatV2Convert),
	  SourceAnalogData(SourceAnalogData::DataType::String, "Chat.Font/chatFontFamily", "fontFamily")}}, //
	{common::PRISM_TEXT_TEMPLATE_ID,
	 {SourceAnalogData(SourceAnalogData::DataType::String, "templateName"), //
	  SourceAnalogData(SourceAnalogData::DataType::String, "TextMotion.Text/font-family", "fontFamily")}}, //
	{common::PRISM_TIMER_SOURCE_ID,
	 {SourceAnalogData(SourceAnalogData::DataType::Int, "template_list", "templateName", timerClockTemplateConvert), //
	  SourceAnalogData(SourceAnalogData::DataType::Int, "timer_type", "clockType", timerClockMoldConvert),
	  SourceAnalogData(SourceAnalogData::DataType::String, "font_famliy/font-family", "fontFamily")}} //
};

bool isEqual(const QVariant &oldData, const QVariant &newData, SourceAnalogData::DataType dataType)
{
	switch (dataType) {
	case SourceAnalogData::DataType::String:
		return oldData.toString() == newData.toString();
	case SourceAnalogData::DataType::Int:
		return oldData.toInt() == newData.toInt();
	case SourceAnalogData::DataType::Bool:
		return oldData.toBool() == newData.toBool();
	case SourceAnalogData::DataType::Double:
		return std::fabs(oldData.toDouble() - newData.toDouble()) < 1e-9;
	default:
		return true;
	}
}

QVariant getData(OBSData obj, const QString &key, SourceAnalogData::DataType dataType)
{
	switch (dataType) {
	case SourceAnalogData::DataType::String:
		return obs_data_get_string(obj, key.toUtf8().constData());
	case SourceAnalogData::DataType::Int:
		return obs_data_get_int(obj, key.toUtf8().constData());
	case SourceAnalogData::DataType::Bool:
		return obs_data_get_bool(obj, key.toUtf8().constData());
	case SourceAnalogData::DataType::Double:
		return obs_data_get_double(obj, key.toUtf8().constData());
	default:
		return QVariant();
	}
}
QVariant getData(OBSData obj, const QStringList &keys, SourceAnalogData::DataType dataType, bool isAssert = true, int index = 0)
{
	auto errorLog = [isAssert](const QStringList &keys) -> QVariant {
		if (isAssert) {
			PLS_ERROR(SOURCEANALOG, "cannot find value,please check key, key = %s", keys.join('/').toUtf8().constData());
			assert(false);
		}
		return QVariant();
	};
	if (keys.isEmpty()) {
		return errorLog(keys);
	}
	if (index + 1 < keys.size()) {
		if (OBSDataAutoRelease currentValue = obs_data_get_obj(obj, keys[index].toUtf8().constData()); currentValue) {
			return getData(currentValue.Get(), keys, dataType, isAssert, ++index);
		} else {
			return errorLog(keys);
		}

	} else {
		return getData(obj, keys[index], dataType);
	}
}

QPair<bool, QJsonObject> specialDataMatch(const char *sourceId, OBSData oldJsonData, OBSData newJsonData)
{
	switch (obs_source_get_icon_type(sourceId)) {
	case PLS_ICON_TYPE_TEXT_TEMPLATE:
		break;
	case PLS_ICON_TYPE_CHAT_TEMPLATE: {
		auto isNewCheckMotion = getData(newJsonData, {"Chat.Motion", "isCheckChatMotion"}, SourceAnalogData::DataType::Bool).toBool();
		if (isNewCheckMotion) {
			auto oldmotionStyle = getData(oldJsonData, {"Chat.Motion", "chatMotionStyle"}, SourceAnalogData::DataType::Int, false).toInt();
			auto isOldCheckMotion = getData(oldJsonData, {"Chat.Motion", "isCheckChatMotion"}, SourceAnalogData::DataType::Bool, false).toBool();
			auto newmotionStyle = getData(newJsonData, {"Chat.Motion", "chatMotionStyle"}, SourceAnalogData::DataType::Int).toInt();
			if (oldmotionStyle != newmotionStyle || !isOldCheckMotion) {
				return {true, {{"motionStyle", getMotionStyleString(newmotionStyle)}}};
			}
		}
	} break;
	default:
		break;
	}
	return {false, {}};
};

void sendPrismSourceAnalog(const char *sourceId, OBSData oldSettings, OBSData newSettings)
{
	auto SourceAnalogDatas = sourceKeyAndAnalogDatas.find(sourceId);
	if (SourceAnalogDatas == sourceKeyAndAnalogDatas.end())
		return;

	QJsonObject sendAnalogJson;
	bool isSendAnalog = false;

	auto callbackData = specialDataMatch(sourceId, oldSettings, newSettings);
	if (callbackData.first) {
		isSendAnalog = true;
		sendAnalogJson = callbackData.second;
	}

	for (auto analogData : SourceAnalogDatas.value()) {
		auto settingkeys = analogData.settingKey.split('/');
		auto newValue = getData(newSettings, settingkeys, analogData.dataType);
		sendAnalogJson.insert(analogData.analogKey.isEmpty() ? settingkeys.last() : analogData.analogKey,
				      analogData.convertDataFunc ? QJsonValue::fromVariant(analogData.convertDataFunc(newValue)) : QJsonValue::fromVariant(newValue));

		if (!isEqual(getData(oldSettings, settingkeys, analogData.dataType, false), newValue, analogData.dataType)) {
			isSendAnalog = true;
		}
	}
	if (isSendAnalog) {
		pls_send_analog(AnalogType::ANALOG_ADD_SOURCE, {{"sourceType", action::GetActionSourceID(sourceId)}, {"detail", sendAnalogJson}});
	}
}
