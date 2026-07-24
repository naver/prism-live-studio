#ifndef PLSUPDATESTICKERGUIDEVIEW_H
#define PLSUPDATESTICKERGUIDEVIEW_H

#include <QFrame>

namespace Ui {
class PLSUpdateStickerGuideView;
}

class PLSUpdateStickerGuideView : public QFrame {
	Q_OBJECT

public:
	explicit PLSUpdateStickerGuideView(QWidget *parent = nullptr);
	~PLSUpdateStickerGuideView();
	void updateGuideIcon(const QPixmap &pix);
	void updateOkButtonEnabled(bool enabled);
	void setDefaultIcon(bool defaultIcon);
	void calcFixedHeight();

signals:
	void onFinishButtonClicked();

protected:
	void showEvent(QShowEvent *event) override;

private:
	Ui::PLSUpdateStickerGuideView *ui;
};

#endif // PLSUPDATESTICKERGUIDEVIEW_H
