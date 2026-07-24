#include "prism-ipc-buffer.h"
#include "handle-wrapper.h"

#pragma comment(lib, "WinMM.lib")

inline int ALIGN_NUM(int bytes, int align)
{
	auto ret = (((bytes) + ((align)-1)) & ~((align)-1));
	return ret;
}

static const auto FILEMAP_BUFFER_CHANGED_EVENT = "data-changed-event";
static const auto FILEMAP_EXTENSION_CHANGED_EVENT = "ext-changed-event";

static const auto FILEMAP_BUFFER_VIEW = "buffer-name";
static const auto FILEMAP_BUFFER_MUTEX_TOP = "mutex-top";
static const auto FILEMAP_BUFFER_MUTEX_ITEM = "mutex-item";

CircleBufferIPC::CircleBufferIPC(const char *queueName, int extendSize, int itemHdrSz, int itemCount, int itemSampleSize, int alignment)
	: strQueueName(queueName ? queueName : ""), itemHeaderSize(itemHdrSz), extendInfoSize(extendSize), itemTotalCount(itemCount), itemBufferSize(itemSampleSize), mutexItems(new HANDLE[itemCount])
{
	assert(queueName && itemCount > 0 && itemSampleSize > 0);

	headerSizeAlign = ALIGN_NUM(extendSize + sizeof(QueueInfo), alignment);
	itemSizeAlign = ALIGN_NUM(itemHdrSz + itemSampleSize, alignment);

	bufUpdateEvent = CHandleWrapper::GetEvent(GenerateName(FILEMAP_BUFFER_CHANGED_EVENT, queueName, 0).c_str(), false);
	extUpdateEvent = CHandleWrapper::GetEvent(GenerateName(FILEMAP_EXTENSION_CHANGED_EVENT, queueName, 0).c_str(), false);
	ResetEvent(bufUpdateEvent);
	ResetEvent(extUpdateEvent);

	mutexTop = CHandleWrapper::GetMutex(GenerateName(FILEMAP_BUFFER_MUTEX_TOP, queueName, 0).c_str());
	for (int i = 0; i < itemCount; ++i) {
		mutexItems.get()[i] = CHandleWrapper::GetMutex(GenerateName(FILEMAP_BUFFER_MUTEX_ITEM, queueName, i).c_str());
	}
}

CircleBufferIPC::~CircleBufferIPC()
{
	if (mapViewOfFile) {
		UnmapViewOfFile(mapViewOfFile);
		mapViewOfFile = nullptr;
	}

	CHandleWrapper::CloseHandleEx(mapHandle);

	CHandleWrapper::CloseHandleEx(bufUpdateEvent);
	CHandleWrapper::CloseHandleEx(extUpdateEvent);

	CHandleWrapper::CloseHandleEx(mutexTop);
	if (mutexItems) {
		for (int i = 0; i < itemTotalCount; ++i)
			CHandleWrapper::CloseHandleEx(mutexItems.get()[i]);
	}
}

DWORD CircleBufferIPC::InitMapBuffer(std::string &errorMsg)
{
	CAutoLockMutex al(mutexTop);
	DWORD error = 0;

	errorMsg = "";
	if (IsBufferValid())
		return 0;

	int mapTotalSize = headerSizeAlign + itemTotalCount * itemSizeAlign;
	std::string mapName = GenerateName(FILEMAP_BUFFER_VIEW, strQueueName.c_str(), 0);

	bool bNewCreate;
	mapHandle = CHandleWrapper::GetMap(mapName.c_str(), mapTotalSize, &bNewCreate);

	if (!mapHandle) {
		assert(false);
		error = GetLastError();
		errorMsg = "Invalid mapHandle";
		return error;
	}

	mapViewOfFile = MapViewOfFile(mapHandle, FILE_MAP_ALL_ACCESS, 0, 0, mapTotalSize);
	if (!mapViewOfFile) {
		assert(false);
		error = GetLastError();
		errorMsg = "Invalid mapViewOfFile";
		return error;
	}

	if (bNewCreate)
		memset(mapViewOfFile, 0, mapTotalSize);

	FormatExtensionView(mapViewOfFile, bNewCreate);
	FormatMapView();

	return 0;
}

bool CircleBufferIPC::IsBufferValid() const
{
	return !!mapViewOfFile;
}

void CircleBufferIPC::WaitBufferChanged(DWORD dwWaitMilliSecond)
{
	// Because the event will be reset automally and it may be waiting by multiple objects,
	// So we'd better make sure the waiting time should not be too long.
	assert(dwWaitMilliSecond < 20);

	if (!IsBufferValid() || !CHandleWrapper::IsHandleValid(bufUpdateEvent)) {
		assert(false);
		return;
	}

	bool data_in = false;
	{
		CAutoLockMutex al(mutexTop);
		data_in = IsDataReady();
	}

	if (!data_in) {
		// should not wait signal except there is no data to read
		CHandleWrapper::IsHandleSigned(bufUpdateEvent, dwWaitMilliSecond);
	}
}

bool CircleBufferIPC::WaitExtendChanged(DWORD dwWaitMilliSecond)
{
	// Because the event will be reset automally and it may be waiting by multiple objects,
	// So we'd better make sure the waiting time should not be too long.
	assert(dwWaitMilliSecond < 20);
	assert(false && "not implemented yet");

	if (CHandleWrapper::IsHandleSigned(extUpdateEvent, dwWaitMilliSecond)) {
		return true;
	} else {
		return false;
	}
}

bool CircleBufferIPC::WriteItemData(void *hdr, int hdrWriteSize, void *data, int dataWriteSize)
{
	int index = WriteItemInner(&hdr, hdrWriteSize, &data, dataWriteSize, false);
	return (index >= 0);
}

int CircleBufferIPC::MapWrite(void **hdr, int hdrSize, void **buf, int dataSize)
{
	return WriteItemInner(hdr, hdrSize, buf, dataSize, true);
}

void CircleBufferIPC::UnmapWrite(int index)
{
	if (index >= 0 && index < itemTotalCount) {
		ReleaseMutex(mutexItems.get()[index]);
		SetEvent(bufUpdateEvent);
	}
}

int CircleBufferIPC::WriteItemInner(void **hdr, int hdrWriteSize, void **data, int dataWriteSize, bool keepLockItem)
{
	if (!IsBufferValid() || hdrWriteSize != itemHeaderSize || dataWriteSize > itemBufferSize) {
		assert(false);
		return -1;
	}

	int writeIndex = -1;
	{
		CAutoLockMutex al(mutexTop);
		if (!CheckHeaderValid(hdr))
			return -1;

		if (0 == itemTotalCount) {
			assert(false);
			return -1;
		}

		writeIndex = int((queueInfo->writenCount++) % itemTotalCount);
		WaitForSingleObject(mutexItems.get()[writeIndex],
				    INFINITE); // before releasing top lock, request item lock
	}

	if (keepLockItem) {
		*hdr = itemArray.get()[writeIndex].hdr;
		*data = itemArray.get()[writeIndex].data;

	} else {
		memcpy(itemArray.get()[writeIndex].hdr, *hdr, hdrWriteSize);
		memcpy(itemArray.get()[writeIndex].data, *data, dataWriteSize);

		ReleaseMutex(mutexItems.get()[writeIndex]);
		SetEvent(bufUpdateEvent);
	}

	return writeIndex;
}

int CircleBufferIPC::ReadItemInner(void **hdr, int hdrReadSize, void **buf, int dataReadSize, bool keepLockItem)
{
	if (!IsBufferValid() || hdrReadSize != itemHeaderSize || dataReadSize > itemBufferSize) {
		assert(false);
		return -1;
	}

	int index = -1;
	ItemInfo item;
	memset(&item, 0, sizeof(ItemInfo));

	{
		CAutoLockMutex al(mutexTop);
		if (IsDataReady()) {
			if (0 == itemTotalCount) {
				assert(false);
				return -1;
			}

			index = int((readedCount++) % itemTotalCount);
			item = itemArray.get()[index];
			if (!CheckHeaderValid(item.hdr))
				return -1;

			// before releasing top lock, request item lock
			WaitForSingleObject(mutexItems.get()[index], INFINITE);
		}
	}

	preReadTime = timeGetTime();
	if (index >= 0) {
		if (keepLockItem) { // this if for maping
			*hdr = item.hdr;
			*buf = item.data;
		} else {
			memcpy(*hdr, item.hdr, hdrReadSize);
			memcpy(*buf, item.data, dataReadSize);
			ReleaseMutex(mutexItems.get()[index]);
		}

		return index;
	} else {
		return -1;
	}
}

bool CircleBufferIPC::ReadItemData(void *hdr, int hdrReadSize, void *buf, int dataReadSize)
{
	int index = ReadItemInner(&hdr, hdrReadSize, &buf, dataReadSize, false);
	return (index >= 0);
}

int CircleBufferIPC::MapRead(void **hdr, int hdrReadSize, void **buf, int dataReadSize)
{
	return ReadItemInner(hdr, hdrReadSize, buf, dataReadSize, true);
}

void CircleBufferIPC::UnmapRead(int index) const
{
	if (index >= 0 && index < itemTotalCount) {
		ReleaseMutex(mutexItems.get()[index]);
	}
}

void CircleBufferIPC::DiscardCurrentData()
{
	if (!IsBufferValid()) {
		assert(false);
		return;
	}

	CAutoLockMutex al(mutexTop);
	readedCount = queueInfo->writenCount;
	if (readedCount < 0)
		readedCount = 0;
}

std::string CircleBufferIPC::GenerateName(const char *hdr, const char *ext, int index) const
{
	char temp[200];
	sprintf_s(temp, ARRAYSIZE(temp), "%s-%s-%d", hdr, ext, index);
	return std::string(temp);
}

void CircleBufferIPC::FormatMapView()
{
	BYTE *localPos = (BYTE *)mapViewOfFile + extendInfoSize;
	queueInfo = reinterpret_cast<QueueInfo *>(localPos);

	itemArray = std::shared_ptr<ItemInfo>(new ItemInfo[itemTotalCount], std::default_delete<ItemInfo[]>());
	assert(itemArray);
	if (itemArray) {
		BYTE *startPos = (BYTE *)mapViewOfFile + headerSizeAlign;
		for (int i = 0; i < itemTotalCount; ++i) {
			BYTE *itemBegin = startPos + i * itemSizeAlign;
			itemArray.get()[i].hdr = itemBegin;
			itemArray.get()[i].data = itemBegin + itemHeaderSize;
		}
	}
}

bool CircleBufferIPC::IsDataReady()
{
	if (queueInfo->writenCount <= 0) // no data in buffer
		return false;

	LONGLONG latestReadCount = (queueInfo->writenCount - 1);
	if (readedCount <= 0 && ShouldDiscardOutdateData()) // first time to read data
	{
		readedCount = latestReadCount; // start reading from latest frame
		return true;
	}

	if (readedCount > latestReadCount)
		return false; // there is no more frame to be readed

	// There are frames staged in buffer
	// If you are late to read buffer, circle queue may be overwritten. Then you should update your read index
	if (queueInfo->writenCount > itemTotalCount && readedCount < (queueInfo->writenCount - itemTotalCount)) {
		printf("Warning : Lost data because late to read buffer. writeCount:%lld readCount:%lld readSpace:%lu ms\n", queueInfo->writenCount, readedCount, timeGetTime() - preReadTime);

		if (ShouldDiscardOutdateData()) {
			// jump to latest item
			readedCount = latestReadCount;
		} else {
			// jump to oladest valid item
			readedCount = (queueInfo->writenCount - itemTotalCount);
		}
	}

	return true;
}
