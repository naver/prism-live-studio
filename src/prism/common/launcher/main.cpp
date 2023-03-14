#include <Windows.h>
#include <userenv.h>
#include <string>
#include <assert.h>

// Warning : when name of PRISM is changed, please remember to modify here
#define PRISM_APP_NAME_W L"PRISMLiveStudio.exe"

#pragma comment(linker, "/subsystem:\"windows\" /entry:\"mainCRTStartup\"")

BOOL RunAppWithoutAdmin();
void RunAppAsCurrent();

int main()
{
	if (!RunAppWithoutAdmin()) {
		RunAppAsCurrent();
		assert(false && "failed to run app without admin");
	}

	return 0;
}

// include '\' at the end
const std::wstring &GetFileDirectory(HINSTANCE hInstance = NULL)
{
	static std::wstring s_wstrDirectory = std::wstring();

	if (!s_wstrDirectory.empty())
		return s_wstrDirectory;

	WCHAR szFilePath[MAX_PATH] = {};
	GetModuleFileNameW(hInstance, szFilePath, MAX_PATH);

	WCHAR wchFlag = '\\';
	int nLen = (int)wcslen(szFilePath);
	for (int i = nLen - 1; i >= 0; --i) {
		if (szFilePath[i] == wchFlag) {
			szFilePath[i + 1] = 0;
			break;
		}
	}

	s_wstrDirectory = szFilePath;
	return s_wstrDirectory;
}

void RunAppAsCurrent()
{
	SHELLEXECUTEINFOW sei = {sizeof(SHELLEXECUTEINFOW)};
	sei.fMask = SEE_MASK_DEFAULT;
	sei.lpVerb = L"runas";
	sei.lpFile = PRISM_APP_NAME_W;
	sei.lpDirectory = GetFileDirectory().c_str();
	ShellExecuteExW(&sei);
}

BOOL RunAppWithoutAdmin()
{
	std::wstring appPath = GetFileDirectory() + std::wstring(PRISM_APP_NAME_W);

	DWORD dwError = ERROR_SUCCESS;
	HANDLE hToken = NULL;
	HANDLE hNewToken = NULL;
	PSID pIntegritySid = NULL;
	LPVOID pEnvironment = NULL;
	PROCESS_INFORMATION pi = {0};

	do {
		// Open the primary access token of the process.
		if (!OpenProcessToken(GetCurrentProcess(), TOKEN_DUPLICATE | TOKEN_QUERY | TOKEN_ADJUST_DEFAULT | TOKEN_ASSIGN_PRIMARY, &hToken)) {
			assert(false && "OpenProcessToken failed");
			dwError = GetLastError();
			break;
		}

		// Duplicate the primary token of the current process.
		if (!DuplicateTokenEx(hToken, 0, NULL, SecurityImpersonation, TokenPrimary, &hNewToken)) {
			assert(false && "DuplicateTokenEx failed");
			dwError = GetLastError();
			break;
		}

		// Create the low integrity SID.
		SID_IDENTIFIER_AUTHORITY MLAuthority = SECURITY_MANDATORY_LABEL_AUTHORITY;
		if (!AllocateAndInitializeSid(&MLAuthority, 1, SECURITY_MANDATORY_MEDIUM_RID, 0, 0, 0, 0, 0, 0, 0, &pIntegritySid)) {
			assert(false && "AllocateAndInitializeSid failed");
			dwError = GetLastError();
			break;
		}

		TOKEN_MANDATORY_LABEL tml = {0};
		tml.Label.Attributes = SE_GROUP_INTEGRITY;
		tml.Label.Sid = pIntegritySid;

		// Set the integrity level in the access token to low.
		if (!SetTokenInformation(hNewToken, TokenIntegrityLevel, &tml, (sizeof(tml) + GetLengthSid(pIntegritySid)))) {
			assert(false && "SetTokenInformation failed");
			dwError = GetLastError();
			break;
		}

		if (!CreateEnvironmentBlock(&pEnvironment, hNewToken, FALSE)) {
			assert(false && "CreateEnvironmentBlock failed");
			dwError = GetLastError();
			break;
		}

		// Create the new process at the Low integrity level.
		STARTUPINFO si = {sizeof(si)};
		if (!CreateProcessAsUser(hNewToken, (LPWSTR)appPath.c_str(), (LPWSTR)appPath.c_str(), NULL, NULL, FALSE, CREATE_UNICODE_ENVIRONMENT, pEnvironment, NULL, &si, &pi)) {
			assert(false && "CreateProcessAsUser failed");
			dwError = GetLastError();
			break;
		}
	} while (0);

	if (hToken)
		CloseHandle(hToken);

	if (hNewToken)
		CloseHandle(hNewToken);

	if (pIntegritySid)
		FreeSid(pIntegritySid);

	if (pEnvironment)
		DestroyEnvironmentBlock(pEnvironment);

	if (pi.hProcess)
		CloseHandle(pi.hProcess);

	if (pi.hThread)
		CloseHandle(pi.hThread);

	return (ERROR_SUCCESS == dwError);
}
