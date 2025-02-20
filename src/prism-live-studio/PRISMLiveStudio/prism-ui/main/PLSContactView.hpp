#ifndef PLSCONTACTVIEW_H
#define PLSCONTACTVIEW_H

#include <QFileInfo>
#include "PLSDialogView.h"
#include <qtextedit.h>
#include <QPointer>
#include "loading-event.hpp"
#include "PLSRadioButton.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class PLSContactView;
}
QT_END_NAMESPACE

class PLSContactView : public PLSDialogView {
	Q_OBJECT

public:
	PLSContactView(const QString &message, const QString &additionalMessage, QWidget *parent = nullptr);
	~PLSContactView() override;

private:
	void updateItems(double dpi);
	void deleteItem(int index);
	void setupTextEdit() const;
	void setupLineEdit();
	void initConnect() const;
	bool checkAddFileValid(const QFileInfo &fileInfo, int &errorLevel) const;
	bool checkTotalFileSizeValid(const QStringList &newFileList) const;
	bool checkSingleFileSizeValid(const QFileInfo &fileInfo) const;
	bool checkMailValid() const;
	bool checkFileFormatValid(const QFileInfo &fileInfo) const;
	void showLoading(QWidget *parent);
	void hideLoading();

	QString WriteUserDetailInfo() const;

	void writePrismVersionInfo(std::ofstream &file) const;
	void writeCurrentSceneInfo(std::ofstream &file) const;
	void writeDefaultAudioMixerInfo(std::ofstream &file) const;
	void writeAudioMonitorDeviceInfo(std::ofstream &file) const;
	void writeHardwareInfo(std::ofstream &file) const;
	void setupErrorMessageTextEdit(const QString &message);
	void updateSendButtonState();

protected:
	void closeEvent(QCloseEvent *event) override;
	bool eventFilter(QObject *watcher, QEvent *event) override;

private slots:
	void on_fileButton_clicked();
	void on_sendButton_clicked();
	void on_cancelButton_clicked();
	void on_textEdit_textChanged();
	void on_emailLineEdit_editingFinished();
	void on_emailLineEdit_textChanged(const QString &string);

private:
	Ui::PLSContactView *ui;
	QList<QFileInfo> m_fileLists;
	PLSLoadingEvent m_loadingEvent;
	QWidget *m_pWidgetLoadingBG = nullptr;
	QPointer<QObject> m_pWidgetLoadingBGParent = nullptr;
	QString chooseFileDir;
	bool m_isGroup = false;
	QString m_additionalMessage;
	QString m_message;
	QString m_originMessage;
	QScrollBar *m_verticalScrollBar{nullptr};
	PLSRadioButtonGroup *m_pInquireType = nullptr;
};

#endif // PLSCONTACTVIEW_H
