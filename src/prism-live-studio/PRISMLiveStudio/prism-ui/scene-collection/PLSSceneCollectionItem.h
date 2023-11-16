#ifndef PLSSCENECOLLECTIONITEM_H
#define PLSSCENECOLLECTIONITEM_H

#include <QFrame>

namespace Ui {
class PLSSceneCollectionItem;
}

struct PLSSceneCollectionData {
	QString fileName;
	QString filePath;
	QString userLocalPath;
	bool delButtonDisable = false;
	bool current = false;
	bool textMode = false;
};
Q_DECLARE_METATYPE(PLSSceneCollectionData)

enum class DropLine {
	DropLineNone,
	DropLineTop,
	DropLineBottom,
};

class PLSSceneCollectionItem : public QFrame {
	Q_OBJECT

public:
	explicit PLSSceneCollectionItem(const QString &name, const QString &path, bool current, bool textMode, QWidget *parent = nullptr);
	~PLSSceneCollectionItem();
	QString GetFileName() const;
	QString GetFilePath() const;
	void UpdateModifiedTimeStamp();
	void Rename(const QString &name, const QString &path);
	void SetDeleteButtonDisable(bool disable) const;
	void Update(const PLSSceneCollectionData &data);
	void ClearDropLine();
	void DrawDropLine(DropLine lineType);

protected:
	void mousePressEvent(QMouseEvent *event) override;
	void mouseDoubleClickEvent(QMouseEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	void enterEvent(QEnterEvent *event) override;
#else
	void enterEvent(QEvent *event) override;
#endif
	void leaveEvent(QEvent *event) override;
	void paintEvent(QPaintEvent *event) override;
private slots:
	void OnApplyBtnClicked();
	void OnExportBtnClicked();
	void OnRenameBtnClicked();
	void OnDuplicateBtnClicked();
	void OnDeleteBtnClicked();
	void OnAdvBtnClicked();

private:
	void SetMouseStatus(const char *status);
	void SetButtonVisible(bool visible);
	void SetCurrentStyles(bool current);
	int GetButtonWidth();
	void SetLeftRightMargin();
	void DrawDropLine(QPainter *painter);

signals:
	void applyClicked(const QString &name, const QString &path, bool textMode);
	void exportClicked(const QString &name, const QString &path);
	void renameClicked(const QString &name, const QString &path);
	void duplicateClicked(const QString &name, const QString &path);
	void deleteClicked(const QString &name, const QString &path);

private:
	Ui::PLSSceneCollectionItem *ui;
	QString fileName{};
	QString filePath{};
	QString timeText{};
	bool current{false};
	bool textMode{false};
	bool buttonVisible{false};
	bool timeVisible{false};
	DropLine lineType{DropLine::DropLineNone};
	int leftMargin{0};
	int rightMargin{0};
	int nameRealWidth{0};
	bool advMenuShow{false};
};

#endif // PLSSCENECOLLECTIONITEM_H
