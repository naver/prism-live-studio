#ifndef PLSTOASTMSGFRAME_H
#define PLSTOASTMSGFRAME_H

#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QTextEdit>

class PLSToastMsgFrame : public QFrame {
	Q_OBJECT
public:
	explicit PLSToastMsgFrame(QWidget *parent = nullptr);
	~PLSToastMsgFrame() override;

	void SetMessage(const QString &message);
	QString GetMessageContent() const;
	void SetShowWidth(int width);
	void ShowToast();
	void HideToast();

protected:
	void resizeEvent(QResizeEvent *event) override;

private slots:
	void resizeToContent();

signals:
	void resizeFinished();

private:
	QPushButton *btnClose = nullptr;
	QTextEdit *editMessage = nullptr;
	int showWidth{0};
	QTimer timerDisappear;
};

#endif // PLSTOASTMSGFRAME_H
