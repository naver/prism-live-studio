#include "PLSAPIAfreecaTV.h"
#include <qfileinfo.h>
#include <QNetworkCookieJar>
#include <vector>
#include "ChannelCommonFunctions.h"
#include "PLSChannelDataAPI.h"
#include "PLSPlatformAfreecaTV.h"
#include "PLSPlatformApi.h"
#include "frontend-api.h"
#include "pls-common-define.hpp"
#include "pls-gpop-data.hpp"
#include "common/PLSAPICommon.h"
#include <QTextDocument>
#include <qregularexpression.h>
using namespace std;

PLSAPIAfreecaTV::PLSAPIAfreecaTV(QObject *parent) : QObject(parent) {}

void PLSAPIAfreecaTV::configDefaultRequest(const pls::http::Request &_request, const QObject *receiver, const PLSAPICommon::dataCallback &onSucceed, const PLSAPICommon::errorCallback &onFailed,
					   bool forceKo)
{
	addCommonData(_request, forceKo);
	_request.timeout(PRISM_NET_REQUEST_TIMEOUT)
		.receiver(receiver)
		.okResult([onSucceed, receiver](const pls::http::Reply &reply) {
			if (onSucceed && !pls_get_app_exiting()) {
				auto _data = reply.data();
				pls_async_call_mt(receiver, [onSucceed, _data] { onSucceed(_data); });
			}
		})
		.failResult([onFailed, receiver](const pls::http::Reply &reply) {
			if (onFailed && !pls_get_app_exiting()) {
				auto _code = reply.statusCode();
				auto _data = reply.data();
				auto _err = reply.error();
				pls_async_call_mt(receiver, [onFailed, _code, _data, _err] { onFailed(_code, _data, _err); });
			}
		});
}

void PLSAPIAfreecaTV::requestUsersInfoAndChannel(const QObject *receiver, const PLSAPICommon::dataCallback &onSucceed, const PLSAPICommon::errorCallback &onFailed)
{
	PLS_INFO(MODULE_PlatformService, "requestUsersInfoAndChannel start");

	const auto _request = pls::http::Request(pls::http::NoDefaultRequestHeaders);
	PLSAPIAfreecaTV::configDefaultRequest(_request, receiver, onSucceed, onFailed);

	_request.method(pls::http::Method::Get).url(g_plsAfreecaTVChannelInfo);

	pls::http::request(_request);
}

void PLSAPIAfreecaTV::requestUsersNickName(const QString &userID, const QObject *receiver, const PLSAPICommon::dataCallback &onSucceed, const PLSAPICommon::errorCallback &onFailed)
{
	PLS_INFO(MODULE_PlatformService, "requestUsersNickName start");
	QString url = g_plsAfreecaTVUserNick.arg(userID);

	const auto _request = pls::http::Request(pls::http::NoDefaultRequestHeaders);
	PLSAPIAfreecaTV::configDefaultRequest(_request, receiver, onSucceed, onFailed);

	_request.method(pls::http::Method::Get).url(url).withLog(PLSAPICommon::maskingUrlKeys(url, {userID}));

	pls::http::request(_request);
}

void PLSAPIAfreecaTV::requestDashboradData(const QObject *receiver, const PLSAPICommon::dataCallback &onSucceed, const PLSAPICommon::errorCallback &onFailed)
{
	PLS_INFO(MODULE_PlatformService, "requestDashboradData start");
	const auto _request = pls::http::Request(pls::http::NoDefaultRequestHeaders);
	PLSAPIAfreecaTV::configDefaultRequest(_request, receiver, onSucceed, onFailed, true);

	_request.method(pls::http::Method::Get).url(g_plsAfreecaTVDashboard);

	pls::http::request(_request);
}

void PLSAPIAfreecaTV::requestCategoryList(const QObject *receiver, const PLSAPICommon::dataCallback &onSucceed, const PLSAPICommon::errorCallback &onFailed)
{
	PLS_INFO(MODULE_PlatformService, "requestCategoryList start");
	QString url = g_plsAfreecaTVCategories.arg(IS_ENGLISH() ? "en_US" : "ko_KR");

	const auto _request = pls::http::Request(pls::http::NoDefaultRequestHeaders);
	PLSAPIAfreecaTV::configDefaultRequest(_request, receiver, onSucceed, onFailed);

	_request.method(pls::http::Method::Get).url(url);

	pls::http::request(_request);
}

void PLSAPIAfreecaTV::updateLiveInfo(const QObject *receiver, const QString &title, const PLSAPICommon::dataCallback &onSucceed, const PLSAPICommon::errorCallback &onFailed)
{
	/*
work		live_update
frmCategory	00020010
is_wait		Y
waiting_time	5
frmTitle	000000000000
frmViewer	777
frmWaterMark	6
frmHashTags	ahahaha,popopopo
frmByeBye	878984998
encode_type	normal
frmStreamKey	abby0816-2146523166
*/
	PLS_INFO(MODULE_PlatformService, "updateLiveInfo start");
	const auto &data = PLS_PLATFORM_AFREECATV->getSelectData();

	QHash<QString, QString> object;
	object["work"] = data.work;
	object["frmCategory"] = data.frmCategoryID;
	object["is_wait"] = data.is_wait;
	if (data.b_showFrmWait) {
		object["waiting_time"] = data.frmWaitTime;
		if (PLS_PLATFORM_API->isLiving()) {
			object["frmWait"] = data.frmWait;
			object["frmWaitTime"] = data.frmWaitTime; //not call living
		}
	}
	object["frmTitle"] = title;
	object["frmViewer"] = data.frmViewer;
	object["frmWaterMark"] = data.frmWaterMark;
	object["frmHashTags"] = data.frmHashTags;
	object["frmByeBye"] = data.frmByeBye;
	object["encode_type"] = data.encode_type;
	object["frmStreamKey"] = data.frmStreamKey;
	if (data.b_frmAdult) {
		object["frmAdult"] = data.frmAdult;
	}
	if (data.b_frmHidden) {
		object["frmHidden"] = data.frmHidden;
	}

	if (data.b_frmTuneOut) {
		object["frmTuneOut"] = data.frmTuneOut;
	}
	if (data.b_containFrmAccess) {
		object["frmAccess"] = data.frmAccess;
		object["frmAccessCode"] = data.frmAccessCode;
	}
	const auto _request = pls::http::Request(pls::http::NoDefaultRequestHeaders);
	PLSAPIAfreecaTV::configDefaultRequest(_request, receiver, onSucceed, onFailed);

	_request.method(pls::http::Method::Post).url(g_plsAfreecaTVUpdate).form(object);

	pls::http::request(_request);
}

void PLSAPIAfreecaTV::addCommonData(const pls::http::Request &builder, bool forceKo)
{
	builder.rawHeader("Cookie", PLS_PLATFORM_AFREECATV->getChannelCookie());
	QString acceptLanguage = "ko;q=0.9,en;q=0.8,zh;q=0.7,ja;q=0.6";

	if (IS_ENGLISH() && !forceKo) {
		acceptLanguage = pls_get_current_accept_language();
	}
	builder.rawHeader("Accept-Language", acceptLanguage);
}

void PLSAPIAfreecaTV::readDataByRegu(PLSAfreecaTVLiveinfoData &data, const QString &originStr)
{
	QString htmlStr = originStr;
	QString compareS = "";
	QString regTemp("name=\"%1\"([\\s\\S]*)>");
	QString frmWaitTimeReg("<select[ ]*name=\"frmWaitTime\"([\\s\\S]*)</select>");
	QString frmByeByeReg("name=\"frmByeBye\"([\\s\\S]*)</textarea>");
	QString tagsReg("var[ ]*szTags[ ]*=([\\s\\S]*);");
	QString serverUrlReg("((id=\"ServerUrl\"))([\\s\\S]*)(</p>)");

	compareS = getFirstReg(htmlStr, regTemp.arg("work"));
	data.work = getValue(compareS);

	compareS = getFirstReg(htmlStr, regTemp.arg("frmCategory"));
	data.frmCategoryID = getValue(compareS);

	compareS = getFirstReg(htmlStr, regTemp.arg("is_wait"));
	data.is_wait = getValue(compareS);

	compareS = getFirstReg(htmlStr, regTemp.arg("frmWait"));
	data.frmWait = getValue(compareS);
	data.b_showFrmWait = data.frmWait == "Y";

	compareS = getFirstReg(htmlStr, regTemp.arg("waiting_time"));
	data.waiting_time = getValue(compareS);

	compareS = getFirstReg(htmlStr, regTemp.arg("frmTitle"));
	data.frmTitle = decodeHtmlContent(getValue(compareS));

	compareS = getFirstReg(htmlStr, regTemp.arg("frmTuneOut"));
	data.b_frmTuneOut = getIsCheck(htmlStr, compareS);
	data.frmTuneOut = getValue(compareS);

	compareS = getFirstReg(htmlStr, regTemp.arg("frmAdult"));
	data.b_frmAdult = getIsCheck(htmlStr, compareS);
	data.frmAdult = getValue(compareS);

	compareS = getFirstReg(htmlStr, regTemp.arg("frmHidden"));
	data.b_frmHidden = getIsCheck(htmlStr, compareS);
	data.frmHidden = getValue(compareS);

	compareS = getFirstReg(htmlStr, frmWaitTimeReg);
	data.frmWaitTime = getValueOfList(compareS);

	compareS = getFirstReg(htmlStr, regTemp.arg("frmViewer"));
	data.frmViewer = getValue(compareS);

	compareS = getFirstReg(htmlStr, regTemp.arg("frmWaterMark"));
	data.frmWaterMark = getValue(compareS);

	compareS = getFirstReg(htmlStr, tagsReg);
	data.frmHashTags = getValueOfTags(compareS);

	compareS = getFirstReg(htmlStr, frmByeByeReg);
	data.frmByeBye = getValueOfFrmByeBye(compareS);

	compareS = getFirstReg(htmlStr, regTemp.arg("frmStreamKey"));
	data.frmStreamKey = decodeHtmlContent(getValue(compareS));

	compareS = getFirstReg(htmlStr, serverUrlReg);
	data.frmServerUrl = getValueOfFrmByeBye(compareS);

	compareS = getFirstReg(htmlStr, regTemp.arg("frmAccess"));
	data.b_containFrmAccess = compareS.contains("checked");
	if (data.b_containFrmAccess) {
		data.frmAccess = getValue(compareS);
		compareS = getFirstReg(htmlStr, regTemp.arg("frmAccessCode"));
		data.frmAccessCode = decodeHtmlContent(getValue(compareS));
	}
}

void PLSAPIAfreecaTV::parseCategory(const QString &originStr)
{
	QString convertStr = "";
	QString oriStr = originStr;
	oriStr.remove(QRegularExpression("^var[ ]*szBroadCategory[ ]*=[ ]*"));
	oriStr.remove(QRegularExpression(";*$"));

	auto doc = QJsonDocument::fromJson(oriStr.toUtf8());
	if (doc.isNull()) {
		return;
	}

	QJsonObject jsonObject = doc.object();
	vector<PLSAfreecaTVCategory> vecCategory;
	auto categories = jsonObject["CHANNEL"].toObject()["BROAD_CATEGORY"].toArray();
	recursiveConvertCategory(categories, vecCategory);
	if (!vecCategory.empty()) {
		PLS_PLATFORM_AFREECATV->setCategories(vecCategory);
	}
}

QString PLSAPIAfreecaTV::getFirstReg(const QString &htmlStr, const QString &regStr)
{
	/*InvertedGreedinessOption*/
	QString pattern(regStr);
	QRegularExpression rx(pattern);
	rx.setPatternOptions(QRegularExpression::InvertedGreedinessOption);
	QRegularExpressionMatch match = rx.match(htmlStr);
	QString compareS = "";
	if (match.hasMatch()) {
		compareS = match.captured(0);
	}
	qDebug() << "compareS" << compareS;
	return compareS;
}

QString PLSAPIAfreecaTV::getValue(const QString &originStr, const QString &keyStr)
{
	QString pattern = QString("%1=\"[^\"]+").arg(keyStr);
	QRegularExpression rx(pattern);
	QRegularExpressionMatch match = rx.match(originStr);
	QString compareS = "";
	if (match.hasMatch()) {
		compareS = match.captured(0);
	}
	qDebug() << "compareS" << compareS;
	QString removeStr = QString("%1=\"").arg(keyStr);
	compareS.remove(removeStr);
	return compareS;
}

QString PLSAPIAfreecaTV::getValueOfList(const QString &originStr)
{
	QString selectValue = "";
	QStringList list = originStr.split("/option");
	for (int i = 0; i < list.size(); i++) {
		QString subList = list[i];
		QString subValue = getValue(subList);
		if (subList.contains("selected")) {
			selectValue = subValue;
		}
	}
	return selectValue;
}

QString PLSAPIAfreecaTV::getValueOfTags(const QString &originStr)
{
	QString selectValue = "";
	QStringList list = originStr.split("'");
	if (list.size() > 1) {
		selectValue = list[1];
	}
	return selectValue;
}

QString PLSAPIAfreecaTV::getValueOfFrmByeBye(const QString &originStr)
{
	QString selectValue = "";
	QStringList list = originStr.split(">");
	if (list.size() > 1) {
		selectValue = list[1].split("<")[0];
	}
	return selectValue;
}

QString PLSAPIAfreecaTV::getServerUrlValue(const QString &originStr)
{
	QString selectValue = "";
	QStringList list = originStr.split(">");
	if (list.size() > 1) {
		selectValue = list[1];
	}
	return selectValue;
}

bool PLSAPIAfreecaTV::getIsCheck(const QString &htmlStr, const QString &getIdStr)
{
	auto idValue = getValue(getIdStr, "id");
	QString regTemp("label[ ]*for=\"%1\"([\\s\\S]*)>");
	auto classStr = getFirstReg(htmlStr, regTemp.arg(idValue));
	return classStr.contains("class=\"on\"");
}

void PLSAPIAfreecaTV::recursiveConvertCategory(const QJsonArray &categories, vector<PLSAfreecaTVCategory> &recieveData)
{
	for (const auto &category : categories) {
		auto dic = category.toObject();
		PLSAfreecaTVCategory data;
		data.cate_name = dic["cate_name"].toString();
		data.cate_no = dic["cate_no"].toString();
		data.ucc_cate = dic["ucc_cate"].toString();
		data.display = dic["display"].toString();
		auto subItems = dic["child"].toArray();
		if (!subItems.isEmpty()) {
			recursiveConvertCategory(subItems, data.child);
		}
		recieveData.push_back(data);
	}
}

QString PLSAPIAfreecaTV::getSelectCategoryString(const QString &selectID)
{
	const auto &categories = PLS_PLATFORM_AFREECATV->getCategories();
	QStringList allLevel;
	recursiveFindSelectStr(selectID, allLevel, categories);
	auto str = allLevel.join(" > ");
	return str;
}

void PLSAPIAfreecaTV::requestCheckIsOnline(const QString &userID, const QObject *receiver, const PLSAPICommon::dataCallback &onSucceed, const PLSAPICommon::errorCallback &onFailed)
{

	PLS_INFO(MODULE_PlatformService, "requestCheckIsOnline start");

	QString url = QString(g_plsAfreecaTVShareUrl_living).arg(userID);

	const auto _request = pls::http::Request(pls::http::NoDefaultRequestHeaders);
	PLSAPIAfreecaTV::configDefaultRequest(_request, receiver, onSucceed, onFailed, false);
	_request.method(pls::http::Method::Get).url(url).withLog(PLSAPICommon::maskingUrlKeys(url, {userID}));

	pls::http::request(_request);
}

bool PLSAPIAfreecaTV::getIsRemoteOnline(const QString &originStr, int &oId)
{
	QString htmlStr = originStr;
	QString tagsReg("var[ ]*nBroadNo[ ]*=([\\s\\S]*);");

	QString compareS = getFirstReg(htmlStr, tagsReg);
	compareS = compareS.remove(QRegularExpression("var[ ]*nBroadNo[ ]*="));
	compareS = compareS.remove(";");
	compareS = compareS.replace(" ", "");

	bool isOK = false;
	int _oidInt = compareS.toInt(&isOK);
	oId = _oidInt;
	return isOK && _oidInt != 0;
}

bool PLSAPIAfreecaTV::recursiveFindSelectStr(const QString &selectID, QStringList &allLevel, const vector<PLSAfreecaTVCategory> &recieveData)
{
	bool isFind = false;
	for (const auto &data : recieveData) {
		auto tempList = allLevel;
		if (data.cate_no == selectID) {
			tempList.append(data.cate_name);
			allLevel = tempList;
			isFind = true;
			break;
		}

		if (!data.child.empty()) {
			tempList.append(data.cate_name);
			isFind = recursiveFindSelectStr(selectID, tempList, data.child);
			if (isFind) {
				allLevel = tempList;
				break;
			}
		}
	}
	return isFind;
}

QString PLSAPIAfreecaTV::decodeHtmlContent(const QString &str)
{
	QTextDocument text;
	text.setHtml(str);
	return text.toPlainText();
}
