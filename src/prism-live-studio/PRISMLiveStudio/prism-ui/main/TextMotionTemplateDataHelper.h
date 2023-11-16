#ifndef TEXTMOTIONTEMPLATEDATAHELPER_H
#define TEXTMOTIONTEMPLATEDATAHELPER_H
#include <QMap>
#include <QObject>
#include <QString>

#include "pls-net-url.hpp"
#include "frontend-api.h"

class TextMotionTemplateButton;
class QPushButton;

using TextMotionTemplateData = struct TextMotionTemplateData {
	QString id;
	QString name;
	QString resourcePath;
	QString resourceBackupPath;
	QString resourceUrl;
};

class TextMotionRemoteDataHandler : public QObject {
	Q_OBJECT
public:
	static TextMotionRemoteDataHandler *instance();
	void getTMRemoteDataRequest(const QString &url);
	const inline QMap<QString, QVector<TextMotionTemplateData>> &getTMRemoteData() const { return m_templateInfos; }
	const inline QMap<int, QString> &getTemplateNames() const { return m_templateTabs; }
	bool initTMData();

signals:
	void loadedTextmotionRes(bool isSuccess);

private:
	explicit TextMotionRemoteDataHandler() = default;
	~TextMotionRemoteDataHandler() override = default;
	void downloadTextmotionRes(const QJsonArray &fileList);
	void parseTMData();

	QString m_url;
	QByteArray m_templateJson;
	QMap<int, QString> m_templateTabs;
	QMap<QString, QVector<TextMotionTemplateData>> m_templateInfos; //key=type,value=TMDatas
	QMap<QString, QString> m_readyDownloadUrl;
	QStringList m_textTemplateNameList;
};

class TextMotionTemplateDataHelper : public QObject, public ITextMotionTemplateHelper {
	Q_OBJECT

public:
	void initTemplateButtons() override;
	QMap<int, QString> getTemplateNames() override;
	QButtonGroup *getTemplateButtons(const QString &templateName) override;
	void resetButtonStyle() override;
	QString findTemplateGroupStr(const int &templateId) override;
	int getDefaultTemplateId() override;
	QStringList getTemplateNameList() override;
	void removeParent();

	static ITextMotionTemplateHelper *instance()
	{
		static TextMotionTemplateDataHelper dataHelper;
		return static_cast<ITextMotionTemplateHelper *>(&dataHelper);
	}

private:
	explicit TextMotionTemplateDataHelper(QObject *parent = nullptr);
	~TextMotionTemplateDataHelper() override = default;

	void initTemplateGroup();
private slots:
	void updateButtonsStyle() const;

private:
	QMap<QString, QButtonGroup *> m_templateButtons;
};

#endif // TEXTMOTIONTEMPLATEDATAHELPER_H
