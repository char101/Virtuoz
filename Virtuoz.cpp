// Virtuoz.cpp : main source file for Virtuoz.exe
//

#include "stdafx.h"
#include "resource.h"
#include "MainDlg.h"

CAppModule _Module;

namespace
{
	int RunApp(HINSTANCE hInstance);
	ATOM RegisterDialogClass(LPCTSTR lpszClassName, HINSTANCE hInstance);
}

int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPTSTR /*lpstrCmdLine*/, int /*nCmdShow*/)
{
	HRESULT hRes = ::CoInitialize(NULL);
// If you are running on NT 4.0 or higher you can use the following call instead to 
// make the EXE free threaded. This means that calls come in on a random RPC thread.
//	HRESULT hRes = ::CoInitializeEx(NULL, COINIT_MULTITHREADED);
	ATLASSERT(SUCCEEDED(hRes));

	// this resolves ATL window thunking problem when Microsoft Layer for Unicode (MSLU) is used
	::DefWindowProc(NULL, 0, 0, 0L);

	//AtlInitCommonControls(ICC_BAR_CLASSES);	// add flags to support other controls

	hRes = _Module.Init(NULL, hInstance);
	ATLASSERT(SUCCEEDED(hRes));

	int nRet = RunApp(hInstance);

	_Module.Term();
	::CoUninitialize();

	return nRet;
}

namespace
{
	int RunApp(HINSTANCE hInstance)
	{
		CHandle hMutex(::CreateMutex(NULL, TRUE, L"virtuoz_app"));
		if(hMutex && GetLastError() == ERROR_ALREADY_EXISTS)
		{
			CWindow wndRunning(::FindWindow(L"Virtuoz", NULL));
			if(wndRunning)
			{
				::AllowSetForegroundWindow(wndRunning.GetWindowProcessID());
				wndRunning.PostMessage(CMainDlg::UWM_BRING_TO_FRONT);
			}

			return 0;
		}

		RegisterDialogClass(L"Virtuoz", hInstance);

		int nRet = 0;
		// BLOCK: Run application
		{
			CMainDlg dlgMain;
			nRet = (int)dlgMain.DoModal();
		}

		UnregisterClass(L"Virtuoz", hInstance);

		if(hMutex)
			::ReleaseMutex(hMutex);

		return nRet;
	}

	ATOM RegisterDialogClass(LPCTSTR lpszClassName, HINSTANCE hInstance)
	{
		WNDCLASS wndcls;
		::GetClassInfo(hInstance, MAKEINTRESOURCE(32770), &wndcls);

		// Set our own class name
		wndcls.lpszClassName = lpszClassName;

		// Just register the class
		return ::RegisterClass(&wndcls);
	}
}
