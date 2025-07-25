#ifndef PLSCHATTEMPLATEDATAHELPER_H
#define PLSCHATTEMPLATEDATAHELPER_H
#include <QMap>
#include <QString>

#include "pls-net-url.hpp"
#include "frontend-api.h"
#include "PLSPropertiesExtraUI.hpp"

class QPushButton;

struct PLSChatTemplateData {
	uint id;
	QString name;
	QString resourcePath;
	QString resourceBackupPath;
	QString backgroundColor;
	bool isPaid = false;
};

class PLSChatTemplateDataHelper : public QObject, public ITextMotionTemplateHelper {
	Q_OBJECT

public:
	void getChatTemplateFromsceneCollection(const QJsonArray &array);
	void initTemplateButtons() override;
	QMap<int, QString> getTemplateNames() override;
	QButtonGroup *getTemplateButtons(const QString &templateName) override;
	void resetButtonStyle() override;
	QString findTemplateGroupStr(const int &templateId) override;
	int getDefaultTemplateId() override;
	QStringList getTemplateNameList() override;
	void removeParent();
	QJsonObject defaultTemplateObj(const int itemId) override;
	void readCutsomPLSChatTemplateData();
	bool saveCustomObj(const OBSData &settings, const int itemId) override;
	QJsonArray getSaveTemplate() const override;
	QSet<QString> getChatTemplateName() const override;
	void updateCustomTemplateName(const QString &name, const int id) override;
	void removeCustomTemplate(const int id) override;
	QList<ITextMotionTemplateHelper::PLSChatDefaultFamily> getChatCustomDefaultFamily() override;
	void clearChatTemplateButton() override;
	QButtonGroup *getBKTemplateButtons() override;
	void checkChatBkRes(const std::function<void(bool)> &callback);
	void initchatBKTemplateButtons();
	void allDownloadedChatBKRes(pls::rsm::Category category, bool isOk);
	static void raisePropertyView();

	static ITextMotionTemplateHelper *instance()
	{
		static PLSChatTemplateDataHelper dataHelper;
		return &dataHelper;
	}
signals:
	void loadechatWidgetBKRes(bool isSuccess, bool isUPdate);

private:
	explicit PLSChatTemplateDataHelper();
	~PLSChatTemplateDataHelper() override = default;
	void initPLSChatTemplateData();
	void initChatDefaultFamilyData(const QJsonArray &fontArray, const QString &langShort);
	void initTemplateGroup();
	QString getDefaultTitle();
	bool isPaidBkTemplate(int id);

private:
	uint m_defaultId = 0;
	QMap<int, QString> m_templateTabs;
	QMap<QString, QVector<PLSChatTemplateData>> m_templateInfos; //key=type,value=TMDatas
	QMap<QString, QPointer<QButtonGroup>> m_templateButtons;
	QMap<int, QJsonObject> m_chatTemplateObjs;
	QJsonArray m_needSaveChatTemplates;
	QSet<QString> m_chatTemplateNames;
	std::string m_currentTemplateTitle;
	QList<PLSChatDefaultFamily> m_chatDefaultfamilies;
	QPointer<QButtonGroup> m_chatBKTemplateGroup;
};

#endif // CHATTEMPLATEDATAHELPER_H
