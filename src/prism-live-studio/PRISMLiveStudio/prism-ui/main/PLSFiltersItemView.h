#ifndef PLSFILTERSITEMVIEW_H
#define PLSFILTERSITEMVIEW_H

#include <QWidget>
#include <QStyledItemDelegate>
#include <QListWidget>
#include <QProxyStyle>
#include <QPainter>
#include "obs.hpp"
#include "focus-list.hpp"

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
	void UpdateNameStyle();
	void SetProperty(QWidget *widget, const char *property, const QVariant &value) const;

private slots:
	void OnVisibilityButtonClicked(bool visible) const;
	void OnRemoveActionTriggered();
	void SourceEnabled(bool enabled);
	void SourceRenamed(QString name);

signals:
	void FilterRenameTriggered(PLSFiltersItemView *item);
	void FilterRemoveTriggered(PLSFiltersItemView *item);
	void FinishingEditName(QWidget *editor, PLSFiltersItemView *item);
	void CurrentItemChanged(PLSFiltersItemView *item);
	void OnCreateCustomContextMenu(const QPoint& pos, bool async);

private:
	Ui::PLSFiltersItemView *ui;
	OBSSource source;
	QString name;
	bool current{false};
	bool async = false;

	OBSSignal enabledSignal;
	OBSSignal renamedSignal;
};

class PLSFiltersItemDelegate : public QStyledItemDelegate {
	Q_OBJECT

public:
	explicit PLSFiltersItemDelegate(QObject *parent = nullptr);

	void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
	QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
protected:
	bool eventFilter(QObject *object, QEvent *event) override;
	void initStyleOption(QStyleOptionViewItem* option, const QModelIndex& index) const override;
};

class PLSFiltersProxyStyle : public QProxyStyle {
	Q_OBJECT
		using QProxyStyle::QProxyStyle;

public:
	void drawPrimitive(PrimitiveElement element, const QStyleOption* option, QPainter* painter, const QWidget* widget) const override
	{
		// customize the drop indicator style
		if (element == QStyle::PE_IndicatorItemViewItemDrop) {
			painter->save();
			auto pen = painter->pen();
			pen.setColor("#effc35");
			painter->setPen(pen);
			QProxyStyle::drawPrimitive(element, option, painter, widget);
			painter->restore();
		}
		else {
			QProxyStyle::drawPrimitive(element, option, painter, widget);
		}
	}
};

#endif // PLSFILTERSITEMVIEW_H
