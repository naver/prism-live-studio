#pragma once
#include <Windows.h>
#include <assert.h>
#include <string>
#include <memory>

/*
Format of mapped view:
-----------------------------------------------------------------------------------------------------------
| extensioninfo | queueInfo | itemHeader + itemData | itemHeader + itemData | ... |
-----------------------------------------------------------------------------------------------------------
An instance of CircleBufferIPC should be using for either reading data or writing data;
If it is for reading, it cannot invoke WriteItemData(...).
Similarly if it is for writing, it cannot invoke ReadItemData(...).
*/

static const auto DEFAULT_WAIT_IPC_BUFFER = 10; // in ms

class CircleBufferIPC {
public:
	virtual ~CircleBufferIPC();

	// return 0 if success, otherwise return GetLastError()
	DWORD InitMapBuffer(std::string &errorMsg);
	bool IsBufferValid() const;

	// writing data
	bool WriteItemData(void *hdr, int hdrWriteSize, void *data, int dataWriteSize);
	int MapWrite(void **hdr, int hdrSize, void **buf, int dataSize);
	void UnmapWrite(int index);

	// reading data
	void WaitBufferChanged(DWORD dwWaitMilliSecond = DEFAULT_WAIT_IPC_BUFFER);
	bool WaitExtendChanged(DWORD dwWaitMilliSecond = DEFAULT_WAIT_IPC_BUFFER);
	bool ReadItemData(void *hdr, int hdrReadSize, void *buf, int dataReadSize);
	// returned index : valid when value >= 0
	int MapRead(void **hdr, int hdrReadSize, void **buf, int dataReadSize);
	void UnmapRead(int index) const;

	void DiscardCurrentData();

	virtual void SetExtensionInfo(const void *input) { assert(false); }
	virtual void GetExtensionInfo(void *output) { assert(false); }

protected:
	explicit CircleBufferIPC(const char *queueName, int extendSize, int itemHeaderSize, int itemCount, int itemSampleSize, int alignment = 64);

	virtual void FormatExtensionView(void *ptr, bool bNewCreate) {}
	virtual bool CheckHeaderValid(const void *hdr) { return true; }
	virtual bool ShouldDiscardOutdateData() { return true; }

private:
	int WriteItemInner(void **hdr, int hdrWriteSize, void **data, int dataWriteSize, bool keepLockItem);
	int ReadItemInner(void **hdr, int hdrReadSize, void **buf, int dataReadSize, bool keepLockItem);
	std::string GenerateName(const char *hdr, const char *ext, int index) const;
	void FormatMapView();
	bool IsDataReady();

protected:
#pragma pack(push, 1) // Note: keep memory alignment during IPC
	struct QueueInfo {
		LONGLONG writenCount; // the count that have been written into buffer
	};
	struct ItemInfo {
		void *hdr;
		void *data;
	};
#pragma pack(pop)

	const std::string strQueueName = "";
	const int itemHeaderSize = 0;
	const int extendInfoSize = 0;
	const int itemTotalCount = 0;
	const int itemBufferSize = 0;

	int headerSizeAlign = 0; // extendInfo + LocalQueueInfo
	int itemSizeAlign = 0;   // itemHeader + itemBuffer

	// readedCount: the index that currently to be readed, the real index should be (readedCount % itemTotalCount)
	LONGLONG readedCount = 0;
	DWORD preReadTime = 0;

	HANDLE mapHandle = nullptr;
	LPVOID mapViewOfFile = nullptr;
	QueueInfo *queueInfo = nullptr;
	std::shared_ptr<ItemInfo> itemArray = nullptr;

	// Warning:
	// While requesting lock, you must firstly lock mutexTop, then item's.
	// If you request mutexTop after getting item's lock, it may cause dead-lock.
	HANDLE mutexTop = nullptr;
	std::shared_ptr<HANDLE> mutexItems = nullptr;

public:
	HANDLE bufUpdateEvent = nullptr;
	HANDLE extUpdateEvent = nullptr;
};
