#ifndef PLSCONTACTVIEW_H
#define PLSCONTACTVIEW_H

#include <QFileInfo>
#include "dialog-view.hpp"
#include <qtextedit.h>

QT_BEGIN_NAMESPACE
namespace Ui {
class PLSContactView;
}
QT_END_NAMESPACE

class PLSContactView : public PLSDialogView {
	Q_OBJECT

public:
	PLSContactView(QWidget *parent = nullptr);
	~PLSContactView();

private:
	void updateItems();
	void deleteItem(int index);
	void setupTextEdit();
	void setupLineEdit();
	void initConnect();
	bool checkAddFileValid(const QFileInfo &fileInfo);
	bool checkTotalFileSizeValid(const QFileInfo &fileInfo);
	bool checkSingleFileSizeValid(const QFileInfo &fileInfo);
	bool checkMailValid();
	bool checkFileFormatValid(const QFileInfo &fileInfo);

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
};

#endif // PLSCONTACTVIEW_H
