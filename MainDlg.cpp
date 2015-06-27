#include "stdafx.h"
#include "resource.h"
#include "MainDlg.h"

BOOL CMainDlg::OnInitDialog(CWindow wndFocus, LPARAM lInitParam)
{
	// Center the dialog on the screen.
	CenterWindow();

	// Set icons.
	//HICON hIcon = AtlLoadIconImage(IDR_MAINFRAME, LR_DEFAULTCOLOR, ::GetSystemMetrics(SM_CXICON), ::GetSystemMetrics(SM_CYICON));
	//SetIcon(hIcon, TRUE);
	//HICON hIconSmall = AtlLoadIconImage(IDR_MAINFRAME, LR_DEFAULTCOLOR, ::GetSystemMetrics(SM_CXSMICON), ::GetSystemMetrics(SM_CYSMICON));
	//SetIcon(hIconSmall, FALSE);

	// Init virtual desktops and hotkeys.
	VirtualDesktopsConfig config;
	m_virtualDesktops.reset(new VirtualDesktops(config));

	CComboBox desksCombo(GetDlgItem(IDC_DESKS_COMBO));

	for(int i = 0; i < config.numberOfDesktops; i++)
	{
		CString comboText;
		comboText.Format(L"Desktop %d: ", i + 1);
		comboText += VirtualDesktopsConfig::HotkeyToString(config.hotkeys[i]);

		if(::RegisterHotKey(m_hWnd, i, config.hotkeys[i].fsModifiers | MOD_NOREPEAT, config.hotkeys[i].vk))
		{
			m_registeredHotkeys.push_back(i);
		}
		else
		{
			comboText += L" (hotkey not registered)";
			DEBUG_LOG(logERROR) << "Could not register hotkey for desktop " << (i + 1);
		}

		desksCombo.AddString(comboText);
	}

	desksCombo.SetCurSel(0);

	// Init and show tray icon.
	InitNotifyIconData();
	Shell_NotifyIcon(NIM_ADD, &m_notifyIconData);

	return TRUE;
}

void CMainDlg::OnWindowPosChanging(LPWINDOWPOS lpWndPos)
{
	if(!m_handledDialogHiding && (lpWndPos->flags & SWP_SHOWWINDOW))
	{
		lpWndPos->flags &= ~SWP_SHOWWINDOW;
		m_handledDialogHiding = true;
	}
}

void CMainDlg::OnHotKey(int nHotKeyID, UINT uModifiers, UINT uVirtKey)
{
	m_virtualDesktops->SwitchDesktop(nHotKeyID);

	CComboBox desksCombo(GetDlgItem(IDC_DESKS_COMBO));
	desksCombo.SetCurSel(nHotKeyID);
}

LRESULT CMainDlg::OnNotify(int idCtrl, LPNMHDR pnmh)
{
	switch(pnmh->idFrom)
	{
	case IDC_MAIN_SYSLINK:
		switch(pnmh->code)
		{
		case NM_CLICK:
		case NM_RETURN:
			if((int)ShellExecute(m_hWnd, L"open", ((PNMLINK)pnmh)->item.szUrl, NULL, NULL, SW_SHOWNORMAL) <= 32)
			{
				CString str = L"An error occurred while trying to open the following website address:\n";
				str += ((PNMLINK)pnmh)->item.szUrl;
				MessageBox(str, NULL, MB_ICONHAND);
			}
			break;
		}
		break;
	}

	SetMsgHandled(FALSE);
	return 0;
}

void CMainDlg::OnDestroy()
{
	for(auto a : m_registeredHotkeys)
	{
		::UnregisterHotKey(m_hWnd, a);
	}

	m_virtualDesktops.reset();

	Shell_NotifyIcon(NIM_DELETE, &m_notifyIconData);
}

void CMainDlg::OnCancel(UINT uNotifyCode, int nID, CWindow wndCtl)
{
	ShowWindow(SW_HIDE);
}

void CMainDlg::OnDesksComboChanged(UINT uNotifyCode, int nID, CWindow wndCtl)
{
	CComboBox desksCombo(wndCtl);
	m_virtualDesktops->SwitchDesktop(desksCombo.GetCurSel());
}

LRESULT CMainDlg::OnTaskbarCreated(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	Shell_NotifyIcon(NIM_ADD, &m_notifyIconData);
	m_virtualDesktops->OnTaskbarCreated();
	return 0;
}

LRESULT CMainDlg::OnNotifyIcon(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if(wParam == 1)
	{
		switch(lParam)
		{
		case WM_LBUTTONUP:
			ShowWindow(SW_SHOW);
			::SetForegroundWindow(m_hWnd);
			break;

		case WM_RBUTTONUP:
			::SetForegroundWindow(m_hWnd);
			NotifyIconRightClickMenu();
			break;
		}
	}

	return 0;
}

LRESULT CMainDlg::OnBringToFront(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	ShowWindow(SW_SHOW);
	::SetForegroundWindow(m_hWnd);
	return 0;
}

void CMainDlg::InitNotifyIconData()
{
	m_notifyIconData.cbSize = NOTIFYICONDATA_V1_SIZE;
	m_notifyIconData.hWnd = m_hWnd;
	m_notifyIconData.uID = 1;
	m_notifyIconData.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	m_notifyIconData.uCallbackMessage = UWM_NOTIFYICON;
	m_notifyIconData.hIcon = AtlLoadIconImage(IDR_MAINFRAME, LR_DEFAULTCOLOR, ::GetSystemMetrics(SM_CXSMICON), ::GetSystemMetrics(SM_CYSMICON));

	CString sWindowText;
	GetWindowText(sWindowText);
	wcscpy_s(m_notifyIconData.szTip, sWindowText);
}

void CMainDlg::NotifyIconRightClickMenu()
{
	int numberOfDesktops = m_virtualDesktops->GetNumberOfDesktops();
	int currentDesktop = m_virtualDesktops->GetCurrentDesktop();

	const int nExitId = 10001;

	CMenu menu;
	menu.CreatePopupMenu();

	CComboBox desksCombo(GetDlgItem(IDC_DESKS_COMBO));

	for(int i = 0; i < numberOfDesktops; i++)
	{
		CString str;
		desksCombo.GetLBText(i, str);

		menu.AppendMenu(MF_STRING, i, str);
	}

	menu.AppendMenu(MF_SEPARATOR);
	menu.AppendMenu(MF_STRING, nExitId, L"Exit");

	menu.CheckMenuRadioItem(0, numberOfDesktops - 1, currentDesktop, MF_BYCOMMAND);

	CPoint point;
	GetCursorPos(&point);
	int nCmd = menu.TrackPopupMenu(TPM_RIGHTBUTTON | TPM_RETURNCMD, point.x, point.y, m_hWnd);

	if(nCmd >= 0 && nCmd < numberOfDesktops)
	{
		m_virtualDesktops->SwitchDesktop(nCmd);
		desksCombo.SetCurSel(nCmd);
	}
	else if(nCmd == nExitId)
		EndDialog(0);
}
