#pragma once

#include <functional>

#include <QLabel>

#include "utils-api.h"

class QScrollBar;
class QScrollArea;

class PLSNaverShoppingLIVEProductDialogView;

class PLSNaverShoppingLIVESmartStoreLabel : public QLabel {
	Q_OBJECT

public:
	PLSNaverShoppingLIVESmartStoreLabel(double dpi, const QString &storeId, const QString &storeName, bool selected, QWidget *parent);
	~PLSNaverShoppingLIVESmartStoreLabel() override = default;

signals:
	void storeSelected(const QString &storeId, const QString &storeName);

protected:
	bool event(QEvent *event) override;

private:
	double dpi;
	QString storeId;
	QString storeName;
};

class PLSNaverShoppingLIVEStoreNameList : public QFrame {
	Q_OBJECT
	PLS_NEW_DELETE_FRIENDS

private:
	explicit PLSNaverShoppingLIVEStoreNameList(double dpi, QWidget *droplistButton, PLSNaverShoppingLIVEProductDialogView *productDialogView);
	~PLSNaverShoppingLIVEStoreNameList() override;

public:
	static void popup(QWidget *droplistButton, PLSNaverShoppingLIVEProductDialogView *productDialogView, const std::function<void(const QString &storeId, const QString &storeName)> &callback);

signals:
	void storeSelected(const QString &storeId, const QString &storeName);

protected:
	bool eventFilter(QObject *watched, QEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;

public:
	static const int MaxVisibleItems = 5;

	static bool storesNameListVisible;

	QWidget *droplistButton = nullptr;
	PLSNaverShoppingLIVESmartStoreLabel *smartStoreLabel = nullptr;
	PLSNaverShoppingLIVESmartStoreLabel *currentSmartStoreLabel = nullptr;
	QScrollArea *scrollArea = nullptr;
	QScrollBar *verticalScrollBar = nullptr;
};
