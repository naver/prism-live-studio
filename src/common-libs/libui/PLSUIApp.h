#ifndef PLSUIAPP_H
#define PLSUIAPP_H

#include <qapplication.h>
#include <libui.h>

class LIBUI_API PLSUiApp : public pls::Application<QApplication> {
	Q_OBJECT

public:
	PLSUiApp(int &argc, char **argv);
	~PLSUiApp();

public:
	static PLSUiApp *instance();

public:
	enum Icon { //
		CheckedNormal,
		CheckedHover,
		CheckedPressed,
		CheckedDisabled,
		UncheckedNormal,
		UncheckedHover,
		UncheckedPressed,
		UncheckedDisabled,
		IconMax
	};

public:
	void setAppState(bool actived);

	bool ipcIsConnected() const;
	uint32_t ipcGetPeerProcessId() const;
	void initIpc();
	void sendIpcMessage(int type, const QJsonValue &data);
	void initPeerApp();
	void openApp(const QStringList &args, QPointer<QObject> receiver, const pls_app_on_state_t &on_state);

	QString openFilePath() const;
	void setOpenFilePath(const QString &openFilePath);
	void parseOpenFilePath(const std::function<QString(const QStringList &cmdlines)> &openFilePathParser);
	void parseOpenFilePath(const QStringList &cmdlines);

private:
	void processHandCursor(QWidget *widget, bool enter);

protected:
	virtual bool isShowHandWidget(const QMetaObject *metaObject, const QWidget *widget) const;

	virtual void onIpcConnected(pls_ipc_t ipc);
	virtual void onIpcDisconnected(pls_ipc_t ipc);
	virtual void onIpcMessage(pls_ipc_t ipc, int type, const QJsonValue &data);
	virtual void onIpcInnerMessage(pls_ipc_inner_message_t inner_message, const QJsonValue &data);

protected:
	bool notify(QObject *receiver, QEvent *e) override;

signals:
	void appStateChanged(bool actived);
	void peerAppStateChanged(bool actived);
	void peerProcessId();
	void wakeUp(const QStringList &args);
	void peerAnyWindowShow();
	void peerMainWindowShow();
	void peerAnyWindowActived();
	void peerMainWindowActived();
	void peerAppState(pls_app_state_t state);

public:
	QPixmap m_checkBoxIcons[IconMax];
	QPixmap m_radioButtonIcons[IconMax];
	QPixmap m_switchButtonIcons[IconMax];
	bool m_appActived = false;
	QPointer<QWidget> m_tipLabel;
	pls_singleton_app_t *m_singletonApp = nullptr;
	pls_ipc_t m_betweenPrismLens = nullptr;
	pls_app_t m_peerApp = nullptr;
	QString m_openFilePath;
	std::function<QString(const QStringList &cmdlines)> m_openFilePathParser;
};

#endif // !PLSUIAPP_H
