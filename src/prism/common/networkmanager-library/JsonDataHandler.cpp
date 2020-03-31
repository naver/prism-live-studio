#include "JsonDataHandler.h"
#include <QFile>

bool JsonDataHandler::isValidJsonFormat(const QByteArray &array)
{
	QJsonParseError jsonError;
	if (array.isEmpty()) {
		return false;
	} else {
		QJsonDocument::fromJson(array, &jsonError);
		if (QJsonParseError::NoError == jsonError.error) {
			return true;
		} else {
			return false;
		}
	}
}

bool JsonDataHandler::getValueFromByteArray(const QByteArray &byteArray, const QString &key, QVariant &value)
{
	if (isValidJsonFormat(byteArray)) {
		return getValue(QJsonDocument::fromJson(byteArray).object(), key, value);
	} else {
		return false;
	}
}

bool JsonDataHandler::getValuesFromByteArray(const QByteArray &byteArray, const QString &key, QJsonObject &object)
{
	if (isValidJsonFormat(byteArray)) {
		return getValues(QJsonDocument::fromJson(byteArray).object(), key, object);
	} else {
		return false;
	}
}

bool JsonDataHandler::getValuesFromByteArray(const QByteArray &byteArray, const QString &key, QVariantList &value)
{
	if (isValidJsonFormat(byteArray)) {
		return getValues(QJsonDocument::fromJson(byteArray).object(), key, value);
	} else {
		return false;
	}
}

QByteArray JsonDataHandler::getJsonByteArrayFromMap(const QVariantMap &map)
{
	QJsonDocument doc;
	QJsonObject obj;
	QVariantMap::const_iterator it;
	for (it = map.constBegin(); it != map.end(); ++it) {
		obj.insert(it.key(), it.value().toJsonValue());
	}
	doc.setObject(obj);
	return doc.toJson();
}

QByteArray JsonDataHandler::getJsonByteArrayFromList(const QVariantList &value)
{
	return QJsonDocument(QJsonArray::fromVariantList(value)).toJson();
}

bool JsonDataHandler::getValue(const QJsonObject &jsonObject, const QString &key, QVariant &value)
{
	for (auto tem : jsonObject.keys()) {
		QJsonValue v = jsonObject.value(tem);
		if (v.isObject()) {
			getValue(jsonObject.value(tem).toObject(), key, value);
		} else {
			if (key == tem) {
				value = v.toVariant();
				return true;
			} else {
				continue;
			}
		}
	}
	return false;
}

bool JsonDataHandler::getValues(const QJsonObject &jsonObject, const QString &key, QVariantList &values)
{
	for (auto tem : jsonObject.keys()) {
		QJsonValue v = jsonObject.value(tem);
		if (v.isObject()) {
			getValues(jsonObject.value(tem).toObject(), key, values);
		} else if (v.isArray()) {
			if (key == tem) {
				values = v.toArray().toVariantList();
				return true;
			} else {
				getArray(v.toArray(), key, values);
			}
		} else {
			continue;
		}
	}
	return false;
}

bool JsonDataHandler::getValues(const QJsonObject &jsonObject, const QString &key, QJsonObject &object)
{
	for (auto tem : jsonObject.keys()) {
		if (tem == key) {
			QJsonValue v = jsonObject.value(tem);
			if (v.isObject()) {
				object = v.toObject();
				return true;
			} else {
				continue;
			}
		} else {
			continue;
		}
	}
	return false;
}
void JsonDataHandler::getArray(const QJsonArray &array, const QString &key, QVariantList &values)
{
	for (auto arr : array) {
		if (arr.isObject()) {
			getValues(arr.toObject(), key, values);
		} else if (arr.isArray()) {
			getArray(arr.toArray(), key, values);
		} else {
			continue;
		}
	}
}
