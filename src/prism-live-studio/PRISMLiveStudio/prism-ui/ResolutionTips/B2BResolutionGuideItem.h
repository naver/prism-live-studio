#ifndef B2BRESOLUTIONGUIDEITEM_H
#define B2BRESOLUTIONGUIDEITEM_H
#include "ResolutionGuidePage.h"
#include <QFrame>
#include <QVariantMap>
class QLabel;

namespace Ui {
class B2BResolutionGuideItem;
}

class B2BResolutionGuideItem : public QFrame {
	Q_OBJECT

public:
	explicit B2BResolutionGuideItem(QWidget *parent = nullptr);
	~B2BResolutionGuideItem() override;
	void initialize(const B2BResolutionPara &data, ResolutionGuidePage *page);

signals:
	void sigResolutionSelected(const QString &solution);

public:
	void onLinkSelected();
	void setLinkEnanled(bool);

protected:
	void changeEvent(QEvent *e) override;
	bool eventFilter(QObject *obj, QEvent *event) override;
	void setThumbnail();

private:
	Ui::B2BResolutionGuideItem *ui;
	B2BResolutionPara m_data;
};

#endif // B2BRESOLUTIONGUIDEITEM_H
