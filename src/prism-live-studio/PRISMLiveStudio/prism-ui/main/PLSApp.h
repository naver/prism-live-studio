#ifndef PLSAPP_H
#define PLSAPP_H
#include "obs-app.hpp"
#include "PLSIPCHandler.h"

#define RUNAPP_API_PATH QStringLiteral("/prismpc/runapp")
#define FAILREASON QStringLiteral("failReason")
#define SUCCESSFAIL QStringLiteral("successFail")

using EventFilterFunc = std::function<bool(QObject *, QEvent *)>;

class PLSEventFilter : public QObject {
	Q_OBJECT

public:
	explicit PLSEventFilter(QObject *parent, const EventFilterFunc &filter_) : QObject(parent), filter(filter_) {}

protected:
	bool eventFilter(QObject *obj, QEvent *event) override { return filter(obj, event); }

private:
	EventFilterFunc filter;
};

class PLSApp : public OBSApp {
	Q_OBJECT
public:
	explicit PLSApp(int &argc, char **argv, profiler_name_store_t *store);
	~PLSApp();
	void AppInit();
	bool PLSInit();
	inline bool isAppRunning() const { return appRunning; }
	inline void setAppRunning(bool appRunning_) { appRunning = appRunning_; }
	static PLSApp *plsApp() { return static_cast<PLSApp *>(qApp); }
	bool HotkeyEnable() const;
	void backupSceneCollectionConfig() const;
	const char *getProjectName() const;
	const char *getProjectName_kr() const;
	inline config_t *CookieConfig() const { return cookieConfig; }

	inline config_t *NaverShoppingConfig() const { return naverShoppingConfig; }
	void clearNaverShoppingConfig();
	bool notify(QObject *obj, QEvent *evt) override;
	bool event(QEvent *event) override;
	void initSideBarWindowVisible() const;
	static void setAnalogBaseInfo(QJsonObject &obj, bool isUploadHardwareInfo = false);
	static void uploadAnalogInfo(const QString &apiPath, const QVariantMap &paramInfos, bool isUploadHardwareInfo = false);
	static void uploadChatWidgetAnalogInfo(int styleId, int fontSize);
	static void uploadTextMotionAnalogInfo(int templateId, const QString &fontColor, const QString &fontFamily, int motion, int motionSpeed);
	static int runProgram(PLSApp &program, int argc, char *argv[], ScopeProfiler &prof);

public slots:
	void sessionExpiredhandler() const;

protected:
	bool eventFilter(QObject *obj, QEvent *event) override;

signals:
	bool AppNotify(void *obj, void *evt);

private:
	void InitCrashConfigDefaults() const;

private:
	bool appRunning = false;
	ConfigFile cookieConfig;
	ConfigFile naverShoppingConfig;
	bool m_isDirectLauncher = true;
};

#endif // PLSAPP_H
