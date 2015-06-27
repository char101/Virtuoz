#pragma once

#include "VirtualDesktops.h"

class CMainDlg : public CDialogImpl<CMainDlg>
{
public:
	enum { IDD = IDD_MAINDLG };

	BEGIN_MSG_MAP_EX(CMainDlg)
		MSG_WM_INITDIALOG(OnInitDialog)
		MSG_WM_HOTKEY(OnHotKey)
		MSG_WM_DESTROY(OnDestroy)
		COMMAND_ID_HANDLER_EX(ID_APP_ABOUT, OnAppAbout)
		COMMAND_ID_HANDLER_EX(IDOK, OnOK)
		COMMAND_ID_HANDLER_EX(IDCANCEL, OnCancel)
		MESSAGE_HANDLER_EX(m_uTaskbarCreatedMsg, OnTaskbarCreated)
	END_MSG_MAP()

	BOOL OnInitDialog(CWindow wndFocus, LPARAM lInitParam);
	void OnHotKey(int nHotKeyID, UINT uModifiers, UINT uVirtKey);
	void OnDestroy();
	void OnAppAbout(UINT uNotifyCode, int nID, CWindow wndCtl);
	void OnOK(UINT uNotifyCode, int nID, CWindow wndCtl);
	void OnCancel(UINT uNotifyCode, int nID, CWindow wndCtl);
	LRESULT OnTaskbarCreated(UINT uMsg, WPARAM wParam, LPARAM lParam);

private:
	std::unique_ptr<VirtualDesktops> m_virtualDesktops;
	std::vector<int> m_registeredHotkeys;
	UINT m_uTaskbarCreatedMsg = RegisterWindowMessage(L"TaskbarCreated");
};
