#include "TextMotionTemplateDataHelper.h"
#include "TextMotionTemplateDataHelper.h"
#include "pls-common-define.hpp"
#include "frontend-api.h"
#include "TextMotionTemplateButton.h"
#include "liblog.h"
#include "log/log.h"
#include <qbuttongroup.h>
#include <qdir.h>
#include "utils-api.h"
#include "PLSAlertView.h"
#include "PLSApp.h"
#include "network-state.h"
#include "PLSBasic.h"
#include "qversionnumber.h"

using namespace common;
#define PRISM_TM_TEMPLATE_PATH QStringLiteral("PRISMLiveStudio/textmotion/textmotion.json")
constexpr auto PRISM_TEXT_TEMPLATE = "text-template";
constexpr auto PRISM_TEXT_TEMPLATE_DEFAULT_ID = 1600;
static void copyTextTemplateResFromPackage()
{
	QString dstWebPath = pls::rsm::getAppDataPath("textTemplatePC/web");
#if defined(Q_OS_WIN)
	QString srcWebPath = pls_get_dll_dir("libresource") + "/../../data/prism-plugins/prism-text-template-source/web";
#elif defined(Q_OS_MACOS)
	QString srcWebPath = pls_get_dll_dir("prism-text-template-source") + "/web";
#endif
	auto isSuccess = pls_copy_dir(srcWebPath, dstWebPath);
	PLS_INFO("libresource", "copyTextmotionWeb %s", isSuccess ? "success" : "failed");
}
struct CategoryTextTemplate : public pls::rsm::ICategory {
	PLS_RSM_CATEGORY(CategoryTextTemplate)
	QString categoryId(pls::rsm::IResourceManager *mgr) const override { return PLS_RSM_CID_TEXT_TEMPLATE; }
	void jsonDownloaded(pls::rsm::IResourceManager *mgr, const pls::rsm::DownloadResult &result) override { copyTextTemplateResFromPackage(); }
	void jsonLoaded(pls::rsm::IResourceManager *mgr, pls::rsm::Category category) override { TextMotionRemoteDataHandler::instance()->parseTMData(); }
	bool checkItem(pls::rsm::IResourceManager *mgr, pls::rsm::Item item) const override
	{
		if (auto group = item.groups().front(); group.groupId() == QStringLiteral("SCREEN SAVER")) {
			return QDir(pls::rsm::getAppDataPath(QString("textTemplatePC/web/static/screen_img"))).exists();
		}
		return true;
	}
	void getItemDownloadUrlAndHowSaves(pls::rsm::IResourceManager *mgr, std::list<pls::rsm::UrlAndHowSave> &urlAndHowSaves, pls::rsm::Item item) const override
	{
		PLS_INFO(moduleName(), "getItemDownloadUrlAndHowSaves textTemplatePC %s", item.itemId().toUtf8().constData());

		auto lang = pls_prism_get_locale() != "ko-KR" ? "en" : "ko";
		urlAndHowSaves.push_back(pls::rsm::UrlAndHowSave() //
						 .names({QStringLiteral("properties"), QStringLiteral("mediaProperties"), QStringLiteral("thumbnail"), lang, QStringLiteral("url")})
						 .fileName(pls::rsm::FileName::FromUrl));

		urlAndHowSaves.push_back(pls::rsm::UrlAndHowSave() //
						 .names({QStringLiteral("properties"), QStringLiteral("mediaProperties"), QStringLiteral("preview"), lang, QStringLiteral("url")})
						 .fileName(pls::rsm::FileName::FromUrl)
						 .needDecompress(true)
						 .decompress([](const pls::rsm::UrlAndHowSave &urlAndHowSave, const auto &filePath) {
							 QFileInfo fileInfo(filePath);
							 if (fileInfo.suffix() == "zip") {
								 return pls::rsm::unzip(filePath, pls::rsm::getAppDataPath("textTemplatePC/web/static/screen_img"), false);
							 }
							 return true;
						 }));
	}
	void allDownload(pls::rsm::IResourceManager *mgr, bool ok) override
	{
		PLS_INFO(moduleName(), "allDownload textTemplatePC");
		pls_async_call_mt([this, ok]() {
			pls_check_app_exiting();
			if (PLSBasic::instance() && PLSBasic::instance()->Get()->GetPropertiesWindow()) {
				if (ok) {
					PLS_INFO(moduleName(), "textmoiton update success");
					TextMotionRemoteDataHandler::instance()->loadedTextmotionRes(ok, true);
				} else {

					auto ret = pls_show_download_failed_alert(nullptr);
					if (ret == PLSAlertView::Button::Ok) {
						PLS_INFO(moduleName(), "textmotion: User select retry download.");
						pls_async_call_mt([this]() { download(); });
					} else {
						PLS_INFO(moduleName(), "textmotion: User select quit download.");
						TextMotionRemoteDataHandler::instance()->loadedTextmotionRes(true, false);
					}
				}
			}
		});
	}
};

TextMotionTemplateDataHelper::TextMotionTemplateDataHelper(QObject *parent) : QObject(parent) {}

void TextMotionTemplateDataHelper::initTemplateGroup()
{
	m_templateButtons.clear();
	for (auto group : CategoryTextTemplate::instance()->getGroups()) {
		auto buttonGroup = pls_new<QButtonGroup>();
		buttonGroup->setExclusive(false);
		m_templateButtons.insert(group.groupId().toLower(), buttonGroup);
	}
}

void TextMotionTemplateDataHelper::initTemplateButtons()
{
	auto checkGroup = [this](const QString &key) -> QPointer<QButtonGroup> {
		auto group = m_templateButtons.value(key);
		if (!group) {
			group = pls_new<QButtonGroup>();
			group->setExclusive(false);
			m_templateButtons.insert(key, group);
		}
		return group;
	};

	if (m_templateButtons.isEmpty()) {
		initTemplateGroup();
	}
	for (auto group = m_templateButtons.begin(); group != m_templateButtons.end(); ++group) {
		auto groupTmp = checkGroup(group.key());
		while (groupTmp->buttons().size()) {
			groupTmp->removeButton(groupTmp->buttons().value(0));
		}
	}

	for (auto group : CategoryTextTemplate::instance()->getGroups()) {
		auto buttonGroup = checkGroup(group.groupId().toLower());

		auto items = group.items();
		for (auto item : items) {
			if (auto versionLimit = item.attr({"properties", "versionLimit"}).toString(); !versionLimit.isEmpty()) {
				if (auto matched =
					    pls_check_version(versionLimit.toUtf8(), QVersionNumber(pls_get_prism_version_major(), pls_get_prism_version_minor(), pls_get_prism_version_patch()));
				    !matched) {
					PLS_WARN(PRISM_TEXT_TEMPLATE, "item: %s, version: %s is a wrong format", qUtf8Printable(item.itemId()), qUtf8Printable(versionLimit));
					continue;
				} else if (!matched.value()) {
					PLS_INFO(PRISM_TEXT_TEMPLATE, "item: %s, version: %s cannot be supported", qUtf8Printable(item.itemId()), qUtf8Printable(versionLimit));
					continue;
				}
			}

			TextMotionTemplateButton *button = pls_new<TextMotionTemplateButton>();
			auto id = item.itemId();
			button->setTemplateText(id);
			int idInt = id.split('_').last().toInt();
			button->setProperty("ID", idInt);
			connect(button, &TextMotionTemplateButton::clicked, this, &TextMotionTemplateDataHelper::updateButtonsStyle);
			button->setGroupName(group.groupId().toLower());
			button->attachGifResource(item.file(0));
			buttonGroup->addButton(button, idInt);
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
TextMotionRemoteDataHandler::TextMotionRemoteDataHandler()
{
	parseTMData();
}
void TextMotionRemoteDataHandler::getTMRemoteDataRequest(const QString &url)
{
	PLS_INFO(PRISM_TEXT_TEMPLATE, "start requeset text template data.");
	m_url = url;
}

bool TextMotionRemoteDataHandler::initTMData(const std::function<void(bool)> &callback)
{
	pls_async_invoke([callback]() {
		QString dstWebPath = pls::rsm::getAppDataPath("textTemplatePC/web");
		QDir dir(dstWebPath);
		if (!dir.exists()) {
			copyTextTemplateResFromPackage();
			auto textmotionPath = PLS_RSM_getLibraryPolicyPC_Path(QStringLiteral("Library_Policy_PC/textmotion/web"));
			auto isSuccess = pls_copy_dir(textmotionPath, dstWebPath);
			PLS_INFO(PRISM_TEXT_TEMPLATE, "copytextmotion res from library to textmotion dir  %s", isSuccess ? "success" : "failed");
		}
		pls::rsm::getResourceManager()->checkCategory(PLS_RSM_CID_TEXT_TEMPLATE, [callback](pls::rsm::Category category, bool ok) {
			pls_async_call_mt([ok, callback]() {
				if (!ok)
					CategoryTextTemplate::instance()->download();
				callback(ok);
			});
		});
	});
	return true;
}

void TextMotionRemoteDataHandler::parseTMData()
{
	int index = 0;
	for (auto group : CategoryTextTemplate::instance()->getGroups()) {
		QString name = group.groupId().toLower();
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
	auto groups = CategoryTextTemplate::instance()->getGroups();
	return groups.empty() ? PRISM_TEXT_TEMPLATE_DEFAULT_ID : groups.front().items().front().itemId().section('_', 1, 1).toInt();
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
