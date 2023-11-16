#ifndef PLSFILTERSITEMVIEW_H
#define PLSFILTERSITEMVIEW_H

#include <QWidget>
#include <QStyledItemDelegate>
#include <QListWidget>
#include "obs.hpp"

namespace Ui {
class PLSFiltersItemView;
}

class PLSFiltersItemView : public QWidget {
	Q_OBJECT

public:
	explicit PLSFiltersItemView(obs_source_t *source, QWidget *parent = nullptr);
	~PLSFiltersItemView() override;

	PLSFiltersItemView(const PLSFiltersItemView &) = delete;
	PLSFiltersItemView &operator=(const PLSFiltersItemView &) = delete;
	PLSFiltersItemView(PLSFiltersItemView &&) = delete;
	PLSFiltersItemView &operator=(PLSFiltersItemView &&) = delete;

	OBSSource GetFilter() const;
	bool GetCurrentState() const;
	void SetCurrentItemState(bool state);
	void OnRenameActionTriggered();
	void SetText(const QString &text);
	void SetColor(const QColor &color, bool active, bool selected) const;

protected:
	void mousePressEvent(QMouseEvent *event) override;
	void mouseDoubleClickEvent(QMouseEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;
	void enterEvent(QEnterEvent *event) override;
	void leaveEvent(QEvent *event) override;
	bool eventFilter(QObject *object, QEvent *event) override;
	void contextMenuEvent(QContextMenuEvent *event) override;

private:
	static void OBSSourceEnabled(void *param, calldata_t *data);
	static void OBSSourceRenamed(void *param, calldata_t *data);
	QString GetNameElideString() const;
	void OnMouseStatusChanged(const QString &status);
	void OnFinishingEditName(bool cancel);
	void CreatePopupMenu();
	void UpdateNameStyle();
	void SetProperty(QWidget *widget, const char *property, const QVariant &value) const;

private slots:
	void OnVisibilityButtonClicked(bool visible) const;
	void OnAdvButtonClicked();
	void OnRemoveActionTriggered();
	void SourceEnabled(bool enabled);
	void SourceRenamed(QString name);

signals:
	void OnFilterRenameTriggered();
	void FilterRenameTriggered(PLSFiltersItemView *item);
	void FilterRemoveTriggered(PLSFiltersItemView *item);
	void FinishingEditName(QWidget *editor, PLSFiltersItemView *item);
	void CurrentItemChanged(PLSFiltersItemView *item);

private:
	Ui::PLSFiltersItemView *ui;
	OBSSource source;
	QString name;
	bool current{false};
	bool isFinishEditing{true};
	bool editActive{false};

	OBSSignal enabledSignal;
	OBSSignal renamedSignal;
};

class PLSFiltersItemDelegate : public QStyledItemDelegate {
	Q_OBJECT

public:
	explicit PLSFiltersItemDelegate(QObject *parent = nullptr);

	void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;

protected:
	bool eventFilter(QObject *object, QEvent *event) override;
};

#endif // PLSFILTERSITEMVIEW_H
