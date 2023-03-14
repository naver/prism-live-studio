#ifndef PLSCONTACTVIEW_H
#define PLSCONTACTVIEW_H

#include <QFileInfo>
#include "dialog-view.hpp"
#include <qtextedit.h>
#include <QPointer>
#include "PLSDpiHelper.h"
#include "loading-event.hpp"

QT_BEGIN_NAMESPACE
namespace Ui {
class PLSContactView;
}
QT_END_NAMESPACE

class PLSContactView : public PLSDialogView {
	Q_OBJECT

public:
	PLSContactView(QWidget *parent = nullptr, PLSDpiHelper dpiHelper = PLSDpiHelper());
	~PLSContactView();

private:
	void updateItems(double dpi);
	void deleteItem(int index);
	void setupTextEdit();
	void setupLineEdit();
	void initConnect();
	bool checkAddFileValid(const QFileInfo &fileInfo, int &errorLevel);
	bool checkTotalFileSizeValid(const QStringList &newFileList);
	bool checkSingleFileSizeValid(const QFileInfo &fileInfo);
	bool checkMailValid();
	bool checkFileFormatValid(const QFileInfo &fileInfo);
	void showLoading(QWidget *parent);
	void hideLoading();

	QString WriteUserDetailInfo();

protected:
	void closeEvent(QCloseEvent *event);
	virtual bool eventFilter(QObject *watcher, QEvent *event) override;

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
	QPointer<QWidget> m_pWidgetLoadingBG = nullptr;
	QPointer<QObject> m_pWidgetLoadingBGParent = nullptr;
	QString chooseFileDir;
};

#endif // PLSCONTACTVIEW_H
