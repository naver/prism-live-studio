#include "PLSDateFormate.h"
#include <QDateTime>
#include <QLocale>
#include <qregularexpression.h>

QString PLSDateFormate::timeStampToUTCString(qint64 timeStamp)
{
	if (timeStamp <= 0) {
		return "";
	}

	QDateTime dateTime = QDateTime::fromSecsSinceEpoch(timeStamp);

	int timeOffsest = dateTime.offsetFromUtc() / 3600;
	QString timeZoneString = QString(" UTC%1%2").arg(timeOffsest < 0 ? "" : "+").arg(timeOffsest);

	dateTime.setTimeSpec(Qt::UTC);

	QLocale local = QLocale::English;
	QString formateStr = "MM/dd/yyyy hh:mm AP";
	QString enTimeStr = local.toString(dateTime, formateStr) + timeZoneString;

	return enTimeStr;
}

QString PLSDateFormate::timeStampToShortString(long timeStamp)
{
	if (timeStamp <= 0) {
		return "";
	}
	QDateTime dateTime = QDateTime::fromSecsSinceEpoch(timeStamp);
	dateTime.setTimeSpec(Qt::UTC);
	QLocale local = QLocale::English;
	QString formateStr = "MM/dd hh:mm AP";
	QString enTimeStr = local.toString(dateTime, formateStr);

	return enTimeStr;
}

long PLSDateFormate::timeStringToStamp(QString time)
{
	if (time.isEmpty()) {
		return 0;
	}
	QString tempTime = time;
	tempTime.remove(QRegularExpression("\\.[0-9]+")); //remove old type time string
	QDateTime stDTime = QDateTime::fromString(tempTime, "yyyy-MM-dd'T'HH:mm:ss'Z'");
	return stDTime.toSecsSinceEpoch() + QDateTime::currentDateTime().offsetFromUtc();
}

long PLSDateFormate::koreanTimeStampToLocalTimeStamp(QString time)
{
	if (time.isEmpty()) {
		return 0;
	}

	QString tempTime = time;
	tempTime.remove(QRegularExpression("\\.[0-9]+")); //remove old type time string
	QDateTime stDTime = QDateTime::fromString(tempTime, "yyyy-MM-dd'T'HH:mm:ss");
	int koreanLocal = 9;
	long finalTime = stDTime.toSecsSinceEpoch() - koreanLocal * 3600 + QDateTime::currentDateTime().offsetFromUtc();
	return finalTime;
}

QString PLSDateFormate::youtubeTimeStampToString(long timeStamp)
{
	if (timeStamp <= 0) {
		return "";
	}
	auto utc0Time = timeStamp - QDateTime::currentDateTime().offsetFromUtc();
	QDateTime dateTime = QDateTime::fromSecsSinceEpoch(utc0Time);
	dateTime.setTimeSpec(Qt::UTC);
	QLocale local = QLocale::English;
	QString formateStr = "yyyy-MM-dd'T'HH:mm:ss'Z'";
	QString enTimeStr = local.toString(dateTime, formateStr);
	return enTimeStr;
}

long PLSDateFormate::vliveTimeStringToStamp(QString time)
{
	QDateTime stDTime = QDateTime::fromString(time, "yyyy-MM-dd HH:mm");
	int koreanLocal = 9;
	long finalTime = stDTime.toSecsSinceEpoch() - koreanLocal * 3600 + QDateTime::currentDateTime().offsetFromUtc();

	return finalTime;
}

QString PLSDateFormate::vliveTimeStampToString(long timeStamp)
{
	if (timeStamp <= 0) {
		return "";
	}
	QDateTime dateTime = QDateTime::fromSecsSinceEpoch(timeStamp);
	dateTime.setTimeSpec(Qt::UTC);
	QLocale local = QLocale::English;
	QString formateStr = "yyyy-MM-dd HH:mm";
	QString enTimeStr = local.toString(dateTime, formateStr);

	return enTimeStr;
}

QString PLSDateFormate::navertvTimeStampToString(qint64 timeStamp)
{
	if (timeStamp <= 0) {
		return "";
	}

	QDateTime dateTime = QDateTime::fromSecsSinceEpoch(timeStamp / 1000);
	dateTime.setTimeSpec(Qt::UTC);
	QLocale local = QLocale::Korean;
	QString formateStr = "yyyy-MM-dd HH:mm AP t";
	QString enTimeStr = local.toString(dateTime, formateStr);

	return enTimeStr;
}

long PLSDateFormate::iso8601ToStamp(QString time)
{
	if (time.isEmpty()) {
		return 0;
	}
	QDateTime stDTime = QDateTime::fromString(time, Qt::ISODate);
	return stDTime.toSecsSinceEpoch();
}

long PLSDateFormate::getNowTimeStamp()
{
	return QDateTime::currentDateTime().toSecsSinceEpoch();
}

bool PLSDateFormate::isToday(qint64 timestamp)
{
	QDateTime inputDateTime = QDateTime::fromSecsSinceEpoch(timestamp);
	QDateTime currentDateTime = QDateTime::currentDateTime();
	return inputDateTime.date() == currentDateTime.date();
}

int PLSDateFormate::daysDifferenceFromNow(qint64 timestamp)
{
	if (timestamp <= 0) {
		return 0;
	}

	QDateTime inputDateTime = QDateTime::fromSecsSinceEpoch(timestamp);
	QDateTime currentDateTime = QDateTime::currentDateTime();
	return currentDateTime.date().daysTo(inputDateTime.date());
}
