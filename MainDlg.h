#pragma once

#include "VirtualDesktops.h"

class CMainDlg : public CDialogImpl<CMainDlg>
{
public:
	enum { IDD = IDD_MAINDLG };

	enum
	{
		UWM_NOTIFYICON = WM_APP,
		UWM_BRING_TO_FRONT,
	};

	BEGIN_MSG_MAP_EX(CMainDlg)
		MSG_WM_INITDIALOG(OnInitDialog)
		MSG_WM_WINDOWPOSCHANGING(OnWindowPosChanging)
		MSG_WM_HOTKEY(OnHotKey)
		MSG_WM_NOTIFY(OnNotify)
		MSG_WM_DESTROY(OnDestroy)
		COMMAND_ID_HANDLER_EX(IDCANCEL, OnCancel)
		COMMAND_HANDLER_EX(IDC_DESKS_COMBO, CBN_SELCHANGE, OnDesksComboChanged)
		MESSAGE_HANDLER_EX(m_uTaskbarCreatedMsg, OnTaskbarCreated)
		MESSAGE_HANDLER_EX(UWM_NOTIFYICON, OnNotifyIcon)
		MESSAGE_HANDLER_EX(UWM_BRING_TO_FRONT, OnBringToFront)
	END_MSG_MAP()

	BOOL OnInitDialog(CWindow wndFocus, LPARAM lInitParam);
	void OnWindowPosChanging(LPWINDOWPOS lpWndPos);
	void OnHotKey(int nHotKeyID, UINT uModifiers, UINT uVirtKey);
	LRESULT OnNotify(int idCtrl, LPNMHDR pnmh);
	void OnDestroy();
	void OnCancel(UINT uNotifyCode, int nID, CWindow wndCtl);
	void OnDesksComboChanged(UINT uNotifyCode, int nID, CWindow wndCtl);
	LRESULT OnTaskbarCreated(UINT uMsg, WPARAM wParam, LPARAM lParam);
	LRESULT OnNotifyIcon(UINT uMsg, WPARAM wParam, LPARAM lParam);
	LRESULT OnBringToFront(UINT uMsg, WPARAM wParam, LPARAM lParam);

private:
	void InitNotifyIconData();
	void NotifyIconRightClickMenu();

	std::unique_ptr<VirtualDesktops> m_virtualDesktops;
	std::vector<int> m_registeredHotkeys;
	UINT m_uTaskbarCreatedMsg = RegisterWindowMessage(L"TaskbarCreated");
	NOTIFYICONDATA m_notifyIconData = {};
	bool m_handledDialogHiding = false;
};
