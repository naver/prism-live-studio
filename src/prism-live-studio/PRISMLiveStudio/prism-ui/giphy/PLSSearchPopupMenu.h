#ifndef PLSSEARCHPOPUPMENU_H
#define PLSSEARCHPOPUPMENU_H

#include <QFrame>
#include <QPushButton>
#include <QLabel>
#include <QListView>
#include <QStandardItemModel>
#include "flowlayout.h"

namespace Ui {
class PLSSearchPopupMenu;
}

class PLSSearchPopupMenu : public QFrame {
	Q_OBJECT

public:
	explicit PLSSearchPopupMenu(QWidget *parent = nullptr);
	~PLSSearchPopupMenu() final;

	void Show(const QPoint &offset, int contentWidth);
	void SetContentWidth(int width);

	void AddHistoryItem(const QString &keyword);
	void AddRecommendItems(const QStringList &listwords);
	void DpiChanged(double dpi) const;

public slots:
	void UpdateContentHeight();
	void resizeToContent();

signals:
	void ItemClicked(const QString &key);
	void ItemDelete(const QString &key);

private:
	Ui::PLSSearchPopupMenu *ui;
	int contentWidth{0};
};

class HistorySearchItem : public QFrame {
	Q_OBJECT

public:
	explicit HistorySearchItem(QWidget *parent = nullptr);
	~HistorySearchItem() final = default;

	void SetContent(const QString &text);
	QString GetContent() const;

private:
	void OptimizeDisplay();

protected:
	void resizeEvent(QResizeEvent *event) override;

signals:
	void Clicked(const QString &content);
	void Deleted(HistorySearchItem *item);

private:
	QString content;
	QPushButton *contentLabel;
};

class HistorySearchList : public QFrame {
	Q_OBJECT

public:
	explicit HistorySearchList(QWidget *parent = nullptr);
	~HistorySearchList() final = default;

	void AddItem(const QString &content);
	void AddItems(const QStringList &contents);
	int ItemCount() const;

private:
	bool FindAndMoveToFront(const QString &content);

signals:
	void Clicked(const QString &content);
	void UpdateContentHeight();
	void ItemDelete(const QString &key);

private slots:
	void OnItemDeleted(HistorySearchItem *item);

private:
	QList<HistorySearchItem *> listItem;
	QVBoxLayout *layout_main{nullptr};
};

class RecommendSearchList : public QFrame {
	Q_OBJECT

public:
	explicit RecommendSearchList(QWidget *parent = nullptr);
	~RecommendSearchList() final = default;

	void AddListData(const QStringList &words);
	void ClearList();

	void DpiChanged(double dpi) const;

signals:
	void UpdateContentHeight();
	void RecommendItemClicked(const QString &key);

private:
	FlowLayout *flowLayout;
	QList<QPushButton *> listItem;
};

#endif // PLSSEARCHPOPUPMENU_H
