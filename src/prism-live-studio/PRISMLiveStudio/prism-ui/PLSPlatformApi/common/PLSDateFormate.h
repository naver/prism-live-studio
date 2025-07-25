#pragma once

#include <QObject>

class PLSDateFormate : public QObject {
	Q_OBJECT
public:
	/**
 *  @param 1527840000
 *  @return  02/07/2020 11:40AM UTC+9
 */
	static QString timeStampToUTCString(qint64 timeStamp);
	/**
 *  @param 1527840000
 *  @return  02/07/2020 11:40
 */
	static QString timeStampToShortString(long timeStamp);

	/**
 *  @param 2020-02-29T07:00:00.000Z || 2020-02-29T07:00:00Z
 *  @return 1527840000
 */
	static long timeStringToStamp(QString time);

	/**
 *  @param 2020-02-29T07:00:00.000
 *  @return 1527840000
 */
	static long koreanTimeStampToLocalTimeStamp(QString time);

	/**
 *  @param 1527840000 
 *  @return 2020-02-29T07:00:00Z
 */
	static QString youtubeTimeStampToString(long timeStamp);
	/**
 *  @param 2020-04-14 15:54
 *  @return 1527840000
 */
	static long vliveTimeStringToStamp(QString time);

	/**
 *  @param 1527840000
 *  @return 2020-04-14 15:54
 */
	static QString vliveTimeStampToString(long timeStamp);

	static QString navertvTimeStampToString(qint64 timeStamp);

	/**
 *  @param 2020-11-14T21:22:24+09:00
 *  @return 1605356544
 */
	static long iso8601ToStamp(QString time);

	static long getNowTimeStamp();

	/**
 *  @param 1605356544
 */
	static bool isToday(qint64 timestamp);

	static int daysDifferenceFromNow(qint64 timestamp);
};
