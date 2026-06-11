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
	QString resourceUrl;
};

class TextMotionRemoteDataHandler : public QObject {
	Q_OBJECT
public:
	static TextMotionRemoteDataHandler *instance();
	void getTMRemoteDataRequest(const QString &url);
	const inline QMap<int, QString> &getTemplateNames() const { return m_templateTabs; }
	bool initTMData(const std::function<void(bool)> &callback);
	void parseTMData();

signals:
	void loadedTextmotionRes(bool isSuccess, bool isUpdate);

private:
	explicit TextMotionRemoteDataHandler();
	~TextMotionRemoteDataHandler() override = default;

	QString m_url;
	QByteArray m_templateJson;
	QMap<int, QString> m_templateTabs;
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
	QMap<QString, QPointer<QButtonGroup>> m_templateButtons;
};

#endif // TEXTMOTIONTEMPLATEDATAHELPER_H
