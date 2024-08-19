#include <Windows.h>
#include <Shlwapi.h>
#include <exdisp.h>
#include <Shobjidl.h>
#include <SHLGUID.h>
#include <comip.h>
#include <comutil.h>

// Warning : when name of PRISM is changed, please remember to modify here
#define PRISM_APP_NAME_W L"PRISMLiveStudio.exe"
//#define PRISM_APP_NAME_W L"PRISMLens.exe"

#pragma comment(linker, "/subsystem:\"windows\" /entry:\"mainCRTStartup\"")
#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "comsuppw.lib")

int ShellExecAsUser(LPCTSTR operation, const TCHAR *const pcFileName, const TCHAR *const pcParameters, const TCHAR *const pcDir);

int main()
{
	CoInitialize(NULL);
	const DWORD MAX_PATH_LEN = 1024;
	WCHAR appdir[MAX_PATH_LEN] = {0};
	GetModuleFileNameW(NULL, appdir, MAX_PATH_LEN);
	PathRemoveFileSpecW(appdir);
	ShellExecAsUser(L"open", PRISM_APP_NAME_W, nullptr, appdir);
	CoUninitialize();
	return 0;
}

#define RELEASE_OBJ(X)          \
	do {                    \
		(X)->Release(); \
		(X) = NULL;     \
	} while (0)

#define VALID_HANDLE(H) (((H) != NULL) && ((H) != INVALID_HANDLE_VALUE))

static void MyShellDispatch_AllowSetForegroundWindow(const HWND &hwnd)
{
	DWORD dwProcessId = 0;
	if (GetWindowThreadProcessId(hwnd, &dwProcessId)) {
		if (dwProcessId != 0) {
			AllowSetForegroundWindow(dwProcessId);
		}
	}
}

int ShellExecAsUser(LPCTSTR operation, const TCHAR *const pcFileName, const TCHAR *const pcParameters, const TCHAR *const pcDir)
{
	bool ok = false;
	IShellWindows *psw = NULL;
	HRESULT hr = CoCreateInstance(CLSID_ShellWindows, NULL, CLSCTX_LOCAL_SERVER, IID_PPV_ARGS(&psw));
	if (SUCCEEDED(hr)) {
		HWND desktopHwnd = 0;
		IDispatch *pdisp = NULL;
		_variant_t vEmpty;
		if (S_OK == psw->FindWindowSW(vEmpty.GetAddress(), vEmpty.GetAddress(), SWC_DESKTOP, (long *)&desktopHwnd, SWFO_NEEDDISPATCH, &pdisp)) {
			if (VALID_HANDLE(desktopHwnd)) {
				IShellBrowser *psb;
				hr = IUnknown_QueryService(pdisp, SID_STopLevelBrowser, IID_PPV_ARGS(&psb));
				if (SUCCEEDED(hr)) {
					IShellView *psv = NULL;
					hr = psb->QueryActiveShellView(&psv);
					if (SUCCEEDED(hr)) {
						IDispatch *pdispBackground = NULL;
						HRESULT hr = psv->GetItemObject(SVGIO_BACKGROUND, IID_PPV_ARGS(&pdispBackground));
						if (SUCCEEDED(hr)) {
							MyShellDispatch_AllowSetForegroundWindow(desktopHwnd);
							IShellFolderViewDual *psfvd = NULL;
							HRESULT hr = pdispBackground->QueryInterface(IID_PPV_ARGS(&psfvd));
							if (SUCCEEDED(hr)) {
								IDispatch *pdisp = NULL;
								hr = psfvd->get_Application(&pdisp);
								if (SUCCEEDED(hr)) {
									IShellDispatch2 *pShellDispatch;
									hr = pdisp->QueryInterface(IID_PPV_ARGS(&pShellDispatch));
									if (SUCCEEDED(hr)) {
										_variant_t dir(pcDir);
										_variant_t verb(operation);
										_bstr_t file(pcFileName);
										_variant_t para(pcParameters);
										_variant_t show(SW_SHOWNORMAL);
										hr = pShellDispatch->ShellExecute(file, para, dir, verb, show);
										if (SUCCEEDED(hr)) {
											ok = true;
										}
										RELEASE_OBJ(pShellDispatch);
									}
									RELEASE_OBJ(pdisp);
								}
								RELEASE_OBJ(psfvd);
							}
							RELEASE_OBJ(pdispBackground);
						}
						RELEASE_OBJ(psv);
					}
					RELEASE_OBJ(psb);
				}
			}
			RELEASE_OBJ(pdisp);
		}
		RELEASE_OBJ(psw);
	}
	return ok;
}
