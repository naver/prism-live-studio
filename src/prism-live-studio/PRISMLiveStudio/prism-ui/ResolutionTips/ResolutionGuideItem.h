#ifndef RESOLUTIONGUIDEITEM_H
#define RESOLUTIONGUIDEITEM_H

#include <QFrame>
#include <QVariantMap>
class QLabel;

namespace Ui {
class ResolutionGuideItem;
}

class ResolutionGuideItem : public QFrame {
	Q_OBJECT

public:
	explicit ResolutionGuideItem(QWidget *parent = nullptr);
	~ResolutionGuideItem() override;
	void initialize(const QVariantMap &);

signals:
	void sigResolutionSelected(const QString &solution);

public:
	void onLinkSelected();
	void setLinkEnanled(bool);

protected:
	void changeEvent(QEvent *e) override;
	bool eventFilter(QObject *obj, QEvent *event) override;
	void checkBitrateData(const QVariantMap &data);

private:
	Ui::ResolutionGuideItem *ui;
	QVariantMap mData;
	QLabel *blockLabel = nullptr;
};

#endif // RESOLUTIONGUIDEITEM_H
