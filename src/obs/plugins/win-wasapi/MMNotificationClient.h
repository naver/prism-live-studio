#pragma once
#include <windows.h>
#include <string>
#include <mutex>
#include <Mmdeviceapi.h>

class IWASEventCallback {
public:
	virtual ~IWASEventCallback() {}

	virtual void OnDefaultInputDeviceChanged(const std::wstring &id) = 0;
	virtual void OnDefaultOutputDeviceChanged(const std::wstring &id) = 0;
};

class CMMNotificationClient : public IMMNotificationClient {
public:
	CMMNotificationClient(IWASEventCallback *pCb);
	virtual ~CMMNotificationClient();

	std::wstring GetDefaultDevice(bool isInputDevice);

	ULONG STDMETHODCALLTYPE AddRef();
	ULONG STDMETHODCALLTYPE Release();
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid,
						 VOID **ppvInterface);

protected:
	HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR pwstrDeviceId,
						       DWORD dwNewState);
	HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR pwstrDeviceId);
	HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR pwstrDeviceId);
	HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow,
							 ERole role,
							 LPCWSTR pwstrDeviceId);
	HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR pwstrDeviceId,
							 const PROPERTYKEY key);

private:
	LONG m_nRef;
	IWASEventCallback *m_pCb;

	std::mutex lock;
	std::wstring m_wstrDefaultInput = L"";
	std::wstring m_wstrDefaultOutput = L"";
};
