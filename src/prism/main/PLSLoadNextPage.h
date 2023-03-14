#ifndef PLSLoadNextPage_H
#define PLSLoadNextPage_H

#include <functional>

#include <QObject>

class QScrollArea;
class QScrollBar;

class PLSLoadNextPage : public QObject {
	Q_OBJECT

private:
	explicit PLSLoadNextPage(QScrollArea *parent);
	~PLSLoadNextPage();

public:
	static PLSLoadNextPage *newLoadNextPage(PLSLoadNextPage *&loadNextPage, QScrollArea *scrollArea);
	static void deleteLoadNextPage(PLSLoadNextPage *&loadNextPage);

	template<typename Type>
	static PLSLoadNextPage *newLoadNextPage(PLSLoadNextPage *&loadNextPage, QScrollArea *scrollArea, Type *receiver, void (Type::*slot)(), Qt::ConnectionType connectionType = Qt::QueuedConnection)
	{
		newLoadNextPage(loadNextPage, scrollArea);
		connect(loadNextPage, &PLSLoadNextPage::loadNextPage, receiver, slot, connectionType);
		return loadNextPage;
	}
	template<typename Slot> static PLSLoadNextPage *newLoadNextPage(PLSLoadNextPage *&loadNextPage, QScrollArea *scrollArea, Slot slot, Qt::ConnectionType connectionType = Qt::QueuedConnection)
	{
		newLoadNextPage(loadNextPage, scrollArea);
		connect(loadNextPage, &PLSLoadNextPage::loadNextPage, scrollArea, slot, connectionType);
		return loadNextPage;
	}

signals:
	void loadNextPage();

private:
	void onScrollBarValueChanged(int value);

private:
	QMetaObject::Connection scrollBarValueChangedConnection;
	QScrollBar *scrollBar;
	int scrollEndRange;
	bool signalEmited;
};

#endif // PLSLoadNextPage_H
