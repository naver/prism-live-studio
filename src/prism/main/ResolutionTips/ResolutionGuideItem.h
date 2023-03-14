#ifndef RESOLUTIONGUIDEITEM_H
#define RESOLUTIONGUIDEITEM_H

#include <QFrame>
#include <QVariantMap>

namespace Ui {
class ResolutionGuideItem;
}

class ResolutionGuideItem : public QFrame {
	Q_OBJECT

public:
	explicit ResolutionGuideItem(QWidget *parent = nullptr);
	~ResolutionGuideItem();
	void initialize(const QVariantMap &);

signals:
	void sigResolutionSelected(const QString &solution);

public:
	void onLinkSelected();
	void setLinkEnanled(bool);

protected:
	void changeEvent(QEvent *e);
	bool eventFilter(QObject *obj, QEvent *event) override;

private:
	Ui::ResolutionGuideItem *ui;
	QVariantMap mData;
};

#endif // RESOLUTIONGUIDEITEM_H
