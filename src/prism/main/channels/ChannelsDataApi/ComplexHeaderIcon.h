#ifndef COMPLEXHEADERICON_H
#define COMPLEXHEADERICON_H

#include <QLabel>

/* this is a complex UI for user profile header */
class ComplexHeaderIcon : public QLabel {
	Q_OBJECT
	Q_PROPERTY(bool Active READ isActive WRITE setActive)
public:
	explicit ComplexHeaderIcon(QWidget *parent = nullptr);
	~ComplexHeaderIcon() override;

	void setPixmap(const QString &pix, bool sharp = true);
	void setPlatformPixmap(const QString &pix, bool sharp = true);
	void setActive(bool isActive = true);
	bool isActive() { return mActive; }

protected:
	void paintEvent(QPaintEvent *event) override;

private:
	QPixmap mBigPix;
	QPixmap mSmallPix;
	bool mActive;
};

#endif // COMPLEXHEADERICON_H
