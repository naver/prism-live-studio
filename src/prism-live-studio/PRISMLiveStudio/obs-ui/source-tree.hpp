#pragma once

#include <QList>
#include <QVector>
#include <QPointer>
#include <QListView>
#include <QCheckBox>
#include <QStaticText>
#include <QSvgRenderer>
#include <QAbstractListModel>
#include <QStyledItemDelegate>
#include <obs.hpp>
#include <obs-frontend-api.h>
#include <QScrollBar>
#include <QProxyStyle>
#include <QPainter>

class QLabel;
class OBSSourceLabel;
class QCheckBox;
class QLineEdit;
class SourceTree;
class QSpacerItem;
class QHBoxLayout;
class VisibilityItemWidget;

class SourceTreeItem : public QWidget {
	Q_OBJECT

	friend class SourceTree;
	friend class SourceTreeModel;

	void mouseDoubleClickEvent(QMouseEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;
	void enterEvent(QEnterEvent *event) override;

	void leaveEvent(QEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;
	bool eventFilter(QObject *object, QEvent *event) override;

	void Update(bool force);

	void OnIconTypeChanged(QString value);
	void OnSelectChanged(bool isSelected, OBSSceneItem sceneitem, bool sendSignal = true);
	void UpdateRightMargin();

	enum class Type {
		Unknown,
		Item,
		Group,
		SubItem,
	};

	void DisconnectSignals();
	void ReconnectSignals();

	Type type = Type::Unknown;
signals:
	void SelectItemChanged(OBSSceneItem item, bool selected);
	void VisibleItemChanged(OBSSceneItem item, bool visible);

public:
	enum class SourceItemBgType { BgDefault = 0, BgPreset, BgCustom };
	enum class IndicatorType { IndicatorNormal, IndicatorAbove, IndicatorBelow };

	explicit SourceTreeItem(SourceTree *tree, OBSSceneItem sceneitem);

	SourceTreeItem(const SourceTreeItem &) = delete;
	SourceTreeItem &operator=(const SourceTreeItem &) = delete;
	SourceTreeItem(SourceTreeItem &&) = delete;
	SourceTreeItem &operator=(SourceTreeItem &&) = delete;

	bool IsEditing() const;
	OBSSceneItem SceneItem() const;

	void SetBgColor(SourceItemBgType type, void *param);

	void OnMouseStatusChanged(const char *s);
	void UpdateIndicator(IndicatorType type);

	// only refresh selected ui for group item in dual output
	void SelectGroupItem(bool selected);
	void resetMousePressed(bool mousePressed);
	bool getMousePressed();
	bool checkItemSelected(bool horSelected);

private:
	QSpacerItem *spacer = nullptr;
	QCheckBox *expand = nullptr;
	QLabel *iconLabel = nullptr;
	QCheckBox *horVisBtn = nullptr;
	QCheckBox *verVisBtn = nullptr;
	QCheckBox *vis = nullptr;
	QCheckBox *lock = nullptr;
	QHBoxLayout *boxLayout = nullptr;
	OBSSourceLabel *label = nullptr;
	QPushButton *deleteBtn = nullptr;

	QLineEdit *editor = nullptr;

	std::string newName;

	QLabel *aboveIndicator = nullptr;
	QLabel *belowIndicator = nullptr;

	QSpacerItem *spaceBeforeVis = nullptr;
	QSpacerItem *spaceBeforeLock = nullptr;
	QSpacerItem *rightMargin = nullptr;
	QSpacerItem *spaceAfterHorVis = nullptr;
	QSpacerItem *spaceAfterVerVis = nullptr;

	SourceTree *tree;
	OBSSceneItem sceneitem;
	std::vector<OBSSignal> sigs;

	bool selected = false;
	bool isScrollShowed;
	bool isItemNormal = true;
	bool editing = false;

	// Used to distinguish whether the selection event is generated on the preview or clicked on the source tree
	bool mousePressed = false;

	void paintEvent(QPaintEvent *event) override;

	void ExitEditModeInternal(bool save);

private slots:
	void Clear();

	void EnterEditMode();
	void ExitEditMode(bool save);

	void HorVisibilityChanged(bool visible);
	void VerVisibilityChanged(bool visible);
	void LockedChanged(bool locked);

	void ExpandClicked(bool checked) const;

	void Select();
	void Deselect();
	void verItemSelect(OBSSceneItem verItem, bool select);

	void OnSourceScrollShow(bool isShow);

	void UpdateNameColor(bool selected, bool visible);
	void UpdateIcon(bool visible);
};

class SourceTreeModel : public QAbstractListModel {
	Q_OBJECT

	friend class SourceTree;
	friend class SourceTreeItem;

	SourceTree *st;
	QVector<OBSSceneItem> items;
	bool hasGroups = false;

	static void OBSFrontendEvent(enum obs_frontend_event event, void *ptr);
	void Clear();
	void SceneChanged();
	void ReorderItems();
	int Count() const;
	QVector<OBSSceneItem> GetItems() const;

	void Add(obs_sceneitem_t *item);
	void Remove(const void *item); // in function we should not use item directly to avoid using deleted memory
	OBSSceneItem Get(int idx);
	QString GetNewGroupName() const;
	void AddGroup();
	bool Existed(OBSSceneItem item);

	void GroupSelectedItems(QModelIndexList &indices);
	void UngroupSelectedGroups(QModelIndexList &indices);

	void ExpandGroup(obs_sceneitem_t *item);
	void CollapseGroup(const obs_sceneitem_t *item);

	void UpdateGroupState(bool update);
signals:
	void itemRemoves(QVector<OBSSceneItem> items);
	void itemReorder();

public:
	explicit SourceTreeModel(SourceTree *st);

	virtual int rowCount(const QModelIndex &parent) const override;
	virtual QVariant data(const QModelIndex &index, int role) const override;

	virtual Qt::ItemFlags flags(const QModelIndex &index) const override;
	virtual Qt::DropActions supportedDropActions() const override;
};

class QSourceScrollBar : public QScrollBar {
	Q_OBJECT

public:
	using QScrollBar::QScrollBar;
	~QSourceScrollBar() override = default;

	void hideEvent(QHideEvent *) override { emit SourceScrollShow(false); }
	void showEvent(QShowEvent *) override { emit SourceScrollShow(true); }

signals:
	void SourceScrollShow(bool isShow);
};

class SourceTreeProxyStyle : public QProxyStyle {
	Q_OBJECT
	using QProxyStyle::QProxyStyle;

public:
	void drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter,
			   const QWidget *widget) const override
	{
		// customize the drop indicator style
		if (element == QStyle::PE_IndicatorItemViewItemDrop) {
			painter->save();
			auto pen = painter->pen();
			pen.setColor(Qt::transparent);
			painter->setPen(pen);
			QProxyStyle::drawPrimitive(element, option, painter, widget);
			painter->restore();
		} else {
			QProxyStyle::drawPrimitive(element, option, painter, widget);
		}
	}
};

class SourceTree : public QListView {
	Q_OBJECT

	bool ignoreReorder = false;

	friend class SourceTreeModel;
	friend class SourceTreeItem;

	QSourceScrollBar *scrollBar;
	SourceTreeItem *preDragOver = nullptr;

	bool textPrepared = false;
	QStaticText textNoSources;
	QSvgRenderer iconNoSources;
	QLabel *noSourceTips;
	OBSData undoSceneData;

	bool iconsVisible = true;

	//void UpdateNoSourcesMessage();

	void ResetWidgets();
	void UpdateWidget(const QModelIndex &idx, obs_sceneitem_t *item);
	void UpdateWidgets(bool force = false);

	inline SourceTreeModel *GetStm() const { return reinterpret_cast<SourceTreeModel *>(model()); }
	void NotifyItemSelect(obs_sceneitem_t *sceneitem, bool select);
	void NotifyVerticalItemSelect(int index, obs_sceneitem_t *sceneitem, bool select);
	void triggeredSelection(int row, bool select);

	bool checkDragBreak(const QDragMoveEvent *event, int row, const DropIndicatorPosition &indicator);

public:
	inline SourceTreeItem *GetItemWidget(int idx)
	{
		QWidget *widget = indexWidget(GetStm()->createIndex(idx, 0));
		return reinterpret_cast<SourceTreeItem *>(widget);
	}
	void SelectGroupItem(obs_sceneitem_t *groupItem, bool selected, bool forceSelect = false);
	explicit SourceTree(QWidget *parent = nullptr);

	inline bool IgnoreReorder() const { return ignoreReorder; }
	inline void Clear() const { GetStm()->Clear(); }

	inline void Add(obs_sceneitem_t *item) const { GetStm()->Add(item); }
	inline OBSSceneItem Get(int idx) const { return GetStm()->Get(idx); }
	inline QString GetNewGroupName() const { return GetStm()->GetNewGroupName(); }

	void SelectItemWithDualOutput(obs_sceneitem_t *sceneitem, bool select, bool fromSourceList);
	void SelectItem(obs_sceneitem_t *sceneitem, bool select, bool fromSourceList = false);

	bool MultipleBaseSelected() const;
	bool GroupsSelected() const;
	bool GroupsSelectedWithDualOutput(const QModelIndexList &selectedIndices) const;
	bool GroupedItemsSelected() const;

	bool CheckItemInSelectedGroup(obs_sceneitem_t *sceneitem);
	bool CheckGroupItemSelected(obs_sceneitem_t *sceneitem);
	bool CheckGroupAllItemSelected(obs_sceneitem_t *sceneitem);
	void SetGroupAllItemSelected(obs_sceneitem_t *sceneitem);
	void CheckGroupItemUnselectStatus(obs_sceneitem_t *sceneitem);

	void UpdateIcons() const;
	void SetIconsVisible(bool visible);

	int Count() const;
	QVector<OBSSceneItem> GetItems() const;

	void ResetDragOver();
	bool GetDestGroupItem(QPoint pos, obs_sceneitem_t *&item_output) const;
	bool CheckDragSceneToGroup(const obs_sceneitem_t *dragItem, const obs_sceneitem_t *destGroupItem) const;
	bool IsValidDrag(obs_sceneitem_t *destGroupItem, QVector<OBSSceneItem> items) const;
	void dropEventHasGroups(const SourceTreeModel *stm, QModelIndexList &indices, const OBSScene &scene,
				const QVector<OBSSceneItem> &items) const;
	void dropEventPersistentIndicesForeachCb(int &r, SourceTreeModel *stm, QVector<OBSSceneItem> &items,
						 const QPersistentModelIndex &persistentIdx) const;
	void selectionChangedProcess(const QItemSelection &selected, const QItemSelection &deselected);
	void resetMousePressed();
public slots:
	inline void ReorderItems() const { GetStm()->ReorderItems(); }
	inline void RefreshItems() { GetStm()->SceneChanged(); }
	void Remove(OBSSceneItem item, OBSScene scene) const;
	void GroupSelectedItems() const;
	void UngroupSelectedGroups() const;
	void AddGroup() const;
	bool Edit(int idx);
	void NewGroupEdit(int idx);
	void OnSourceItemRemove(unsigned long long sceneItemPtr) const;
	void OnSelectItemChanged(OBSSceneItem item, bool selected);
	void OnVisibleItemChanged(OBSSceneItem item, bool visible);

	// for dual output
	void updateVerItemIconVisible(OBSSceneItem horItem, OBSSceneItem verItem);
	void updateVisIconVisibleWhenDualoutput();
	void updateVisibleIcon(int index, OBSSceneItem horItem, OBSSceneItem verItem);

protected:
	virtual void mouseDoubleClickEvent(QMouseEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;
	void keyPressEvent(QKeyEvent *event) override;
	void keyReleaseEvent(QKeyEvent *event) override;
	void dragEnterEvent(QDragEnterEvent *event) override;
	void dragLeaveEvent(QDragLeaveEvent *event) override;
	void dragMoveEvent(QDragMoveEvent *event) override;
	virtual void dropEvent(QDropEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void leaveEvent(QEvent *event) override;
	virtual void paintEvent(QPaintEvent *event) override;

	virtual void selectionChanged(const QItemSelection &selected, const QItemSelection &deselected) override;
signals:
	void SelectItemChanged(OBSSceneItem item, bool selected);
	void VisibleItemChanged(OBSSceneItem item, bool visible);
	void itemsRemove(QVector<OBSSceneItem> items);
	void itemsReorder();

private:
	bool isVerticalItemSignal = false;
	bool needHandleClick = true;
	bool ignoreSelectionChanged = false;
	bool noValidClick = false;
	bool shiftPressed = false;
	bool mousePressed = false;
};

class SourceTreeDelegate : public QStyledItemDelegate {
	Q_OBJECT

public:
	SourceTreeDelegate(QObject *parent);
	virtual QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
};
