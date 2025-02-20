#ifndef PLSSCENECOLLECTIONVIEW_H
#define PLSSCENECOLLECTIONVIEW_H

#include <QWidget>
#include <QListView>
#include <QPushButton>
#include <QAbstractListModel>
#include "PLSDialogView.h"
#include "PLSSceneCollectionItem.h"
#include "PLSCommonScrollBar.h"

enum class SceneCollectionCustomRole { DataRole = Qt::UserRole, VisibleRole, CurrentRole, DelButtonDisableRole, UserLocalPathRole, EnterRole };
Q_DECLARE_METATYPE(SceneCollectionCustomRole)

namespace Ui {
class PLSSceneCollectionView;
}

class PLSSceneCollectionListView;

class PLSClickButton : public QWidget {
	Q_OBJECT
public:
	explicit PLSClickButton(QWidget *parent);

	void setDisplayText(const QString &text);
	void setShowOverlay(bool show);

protected:
	void enterEvent(QEnterEvent *event) override;
	void leaveEvent(QEvent *event) override;

signals:
	void newBtnClicked();
	void importFromLocalBtnClicked();
	void importFromOtherBtnClicked();

private:
	QPushButton *iconButton{nullptr};
	QLabel *textLabel{nullptr};
	QPushButton *baseContent{nullptr};
	QPushButton *overlay{nullptr};
	bool showOverlay = false;
};

class PLSSceneTemplateButton : public QPushButton {
	Q_OBJECT
public:
	explicit PLSSceneTemplateButton(QWidget *parent);

private:
};

class PLSSceneCollectionModel : public QAbstractListModel {
	Q_OBJECT

	friend class PLSSceneCollectionListView;

	PLSSceneCollectionListView *listView;
	QVector<PLSSceneCollectionData> itemDatas;

	void InitDatas(const QVector<PLSSceneCollectionData> &datas);
	void Add(const PLSSceneCollectionData &data);
	void Remove(const PLSSceneCollectionData &data);
	void Rename(const PLSSceneCollectionData &srcData, const PLSSceneCollectionData &destData);
	void Clear();
	void SetCurrentData(const QString &name, const QString &path);
	int GetCurrentRow();
	void RowChanged(int srcIndex, int destIndex);
	QVector<PLSSceneCollectionData> GetDatas() const;
	PLSSceneCollectionData GetData(int row) const;
	int Count() const { return static_cast<int>(itemDatas.count()); }

public:
	explicit PLSSceneCollectionModel(PLSSceneCollectionListView *view);

	int rowCount(const QModelIndex &parent) const override;
	QVariant data(const QModelIndex &index, int role) const override;
	bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
	Qt::ItemFlags flags(const QModelIndex &index) const override;
	Qt::DropActions supportedDropActions() const override;
};

class PLSSceneCollectionListView : public QListView {
	Q_OBJECT

	friend class PLSSceneCollectionModel;

public:
	explicit PLSSceneCollectionListView(QWidget *parent = nullptr);
	PLSSceneCollectionItem *GetItemWidget(int index);

	void InitWidgets(const QVector<PLSSceneCollectionData> &datas) const;
	void Add(const PLSSceneCollectionData &data) const { GetModel()->Add(data); }
	void Remove(const PLSSceneCollectionData &data) const { GetModel()->Remove(data); };
	void Rename(const PLSSceneCollectionData &srcData, const PLSSceneCollectionData &destData) const;
	void ResetWidgets();
	void RepaintWidgets() const;
	void UpdateWidgets();
	void UpdateWidget(int row, const PLSSceneCollectionData &data, bool update = false);
	void SetCurrentData(const QString &name, const QString &path) const { GetModel()->SetCurrentData(name, path); }
	void SetData(int row, QVariant variant, SceneCollectionCustomRole role = SceneCollectionCustomRole::DataRole) const;
	void SetDatas(QVariant variant, SceneCollectionCustomRole role = SceneCollectionCustomRole::DataRole) const;
	void UpdateCurrentTimeStampLabel() const;
	void SetEnableDrops(bool enable);
	PLSSceneCollectionModel *GetModel() const;
	QVector<PLSSceneCollectionData> GetDatas() const;
	PLSSceneCollectionItem *CreateItem(const PLSSceneCollectionData &data);
	int Count() const { return GetModel()->Count(); };

protected:
	void mousePressEvent(QMouseEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void dragEnterEvent(QDragEnterEvent *event) override;
	void dragMoveEvent(QDragMoveEvent *event) override;
	void dropEvent(QDropEvent *event) override;
	void dragLeaveEvent(QDragLeaveEvent *event) override;
	void showEvent(QShowEvent *event) override;
private slots:
	void OnApplyBtnClicked(const QString &name, const QString &path, bool textMode) const;
	void OnExportBtnClicked(const QString &name, const QString &path) const;
	void OnRenameBtnClicked(const QString &name, const QString &path) const;
	void OnDuplicateBtnClicked(const QString &name, const QString &path) const;
	void OnDeleteBtnClicked(const QString &name, const QString &path) const;
	void OnEnverEvent(const QString &name, const QString &path);

private:
	void SetPaintLinePos(int startPosX, int startPosY, int endPosX, int endPosY);

signals:
	void RowChanged(int srcIndex, int destIndex);
	void ScrollBarShow(bool show);
	void TriggerEventEvent(const QString &name, const QString &path);

private:
	QPoint startDragPoint{};
	QModelIndex startDragModelIdx;
	int startDragIndex{-1};
	bool isDraging{false};
	bool enableDrops{true};
	PLSCommonScrollBar *scrollBar{nullptr};
};

class PLSSceneCollectionView : public PLSDialogView {
	Q_OBJECT

public:
	explicit PLSSceneCollectionView(QWidget *parent = nullptr);
	~PLSSceneCollectionView();
	void AddSceneCollectionItem(const QString &name, const QString &path, QString userLocalPath = QString()) const;
	void RemoveCollectionItem(const QString &name, const QString &path) const;
	void RenameCollectionItem(const QString &srcName, const QString &srcPath, const QString &destName, const QString &destPath) const;
	void InitDefaultCollectionItem(QVector<PLSSceneCollectionData> &datas) const;
	QVector<PLSSceneCollectionData> GetDatas() const;
	void UpdateDeleteButtonState() const;
	void UpdateTimeStampLabel() const;
	void SetCurrentItem(const QString &name, const QString &path);
	void SetCurrentText(const QString &name, const QString &path);
	void AddCollectionUserLocalPath(const QString &name, const QString &userLocalPath) const;
	void ClearCollectionUserLocalPath(const QString &name) const;

protected:
	void showEvent(QShowEvent *event) override;
	void closeEvent(QCloseEvent *event) override;
	bool eventFilter(QObject *obj, QEvent *event) override;
private slots:
	void OnSearchTriggerd(const QString &text) const;
	void OnSceneCollectionItemRowChanged(int srcIndex, int destIndex) const;
	void OnImportFromLocalButtonClicked();
	void OnImportFromOtherButtonClicked() const;
	void OnShowSceneTemplateView() const;
	void OnCloseButtonClicked();
	void OnScrollBarShow(bool show);
	void HandleEnterEvent(const QObject *obj, const QEvent *) const;
	void OnTriggerEnterEvent(const QString &name, const QString &path);

signals:
	void currentSceneCollectionChanged(QString name, QString path);
	void newButtonClicked();
	void importButtonClicked();

private:
	int FindExistedCollectionData(const QVector<PLSSceneCollectionData> &datas, const QString &name, const QString &path) const;
	int GetCollectionItemRow(const QString &name, const QString &path) const;
	void InitSceneCollectionConfig(QVector<PLSSceneCollectionData> &datas) const;
	void WriteSceneCollectionConfig() const;
	void UpdateListViewDelBtnStatus(PLSSceneCollectionListView *view, const QVector<PLSSceneCollectionData> &datas) const;
	void SetMouseStatus(QWidget *widget, QString status) const;

	Ui::PLSSceneCollectionView *ui;
};

#endif // PLSSCENECOLLECTIONVIEW_H
