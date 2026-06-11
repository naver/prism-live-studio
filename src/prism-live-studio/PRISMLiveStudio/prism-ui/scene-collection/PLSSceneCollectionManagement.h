#ifndef PLSSCENECOLLECTIONMANAGEMENT_H
#define PLSSCENECOLLECTIONMANAGEMENT_H

#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QEvent>
#include "PLSSceneCollectionItem.h"
#include "PLSLabel.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class PLSSceneCollectionManagement;
}
QT_END_NAMESPACE

class PLSSceneCollectionManagement;
class PLSSceneCollectionManagement : public QFrame {
	Q_OBJECT

public:
	explicit PLSSceneCollectionManagement(QWidget *parent = nullptr);
	~PLSSceneCollectionManagement();
	void InitDefaultCollectionText(QVector<PLSSceneCollectionData> datas) const;
	void SetCurrentText(const QString &name, const QString &path) const;
	void AddSceneCollection(const QString &name, const QString &path) const;
	void RemoveSceneCollection(const QString &name, const QString &path) const;
	void RenameSceneCollection(const QString &srcName, const QString &srcPath, const QString &destName, const QString &destPath) const;
	void Resize(int count);
	void OnTriggerEnterEvent(const QString &name, const QString &path);

protected:
	bool eventFilter(QObject *obj, QEvent *event) override;
signals:
	void ShowSceneCollectionView();

private:
	Ui::PLSSceneCollectionManagement *ui;
};
#endif // PLSSCENECOLLECTIONMANAGEMENT_H
