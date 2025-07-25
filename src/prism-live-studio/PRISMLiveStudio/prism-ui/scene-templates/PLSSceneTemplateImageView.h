#ifndef PLSSCENETEMPLATEIMAGEVIEW_H
#define PLSSCENETEMPLATEIMAGEVIEW_H

#include <QWidget>
#include "PLSSceneTemplateModel.h"
#include <QLabel>

namespace Ui {
class PLSSceneTemplateImageView;
}

class PLSSceneTemplateImageView : public QWidget {
	Q_OBJECT

public:
	explicit PLSSceneTemplateImageView(QWidget *parent = nullptr);
	~PLSSceneTemplateImageView();

public:
	void updateImagePath(const QString &path);
	void setHasBorder(bool hasBorder);
	const QString &imagePath() const;
	void setSceneName(const QString &sceneName);

	void showAIBadge(const QPixmap &pixmap, bool bLongAIBadge);

protected:
	bool eventFilter(QObject *watched, QEvent *event) override;

signals:
	void clicked(PLSSceneTemplateImageView *imageView);

private:
	void loadImagePixel();

private:
	Ui::PLSSceneTemplateImageView *ui;
	QString m_path;
	QPixmap imagePix;
};

#endif // PLSSCENETEMPLATEIMAGEVIEW_H
