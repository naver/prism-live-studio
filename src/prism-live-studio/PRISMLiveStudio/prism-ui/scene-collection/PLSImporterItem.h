#ifndef PLSIMPORTERITEM_H
#define PLSIMPORTERITEM_H

#include <QWidget>
#include <QListView>
#include <QAbstractListModel>
#include "PLSCommonScrollBar.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class PLSImporterItem;
}
QT_END_NAMESPACE

class PLSImporterItem : public QFrame {
	Q_OBJECT

public:
	PLSImporterItem(QString name, QString path, QString program, bool selected, QWidget *parent = nullptr);
	~PLSImporterItem();

protected:
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	void enterEvent(QEnterEvent *event) override;
#else
	void enterEvent(QEvent *event)
#endif
	void leaveEvent(QEvent *event) override;

private:
	void SetMouseStatus(const char *status);

signals:
	void CheckedState(bool checked);

private:
	Ui::PLSImporterItem *ui;
	QString path;
};

struct ImporterEntry {
	QString path;
	QString program;
	QString name;

	bool selected{false};
};
enum class ImporterCustomRole { DataRole = Qt::UserRole, pathRole, programRole, nameRole, selectedRole };

class PLSImporterListView;
class PLSImporterModel : public QAbstractListModel {
	Q_OBJECT

	friend class PLSImporterListView;

	void InitDatas(QList<ImporterEntry> &datas);

public:
	explicit PLSImporterModel(QObject *view) : QAbstractListModel(view), listView((PLSImporterListView *)view) {}

	int rowCount(const QModelIndex &parent = QModelIndex()) const override;
	QVariant data(const QModelIndex &index, int role) const override;
	Qt::ItemFlags flags(const QModelIndex &index) const override;
	bool setData(const QModelIndex &index, const QVariant &value, int role) override;
	ImporterEntry GetData(int row) const;
	QList<ImporterEntry> GetDatas() const;

private:
	QList<ImporterEntry> options;
	PLSImporterListView *listView;
};

class PLSImporterListView : public QListView {
	Q_OBJECT
	friend class PLSImporterModel;

public:
	explicit PLSImporterListView(QWidget *parent = nullptr);
	PLSImporterModel *GetModel() const;

	int GetSelectedCount() const;
	void InitWidgets(QList<ImporterEntry> datas) const;
	void UpdateWidgets();
	void UpdateWidget(int row, const ImporterEntry &data);
	void SetData(int row, QVariant variant, ImporterCustomRole role = ImporterCustomRole::DataRole) const;
	PLSImporterItem *CreateItem(int row, ImporterEntry data);
	QList<ImporterEntry> GetDatas() const;

signals:
	void ScrollBarShow(bool show);
	void DataChanged();

private:
	PLSCommonScrollBar *scrollBar{nullptr};
};
#endif // PLSIMPORTERITEM_H
