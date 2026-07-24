#ifndef PLSSTICKERTOASTFRAME_H
#define PLSSTICKERTOASTFRAME_H

#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QTextEdit>

class PLSStickerToastFrame : public QFrame {
	Q_OBJECT
public:
	explicit PLSStickerToastFrame(QWidget *parent = nullptr);
	~PLSStickerToastFrame() override;

	void SetMessage(const QString &message);
	QString GetMessageContent() const;
	void ShowToast();
	void HideToast();
	void calcFixedHeight();

protected:
	void resizeEvent(QResizeEvent *event) override;
	void showEvent(QShowEvent *event) override;

private:
	QPushButton *btnClose = nullptr;
	QTextEdit *editMessage = nullptr;
	QTimer timerDisappear;
};

#endif // PLSSTICKERTOASTFRAME_H
