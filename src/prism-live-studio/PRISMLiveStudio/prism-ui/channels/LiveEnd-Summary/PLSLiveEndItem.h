#ifndef PLSLIVEENDITEM_H
#define PLSLIVEENDITEM_H

#include <QFrame>

namespace Ui {
class PLSLiveEndItem;
}

class PLSLiveEndItem : public QFrame {
	Q_OBJECT

public:
	explicit PLSLiveEndItem(const QString &uuid, QWidget *parent = nullptr);
	~PLSLiveEndItem() override;

	void setNameElideString();

signals:
	void tipButtonClick();

private slots:
	void shareButtonClicked() const;

private:
	Ui::PLSLiveEndItem *ui;
	QString m_shareUrl;

	const QVariantMap &mSourceData;
	void setupData();
	QString toThousandsNum(QString numString) const;
	void combineTwoImage();
	void setupStatusWidget();
	void refreshUIWhenDPIChanged(double dpi, const QString &platformName);
	bool needShowTipView() const;
};

#endif // PLSLIVEENDITEM_H
