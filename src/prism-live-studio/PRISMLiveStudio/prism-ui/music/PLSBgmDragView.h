#ifndef PLSBGMDRAGVIEW_H
#define PLSBGMDRAGVIEW_H

#include "focus-list.hpp"
#include "PLSBgmDataManager.h"
#include "PLSBgmItemDelegate.h"
#include <QListView>

class PLSBgmItemView;
class PLSBgmItemData;
class PLSBgmItemViewModel;
class PLSBgmDragView : public QListView {
	Q_OBJECT
public:
	explicit PLSBgmDragView(QWidget *parent = nullptr);
	~PLSBgmDragView() override;

	void SetDpi(double dpi) const;
	void SetMediaStatus(const int &row, const MediaStatus &status);
	int GetCurrentRow() const;
	int GetRow(const PLSBgmItemData &data) const;
	PLSBgmItemData GetData(const QModelIndex &idx) const;

	void UpdateWidget();
	void UpdateWidget(const QVector<PLSBgmItemData> &datas);
	void InsertWidget(const QVector<PLSBgmItemData> &datas) const;

	void UpdateWidget(const QModelIndex &idx, const PLSBgmItemData &data) const;
	void SetCurrentRow(const PLSBgmItemData &data);
	void UpdataData(const int &index, const PLSBgmItemData &data) const;
	void UpdataData(const int &index, QVariant value, CustomDataRole role) const;

	QVector<PLSBgmItemData> GetData() const;
	QModelIndex GetModelIndex(const int &row) const;
	void Remove(const PLSBgmItemData &data) const;
	bool Existed(const QString &url) const;
	void Clear() const;
	int Count() const;
	int GetId(const QString url) const;
	PLSBgmItemData Get(int idx) const;
	PLSBgmItemData Get(const QString &url, const int &id) const;
	PLSBgmItemData GetCurrent() const;

protected:
	void mousePressEvent(QMouseEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void dragEnterEvent(QDragEnterEvent *event) override;
	void dragMoveEvent(QDragMoveEvent *event) override;
	void dropEvent(QDropEvent *event) override;
	void dragLeaveEvent(QDragLeaveEvent *event) override;
	void paintEvent(QPaintEvent *event) override;
	void showEvent(QShowEvent *event) override;

private:
	void SetPaintLinePos(const int &startPosX, const int &startPosY, const int &endPosX, const int &endPosY);
	PLSBgmItemViewModel *GetStm() const;
	bool ExistedId(const QString url, const int &id) const;
	void RemoveFile(const PLSBgmItemData &data) const;
	void CreateItemDelegate();

signals:
	void DelButtonClickedSignal(const PLSBgmItemData &data, bool);
	void LoadingFailed(const QString &name);
	void MousePressedSignal(PLSBgmItemView *item);
	void MouseDoublePressedSignal(const QModelIndex &index);
	void RowChanged(const int &srcIndex, const int &destIndex);
	void AudioFileDraggedIn(const QStringList &paths);

private:
	QPoint startDragPoint{};
	QPoint lineStart{};
	QPoint lineEnd{};
	QModelIndex startDragModelIdx{};
	QModelIndex dragOverModelIndex;
	int startDragIndex{-1};
	bool isDraging{false};
	bool dragFile{false};
};

#endif // PLSBGMDRAGVIEW_H
