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
	~PLSLiveEndItem();

	void setNameElideString();

private:
	Ui::PLSLiveEndItem *ui;
	const QVariantMap &mSourceData;
	void setupData();
	QString toThousandsNum(QString numString);
	void combineTwoImage();
	void setupStatusWidget();
};

#endif // PLSLIVEENDITEM_H
