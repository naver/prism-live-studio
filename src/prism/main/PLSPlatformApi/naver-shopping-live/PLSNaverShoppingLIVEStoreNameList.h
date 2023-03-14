#pragma once

#include <functional>

#include <QLabel>

class QScrollBar;
class QScrollArea;

class PLSNaverShoppingLIVEProductDialogView;

class PLSNaverShoppingLIVESmartStoreLabel : public QLabel {
	Q_OBJECT

public:
	PLSNaverShoppingLIVESmartStoreLabel(double dpi, const QString &storeId, const QString &storeName, bool selected, QWidget *parent);
	~PLSNaverShoppingLIVESmartStoreLabel();

signals:
	void storeSelected(const QString &storeId, const QString &storeName);

protected:
	virtual bool event(QEvent *event) override;

private:
	double dpi;
	QString storeId;
	QString storeName;
};

class PLSNaverShoppingLIVEStoreNameList : public QFrame {
	Q_OBJECT

private:
	explicit PLSNaverShoppingLIVEStoreNameList(double dpi, QWidget *droplistButton, PLSNaverShoppingLIVEProductDialogView *productDialogView);
	~PLSNaverShoppingLIVEStoreNameList();

public:
	static void popup(QWidget *droplistButton, PLSNaverShoppingLIVEProductDialogView *productDialogView, std::function<void(const QString &storeId, const QString &storeName)> callback);

signals:
	void storeSelected(const QString &storeId, const QString &storeName);

protected:
	virtual bool eventFilter(QObject *watched, QEvent *event) override;
	virtual void resizeEvent(QResizeEvent *event) override;

public:
	static const int MaxVisibleItems = 5;

	QWidget *droplistButton = nullptr;
	PLSNaverShoppingLIVESmartStoreLabel *smartStoreLabel = nullptr;
	PLSNaverShoppingLIVESmartStoreLabel *currentSmartStoreLabel = nullptr;
	QScrollArea *scrollArea = nullptr;
	QScrollBar *verticalScrollBar = nullptr;
};
