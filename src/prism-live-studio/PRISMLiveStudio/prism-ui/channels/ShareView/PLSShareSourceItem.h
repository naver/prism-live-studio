#ifndef SHARESOURCEITEM_H
#define SHARESOURCEITEM_H
#include "PLSDialogView.h"
#include "ui_PLSShareSourceItem.h"

namespace Ui {
class ShareSourceItem;
}

class PLSShareSourceItem : public PLSDialogView {
	Q_OBJECT

public:
	/*  parent must't be NULL  */
	explicit PLSShareSourceItem(QWidget *parent);
	~PLSShareSourceItem() override;
	/*  for initialize data and children views  */
	void initInfo(const QVariantMap &source);

signals:
	/*  used by parent obj  */
	void UrlCopied();

public slots:
	/*  copy to clipboard  */
	void onCopyPressed();

	void openUrl() const;

	void on_CloseBtn_clicked();

protected:
	bool eventFilter(QObject *watched, QEvent *event) override;

private:
	/*  update cache header image  */
	void updatePixmap();

	//private:
	QVariantMap mSource;
	Ui::ShareSourceItem *ui = nullptr;
};

#endif // SHARESOURCEITEM_H
