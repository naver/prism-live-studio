#ifndef PLSSCHEDULEMENUITEM_H
#define PLSSCHEDULEMENUITEM_H

#include <QWidget>
#include <QLabel>
#include <qmetatype.h>
#include <vector>
#include <QTimer>
#include <QWidget>

enum class PLSScheduleItemType {
	Ty_NormalLive,
	Ty_Loading,
	Ty_Placehoder,
	Ty_Schedule,
};

struct ComplexItemData {
	QString _id;
	QString title;
	long timeStamp;
	PLSScheduleItemType type;
	QString time;
};

Q_DECLARE_METATYPE(ComplexItemData)

class PLSScheduleMenuItem : public QWidget {
	Q_OBJECT
public:
	explicit PLSScheduleMenuItem(const ComplexItemData data, int superWidth, QWidget *parent = nullptr);
	~PLSScheduleMenuItem();

private:
	QLabel *titleLabel;
	QLabel *detailLabel;

	QLabel *loadingDetailLabel;

	const ComplexItemData _data;
	QTimer *m_pTimer;

	void setupTimer();

private slots:
	void updateProgress();

protected:
	void enterEvent(QEvent *event);
	void leaveEvent(QEvent *event);
};

#endif // PLSSCHEDULEMENUITEM_H
