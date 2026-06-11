#include <Windows.h>
#include <netlistmgr.h>

#include <atomic>

#include <private/qobject_p.h>

#include "libutils-api.h"
#include "libutils-api-log.h"
#include "network-state.h"

#pragma comment(lib, "ole32.lib")

namespace pls {

constexpr auto NETWORK_STATE_MODULE = "libutils-api/network-state";

template<typename T> void releaseComObject(T *&comObject)
{
	if (comObject) {
		comObject->Release();
		comObject = nullptr;
	}
}

class NetworkEvent : public INetworkListManagerEvents {
public:
	explicit NetworkEvent(NetworkStatePrivate *nsp) : m_nsp(nsp) {}
	virtual ~NetworkEvent() = default;

	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject) override;
	ULONG STDMETHODCALLTYPE AddRef(void) override;
	ULONG STDMETHODCALLTYPE Release(void) override;
	HRESULT STDMETHODCALLTYPE ConnectivityChanged(NLM_CONNECTIVITY connectivity) override;

private:
	std::atomic<ULONG> m_rc = 1;
	NetworkStatePrivate *m_nsp = nullptr;
};

class NetworkStatePrivate : public QObjectPrivate {
	Q_DECLARE_PUBLIC(NetworkState)

	enum class Status {
		Unknown = 0,
		Connected,
		Disconnected,
	};

	bool m_init = false;
	INetworkListManager *m_nlmgr = nullptr;
	IConnectionPointContainer *m_cpc = nullptr;
	IConnectionPoint *m_cp = nullptr;
	NetworkEvent *m_nes = nullptr;
	DWORD m_advise = 0;
	Status m_status = Status::Unknown;

public:
	bool init()
	{
		m_nes = pls_new<NetworkEvent>(this);

		if (HRESULT hr = CoCreateInstance(CLSID_NetworkListManager, nullptr, CLSCTX_ALL, IID_INetworkListManager, (LPVOID *)&m_nlmgr); FAILED(hr)) {
			PLS_WARN(NETWORK_STATE_MODULE, "Failed to init INetworkListManager, 0X%X", hr);
			return false;
		}

		if (HRESULT hr = m_nlmgr->QueryInterface(IID_IConnectionPointContainer, (void **)&m_cpc); FAILED(hr)) {
			PLS_WARN(NETWORK_STATE_MODULE, "Failed to init IConnectionPointContainer, 0X%X", hr);
			return false;
		}

		if (HRESULT hr = m_cpc->FindConnectionPoint(IID_INetworkListManagerEvents, &m_cp); FAILED(hr)) {
			PLS_WARN(NETWORK_STATE_MODULE, "Failed to init IConnectionPoint, 0X%X", hr);
			return false;
		}

		if (HRESULT hr = m_cp->Advise(m_nes, &m_advise); FAILED(hr)) {
			PLS_WARN(NETWORK_STATE_MODULE, "Failed to invoke Advise(), 0X%X", hr);
			return false;
		}

		if (NLM_CONNECTIVITY connectivity; SUCCEEDED(m_nlmgr->GetConnectivity(&connectivity))) {
			m_nes->ConnectivityChanged(connectivity);
		}

		m_init = true;

		PLS_INFO(NETWORK_STATE_MODULE, "Successed to init NetworkState. network state : %s", isAvailable() ? "Connected" : "Disconnected");
		return true;
	}

	bool start()
	{
		if (init()) {
			return true;
		}

		stop();
		return false;
	}
	void stop()
	{
		PLS_INFO(NETWORK_STATE_MODULE, "To clear NetworkState");

		if (m_cp && m_advise != 0) {
			PLS_INFO(NETWORK_STATE_MODULE, "Before invoke Unadvise()");
			m_cp->Unadvise(m_advise);
			PLS_INFO(NETWORK_STATE_MODULE, "Finish invoke Unadvise()");
			m_advise = 0;
		}

		releaseComObject(m_nes);
		releaseComObject(m_cp);
		releaseComObject(m_cpc);
		releaseComObject(m_nlmgr);

		PLS_INFO(NETWORK_STATE_MODULE, "Finish clear NetworkState");
	}

	bool isAvailable() const { return m_status == Status::Connected; }
	void stateChanged(bool available)
	{
		if (Status status = available ? Status::Connected : Status::Disconnected; m_status != status) {
			if (m_status != Status::Unknown) {
				PLS_INFO(NETWORK_STATE_MODULE, "Network state is changed : %s", available ? "Connected" : "Disconnected");
			}

			m_status = status;
			Q_Q(NetworkState);
			pls_async_call(q, [q, available]() { emit q->stateChanged(available); });
		}
	}

	static bool start(NetworkState *ns) { return ns->d_func()->start(); }
	static void stop(NetworkState *ns) { ns->d_func()->stop(); }
};

HRESULT STDMETHODCALLTYPE NetworkEvent::QueryInterface(REFIID riid, void **ppvObject)
{
	if (IsEqualIID(riid, IID_IUnknown)) {
		*ppvObject = static_cast<IUnknown *>(this);
		AddRef();
		return S_OK;
	} else if (IsEqualIID(riid, IID_INetworkListManagerEvents)) {
		*ppvObject = static_cast<INetworkListManagerEvents *>(this);
		AddRef();
		return S_OK;
	}
	*ppvObject = nullptr;
	return E_NOINTERFACE;
}
ULONG STDMETHODCALLTYPE NetworkEvent::AddRef(void)
{
	return ++m_rc;
}
ULONG STDMETHODCALLTYPE NetworkEvent::Release(void)
{
	ULONG rc = --m_rc;
	if (rc == 0) {
		pls_delete(this);
	}
	return rc;
}
HRESULT STDMETHODCALLTYPE NetworkEvent::ConnectivityChanged(NLM_CONNECTIVITY connectivity)
{
	m_nsp->stateChanged((connectivity & NLM_CONNECTIVITY_IPV4_INTERNET) || (connectivity & NLM_CONNECTIVITY_IPV6_INTERNET));
	return S_OK;
}

NetworkState::NetworkState() : QObject(*pls_new<NetworkStatePrivate>()) {}

NetworkState *NetworkState::instance()
{
	static NetworkState ns;
	return &ns;
}

bool NetworkState::isAvailable() const
{
	Q_D(const NetworkState);
	return d->isAvailable();
}

bool network_state_start()
{
	return NetworkStatePrivate::start(NetworkState::instance());
}
void network_state_stop()
{
	NetworkStatePrivate::stop(NetworkState::instance());
}

}
