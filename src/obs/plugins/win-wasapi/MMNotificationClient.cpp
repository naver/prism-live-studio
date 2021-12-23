#include "MMNotificationClient.h"

CMMNotificationClient::CMMNotificationClient(IWASEventCallback *pCb)
	: m_nRef(1), m_pCb(pCb)
{
}

CMMNotificationClient::~CMMNotificationClient() {}

std::wstring CMMNotificationClient::GetDefaultDevice(bool isInputDevice)
{
	std::lock_guard<std::mutex> auto_lock(lock);
	if (isInputDevice) {
		return m_wstrDefaultInput;
	} else {
		return m_wstrDefaultOutput;
	}
}

ULONG STDMETHODCALLTYPE CMMNotificationClient::AddRef()
{
	return InterlockedIncrement(&m_nRef);
}

ULONG STDMETHODCALLTYPE CMMNotificationClient::Release()
{
	ULONG ulRef = InterlockedDecrement(&m_nRef);
	if (0 == ulRef)
		delete this;

	return ulRef;
}

HRESULT STDMETHODCALLTYPE
CMMNotificationClient::QueryInterface(REFIID riid, VOID **ppvInterface)
{
	if (IID_IUnknown == riid) {
		AddRef();
		*ppvInterface = (IUnknown *)this;
	} else if (__uuidof(IMMNotificationClient) == riid) {
		AddRef();
		*ppvInterface = (IMMNotificationClient *)this;
	} else {
		*ppvInterface = NULL;
		return E_NOINTERFACE;
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CMMNotificationClient::OnDeviceStateChanged(
	LPCWSTR pwstrDeviceId, DWORD dwNewState)
{
	return S_OK;
}

HRESULT STDMETHODCALLTYPE
CMMNotificationClient::OnDeviceAdded(LPCWSTR pwstrDeviceId)
{
	return S_OK;
}

HRESULT STDMETHODCALLTYPE
CMMNotificationClient::OnDeviceRemoved(LPCWSTR pwstrDeviceId)
{
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CMMNotificationClient::OnDefaultDeviceChanged(
	EDataFlow flow, ERole role, LPCWSTR pwstrDeviceId)
{
	if (pwstrDeviceId && m_pCb) {
		std::wstring devID = pwstrDeviceId;
		bool notifyEvent = false;
		if (flow == eCapture && role == eCommunications) // input device
		{
			{
				std::lock_guard<std::mutex> auto_lock(lock);
				notifyEvent = (m_wstrDefaultInput != devID);
				m_wstrDefaultInput = devID;
			}

			if (notifyEvent) {
				m_pCb->OnDefaultInputDeviceChanged(devID);
			}

		} else if (flow == eRender && role == eConsole) // output device
		{
			{
				std::lock_guard<std::mutex> auto_lock(lock);
				notifyEvent = (m_wstrDefaultOutput != devID);
				m_wstrDefaultOutput = devID;
			}

			if (notifyEvent) {
				m_pCb->OnDefaultOutputDeviceChanged(devID);
			}
		}
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CMMNotificationClient::OnPropertyValueChanged(
	LPCWSTR pwstrDeviceId, const PROPERTYKEY key)
{
	return S_OK;
}
