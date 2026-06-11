#ifndef PLSLIVEENDDIALOG_H
#define PLSLIVEENDDIALOG_H

#include <QVBoxLayout>
#include "PLSDialogView.h"

namespace Ui {
class PLSLiveEndDialog;
}

enum class PLSEndPageType { PLSRecordPage = 1, PLSRehearsalPage, PLSLivingPage };

class PLSLiveEndDialog : public PLSDialogView {
	Q_OBJECT

public:
	explicit PLSLiveEndDialog(PLSEndPageType pageType, QWidget *parent = nullptr);
	~PLSLiveEndDialog() override;

private:
	Ui::PLSLiveEndDialog *ui;
	void setupFirstUI();
	QVBoxLayout *mChannelVBoxLayout;
	void setupScrollData();
	void updateOnDPIChanged(int tmpLiveCount, double dpi, bool isExceendMax);
	PLSEndPageType m_pageType;
	QString getRecordPath() const;

private slots:
	void okButtonClicked();
	void openFileSavePath() const;
	void openTipView();

private:
	const static int endItemHeight = 75;
	const static int windowWidth = 720;
	const static int maxCurrentShowCount = 3;
	const static int recordWidnowHeight = 240;
	const static int liveModeOffest = 5;
};

#endif // PLSLIVEENDDIALOG_H
