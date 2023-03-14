#ifndef PLSTESTMODULE_H
#define PLSTESTMODULE_H

#include "dialog-view.hpp"

enum class CamTestModeType {
	TMT_NONE = 0,
	TMT_CRASH,
	TMT_DISAPPEAR,
	TMT_EMPTY_PATH,
};

namespace Ui {
class PLSTestModule;
}

class PLSTestModule : public PLSDialogView {
	Q_OBJECT

public:
	explicit PLSTestModule(QWidget *parent = nullptr, PLSDpiHelper dpiHelper = PLSDpiHelper());
	~PLSTestModule();

private slots:
	//init exception
	void on_initExceptionBtn_clicked();
	void initEngineCrash();
	void initEngineFailed();
	void initEngineInvalidParam();
	void invalidDxVersion();
	void prismInitFailed();
	void initEngineCheckTimeout();
	void on_runtimeExceptionComboBox_currentIndexChanged(int index);

	//runtime exception
	void on_runtimeExceptionBtn_clicked();
	void textureOutofmemory();
	void showNotSupportDX11Notice();
	void deviceRebuildFailed();
	void saveAudioIntoPCM();
	void cameraProcessCrash(CamTestModeType type);
	void popupPrismAlertMessagebox();
	void subProcessDeviceRebuild();
	void mainEngineRebuildFailed();

	//crash exception
	void on_crashExceptionBtn_clicked();
	void vstPluginCrash();
	void systemOutOfMemeryCrash();
	void graphicsDriverCrash();
	void videoDeviceCrash();
	void audioDeviceCrash();
	void othersTypeCrash();

	//Lookup string
	void on_pushButtonLookupString_clicked();
	void on_pushButton_clicked();
	void on_pushButton_2_clicked();
	void on_pushButton_3_clicked();
	void on_pushButton_4_clicked();
	void on_pushButton_5_clicked();
	void on_pushButton_6_clicked();
	void on_pushButton_7_clicked();
	void on_pushButton_8_clicked();
	void on_pushButton_9_clicked();
	void on_pushButton_10_clicked();
	void on_pushButton_11_clicked();
	void on_pushButton_12_clicked();
	void on_pushButton_13_clicked();

private:
	Ui::PLSTestModule *ui;
	QTimer *m_pTimerForAlert = nullptr;
};

#endif // PLSTESTMODULE_H
