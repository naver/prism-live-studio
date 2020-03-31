#ifndef PLSCOLORFILTERVIEW_H
#define PLSCOLORFILTERVIEW_H

#include <QFrame>
#include <QPushButton>
#include <QLabel>
#include <QVariant>

#include "obs.hpp"

namespace Ui {
class PLSColorFilterView;
}

class PLSColorFilterImage;
class PLSColorFilterView : public QFrame {
	Q_OBJECT

public:
	explicit PLSColorFilterView(obs_source_t *source, QWidget *parent = nullptr);
	~PLSColorFilterView();
	void ResetColorFilter();
	OBSSource GetSource();

protected:
	virtual void showEvent(QShowEvent *event) override;

private slots:
	void OnLeftScrollBtnClicked();
	void OnRightScrollBtnClicked();
	void OnColorFilterImageMousePressd(PLSColorFilterImage *image);

private:
	int GetScrollAreaWidth();
	void CreateColorFilterImage(const QString &name, const QString &shortTitle, const QString &filterPath, const QString &thumbnailPath);
	void UpdateFilterItemStyle(PLSColorFilterImage *image);
	void UpdateFilterItemStyle(const QString &imagePath);
	void UpdateColorFilterSource(const QString &path);
	void LoadDefaultColorFilter();
	QString GetDefaultColorFilterPath();
	void SetDefaultCurrentColorFilter();

signals:
	void OriginalPressed(bool state);

private:
	Ui::PLSColorFilterView *ui;
	OBSSource source;
	QPushButton *leftScrollBtn{};
	QPushButton *rightScrollBtn{};
	std::vector<PLSColorFilterImage *> ImageVec;
};

class PLSColorFilterImage : public QPushButton {
	Q_OBJECT
public:
	PLSColorFilterImage(const QString &name, const QString &shortTitle, const QString &path, const QString &thumbnail, QWidget *parent = nullptr);
	QString GetPath();
	QString GetThumbnailPath();
	void SetClickedState(bool clicked);
	void SetPixMap(const QPixmap &pixmap);
	bool GetClickedState();

protected:
	virtual void mousePressEvent(QMouseEvent *event) override;
	virtual void paintEvent(QPaintEvent *event) override;

private:
	void SetClickedStyle(bool clicked);

signals:
	void MousePressd(PLSColorFilterImage *image);
	void ClickedStateChanged(bool clicked);

private:
	QString name;
	QString path;
	QString thumbnail;
	QString shortTitle;
	bool clicked{false};
	QPixmap pixmap;
};

#endif // PLSCOLORFILTERVIEW_H
