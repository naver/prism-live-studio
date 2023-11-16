#include "TextMotionTemplateDataHelper.h"
#include "TextMotionTemplateDataHelper.h"
#include "pls-common-define.hpp"
#include "json-data-handler.hpp"
#include "frontend-api.h"
#include "TextMotionTemplateButton.h"
#include "liblog.h"
#include "log/log.h"
#include <qbuttongroup.h>
#include <qdir.h>
#include "utils-api.h"
#include "PLSResourceManager.h"
#include "PLSResCommonFuns.h"
#include "PLSAlertView.h"
#include "PLSApp.h"

using namespace common;
#define PRISM_TM_TEMPLATE_PATH QStringLiteral("PRISMLiveStudio/textmotion/textmotion.json")

constexpr auto PRISM_TEXT_TEMPLATE = "text-template";

TextMotionTemplateDataHelper::TextMotionTemplateDataHelper(QObject *parent) : QObject(parent) {}

void TextMotionTemplateDataHelper::initTemplateGroup()
{
	m_templateButtons.clear();

	const QMap<QString, QVector<TextMotionTemplateData>> &templateInfos = TextMotionRemoteDataHandler::instance()->getTMRemoteData();
	for (auto templateStr : templateInfos.keys()) {
		auto buttonGroup = pls_new<QButtonGroup>();
		buttonGroup->setExclusive(false);
		m_templateButtons.insert(templateStr, buttonGroup);
	}
}

void TextMotionTemplateDataHelper::initTemplateButtons()
{
	const QMap<QString, QVector<TextMotionTemplateData>> &templateInfos = TextMotionRemoteDataHandler::instance()->getTMRemoteData();
	if (m_templateButtons.isEmpty()) {
		initTemplateGroup();
	}
	for (auto group = m_templateButtons.begin(); group != m_templateButtons.end(); ++group) {
		auto groupTmp = group.value();
		while (groupTmp->buttons().size()) {
			groupTmp->removeButton(groupTmp->buttons().value(0));
		}
	}

	for (auto templateKey : templateInfos.keys()) {
		auto group = m_templateButtons.value(templateKey);
		const QVector<TextMotionTemplateData> templateDatas = templateInfos.value(templateKey);
		for (int index = 0; index != templateDatas.count(); ++index) {
			TextMotionTemplateData data = templateDatas.value(index);
			TextMotionTemplateButton *button = pls_new<TextMotionTemplateButton>();
			QString id = data.id;
			button->setTemplateText(id);
			int idInt = id.split('_').last().toInt();
			button->setProperty("ID", idInt);
			connect(button, &TextMotionTemplateButton::clicked, this, &TextMotionTemplateDataHelper::updateButtonsStyle);
			button->setTemplateData(data);
			button->setGroupName(templateKey);
			button->attachGifResource(data.resourcePath, data.resourceBackupPath, data.resourceUrl);
			group->addButton(button, idInt);
		}
	}
}

QMap<int, QString> TextMotionTemplateDataHelper::getTemplateNames()
{
	return TextMotionRemoteDataHandler::instance()->getTemplateNames();
}

void TextMotionTemplateDataHelper::updateButtonsStyle() const
{
	auto button = static_cast<TextMotionTemplateButton *>(sender());
	QString buttonGroupName = button->getGroupName();

	for (auto templateKey : m_templateButtons.keys()) {
		for (auto _button : m_templateButtons.value(templateKey)->buttons()) {
			if (_button == button && !_button->isChecked()) {
				_button->setChecked(true);
			} else if (_button->isChecked() && _button != button) {
				_button->setChecked(false);
			}
		}
	}
}

QButtonGroup *TextMotionTemplateDataHelper::getTemplateButtons(const QString &templateName)
{
	if (m_templateButtons.find(templateName.toLower()) != m_templateButtons.end()) {
		return m_templateButtons.value(templateName);
	}
	return nullptr;
}

void TextMotionTemplateDataHelper::resetButtonStyle()
{
	for (auto templateKey : m_templateButtons.keys()) {
		for (auto _button : m_templateButtons.value(templateKey)->buttons()) {
			_button->setChecked(false);
		}
	}
}

TextMotionRemoteDataHandler *TextMotionRemoteDataHandler::instance()
{
	static TextMotionRemoteDataHandler remoteDataHandler;
	return &remoteDataHandler;
}

void TextMotionRemoteDataHandler::getTMRemoteDataRequest(const QString &url)
{
	PLS_INFO(PRISM_TEXT_TEMPLATE, "start requeset text template data.");
	m_url = url;
}
void TextMotionRemoteDataHandler::downloadTextmotionRes(const QJsonArray &fileList)
{
	PLSResCommonFuns::downloadResources(
		fileList,
		[this, fileList](const QMap<QString, bool> &, bool isSuccess, const QJsonArray &) {
			if (isSuccess) {
				PLS_INFO(PRISM_TEXT_TEMPLATE, "textmoiton update success");
				parseTMData();
				emit loadedTextmotionRes(isSuccess);
			} else {
				pls_async_call_mt(this, [fileList, this]() {
					pls_check_app_exiting();
					auto ret = pls_show_download_failed_alert(nullptr);
					if (ret == PLSAlertView::Button::Ok) {
						PLS_INFO("textmoton", "textmotion: User select retry download.");
						pls_async_call_mt(this, [fileList, this]() { downloadTextmotionRes(fileList); });
					} else {
						PLS_INFO("textmoton", "textmotion: User select quit download.");
						emit loadedTextmotionRes(true);
					}
				});
			}
		},
		false);
}

bool TextMotionRemoteDataHandler::initTMData()
{
	m_templateInfos.clear();
	m_templateTabs.clear();
	m_readyDownloadUrl.clear();
	QJsonObject data;
	bool isSuccess = pls_read_json(data, pls_get_user_path(PRISM_TM_TEMPLATE_PATH));
	if (!isSuccess) {
		bool isRemove = QFile::remove(pls_get_user_path(PRISM_TM_TEMPLATE_PATH));
		PLS_INFO(PRISM_TEXT_TEMPLATE, "textmotion file parse error,delete file is %s", isRemove ? "success" : "failed");
	}
	auto needDownloadFileList = PLSResourceManager::instance()->getNeedDownloadResInfo("textmotion");
	if (needDownloadFileList.isEmpty()) {
		PLS_INFO(PRISM_TEXT_TEMPLATE, "textmoiton res no update");
		parseTMData();
		emit loadedTextmotionRes(true);
		return true;
	} else {
		downloadTextmotionRes(needDownloadFileList);
		return false;
	}
}

void TextMotionRemoteDataHandler::parseTMData()
{
	QDir appDir(QApplication::applicationDirPath());
#if defined(Q_OS_WIN)
	QString backupGifPath = appDir.absoluteFilePath("../../data/prism-studio/text_motion/");
#elif defined(Q_OS_MACOS)
	QString backupGifPath = pls_get_app_resource_dir() + ("/data/prism-studio/text_motion/");
#endif
	QString originLang = pls_get_current_language_short_str().toLower();
	PLSJsonDataHandler::getJsonArrayFromFile(m_templateJson, pls_get_user_path(PRISM_TM_TEMPLATE_PATH));
	QVariantList list;
	PLSJsonDataHandler::getValuesFromByteArray(m_templateJson, "group", list);
	int index = 0;
	for (auto map : list) {
		QString name = map.toMap().value(name2str(groupId)).toString().toLower();
		QJsonArray templateList = map.toMap().value(name2str(items)).toJsonArray();
		QVector<TextMotionTemplateData> templateDatas;
		QVector<QJsonObject> tempateJsons;
		for (auto jsonValue : templateList) {
			QVariantMap templateMap = jsonValue.toObject().toVariantMap();
			TextMotionTemplateData tmData;
			tmData.id = templateMap.value(name2str(itemId)).toString();
			tmData.name = templateMap.value(name2str(title)).toString();
			//get lang
			const auto &adjustAttributes = templateMap.value(name2str(properties)).toMap().value("mediaProperties").toMap().value("thumbnail").toMap();
			auto lang = originLang;
			if (adjustAttributes.find(lang) == adjustAttributes.end()) {
				lang = "en";
			}
			tmData.resourcePath = QString(pls_get_user_path(CONFIGS_USER_TEXTMOTION_PATH)).arg(QString("%1%2.gif").arg(lang).arg(tmData.name));
			tmData.resourceBackupPath = backupGifPath + QString("%1%2.gif").arg(lang).arg(tmData.name);
			QString gifUrl = adjustAttributes.value(lang).toMap().value(name2str(thumbnailUrl)).toString();
			tmData.resourceUrl = gifUrl;
			if (QFileInfo fileInfo(tmData.resourcePath); !fileInfo.exists()) {
				m_readyDownloadUrl.insert(gifUrl, tmData.resourcePath);
			}
			templateDatas.append(tmData);
			tempateJsons.append(jsonValue.toObject());
			//get gif url
		}
		m_templateInfos.insert(name, templateDatas);
		m_templateTabs.insert(index, name);
		++index;
	}
}
QStringList TextMotionTemplateDataHelper::getTemplateNameList()
{
	return TextMotionRemoteDataHandler::instance()->getTemplateNames().values();
}

QString TextMotionTemplateDataHelper::findTemplateGroupStr(const int &templateId)
{
	for (auto templateKey : m_templateButtons.keys()) {
		auto buttonGroup = m_templateButtons.value(templateKey)->buttons();
		for (auto _button : buttonGroup) {
			if (templateId == _button->property("ID").toInt()) {
				return templateKey;
			}
		}
	}
	return QString();
}

int TextMotionTemplateDataHelper::getDefaultTemplateId()
{
	if (!getTemplateNames().isEmpty()) {
		auto name = TextMotionRemoteDataHandler::instance()->getTemplateNames()[0];
		auto id = TextMotionRemoteDataHandler::instance()->getTMRemoteData().value(name)[0].id.section('_', 1, 1).toInt();
		return id;
	}
	return 0;
}

void TextMotionTemplateDataHelper::removeParent()
{
	for (auto templateKey : m_templateButtons.keys()) {
		auto buttonGroup = m_templateButtons.value(templateKey)->buttons();
		for (auto _button : buttonGroup) {
			_button->setParent(nullptr);
		}
	}
}
