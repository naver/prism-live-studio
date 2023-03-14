#ifndef PLSSELECTIMAGEBUTTON_H
#define PLSSELECTIMAGEBUTTON_H

#include <QLabel>

#include "PLSDpiHelper.h"

namespace Ui {
class PLSSelectImageButton;
}

class PLSSelectImageButton : public QLabel {
	Q_OBJECT
	Q_PROPERTY(QString lang READ getLanguage)

public:
	using ImageChecker = std::function<QPair<bool, QString>(const QPixmap &image, const QString &imagePath)>;

public:
	explicit PLSSelectImageButton(QWidget *parent = nullptr, PLSDpiHelper dpiHelper = PLSDpiHelper());
	~PLSSelectImageButton();

public:
	QString getLanguage() const;

	QString getImagePath() const;
	void setImagePath(const QString &imagePath);
	void setPixmap(const QPixmap &pixmap, const QSize &size = QSize());
	QPixmap getOriginalPixmap() const { return originPixmap; };

	void setEnabled(bool enabled);

	void setImageSize(const QSize &imageSize);
	void setImageChecker(const ImageChecker &imageChecker);

	void mouseEnter();
	void mouseLeave();

signals:
	void takeButtonClicked();
	void selectButtonClicked();
	void imageSelected(const QString &imageFilePath);

private slots:
	void on_takeButton_clicked();
	void on_selectButton_clicked();

protected:
	virtual bool event(QEvent *event);
	virtual bool eventFilter(QObject *watched, QEvent *event);
	virtual bool nativeEvent(const QByteArray &eventType, void *message, long *result);

private:
	Ui::PLSSelectImageButton *ui;
	QLabel *icon = nullptr;
	bool mouseHover = false;
	QString imagePath;
	QSize imageSize;
	ImageChecker imageChecker;
	QPixmap originPixmap;
};

#endif // PLSSELECTIMAGEBUTTON_H
