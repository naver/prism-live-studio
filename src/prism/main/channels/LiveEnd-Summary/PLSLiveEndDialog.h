#ifndef PLSLIVEENDDIALOG_H
#define PLSLIVEENDDIALOG_H

#include <dialog-view.hpp>
#include <QVBoxLayout>

namespace Ui {
class PLSLiveEndDialog;
}

class PLSLiveEndDialog : public PLSDialogView {
	Q_OBJECT

public:
	explicit PLSLiveEndDialog(bool isRecord, QWidget *parent = nullptr);
	~PLSLiveEndDialog();

private:
	Ui::PLSLiveEndDialog *ui;
	void setupFirstUI();
	QVBoxLayout *mChannelVBoxLayout;
	void setupScrollData();
	bool m_bRecord;
	const QString getRecordPath();

private slots:
	void okButtonClicked();
	void openFileSavePath();
};

#endif // PLSLIVEENDDIALOG_H
