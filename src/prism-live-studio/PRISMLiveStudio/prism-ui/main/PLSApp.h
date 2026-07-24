#ifndef PLSAPP_H
#define PLSAPP_H
#include "obs-app.hpp"
#include "PLSIPCHandler.h"

#define RUNAPP_API_PATH QStringLiteral("/prismpc/runapp")
#define FAILREASON QStringLiteral("failReason")
#define SUCCESSFAIL QStringLiteral("successFail")

using EventFilterFunc = std::function<bool(QObject *, QEvent *)>;
class pls_process_t;

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
	void backupGolbalConfig() const;
	const char *getProjectName() const;
	const char *getProjectName_kr() const;
	inline config_t *CookieConfig() const { return cookieConfig; }

	inline config_t *NaverShoppingConfig() const { return naverShoppingConfig; }
	void clearNaverShoppingConfig();
	bool notify(QObject *obj, QEvent *evt) override;
	bool event(QEvent *event) override;
	void initSideBarWindowVisible() const;
	void createLoadingApp();
	void destoryLoadingApp();
	static void setAnalogBaseInfo(QJsonObject &obj, bool isUploadHardwareInfo = false);
	static int runProgram(PLSApp &program, int argc, char *argv[], ScopeProfiler &prof);
	static void generatePrismSessionAndSubSession(int argc, char *argv[]);

public slots:
	void sessionExpiredhandler() const;

protected:
	bool eventFilter(QObject *obj, QEvent *event) override;

	void onIpcConnected(pls_ipc_t ipc) override;
	void onIpcDisconnected(pls_ipc_t ipc) override;
	void onIpcMessage(pls_ipc_t ipc, int type, const QJsonValue &data) override;

signals:
	void appleIDAuthCallbackUrl(const QString &url);
	void facebookAuthCallbackUrl(const QString &url);

private:
	void InitCrashConfigDefaults() const;

private:
	bool appRunning = false;
	ConfigFile cookieConfig;
	ConfigFile naverShoppingConfig;
	bool m_isDirectLauncher = true;
	pls_process_t *m_loadingAppPro{nullptr};
	quint32 m_loadingAppPid = -1;
};

#endif // PLSAPP_H
