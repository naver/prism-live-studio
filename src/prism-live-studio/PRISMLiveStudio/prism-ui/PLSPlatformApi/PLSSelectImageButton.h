#ifndef PLSSELECTIMAGEBUTTON_H
#define PLSSELECTIMAGEBUTTON_H

#include <QLabel>

namespace Ui {
class PLSSelectImageButton;
}

class PLSSelectImageButton : public QLabel {
	Q_OBJECT
	Q_PROPERTY(QString lang READ getLanguage)

public:
	using ImageChecker = std::function<QPair<bool, QString>(const QPixmap &image, const QString &imagePath)>;

	explicit PLSSelectImageButton(QWidget *parent = nullptr);
	~PLSSelectImageButton() override;

	QString getLanguage() const;

	QString getImagePath() const;
	void setImagePath(const QString &imagePath);
	void setPixmap(const QPixmap &pixmap, const QSize &size = QSize());
	QPixmap getOriginalPixmap() const { return originPixmap; };

	void setButtonEnabled(bool enabled);

	void setImageSize(const QSize &imageSize);
	void setImageChecker(const ImageChecker &imageChecker);

	void mouseEnter();
	void mouseLeave();

	void setIsIgoreMinSize(bool isIgoreMinSize) { m_isIgoreMinSize = isIgoreMinSize; };

signals:
	void takeButtonClicked();
	void selectButtonClicked();
	void imageSelected(const QString &imageFilePath);

private slots:
	void on_takeButton_clicked();
	void on_selectButton_clicked();

protected:
	bool event(QEvent *event) override;
	bool eventFilter(QObject *watched, QEvent *event) override;
	bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;

private:
	static QPair<bool, QString> defaultImageChecker(const QPixmap &, const QString &);
	void moveIconToCenter(const QSize &containerSize);
	void setRemoveRetainSizeWhenHidden(QWidget *widget) const;

	static const int MIN_PHOTO_WIDTH = 440;
	static const int MIN_PHOTO_HEIGHT = 245;
	static const int MAX_PHOTO_WIDTH = 3840;
	static const int MAX_PHOTO_HEIGHT = 2160;

	Ui::PLSSelectImageButton *ui = nullptr;
	QLabel *icon = nullptr;
	bool mouseHover = false;
	QString imagePath;
	QSize imageSize{MIN_PHOTO_WIDTH, MIN_PHOTO_HEIGHT};
	ImageChecker imageChecker{defaultImageChecker};
	QPixmap originPixmap;
	bool m_isIgoreMinSize = false;
};

#endif // PLSSELECTIMAGEBUTTON_H
