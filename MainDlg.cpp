#include "stdafx.h"
#include "resource.h"
#include "MainDlg.h"

BOOL CMainDlg::OnInitDialog(CWindow wndFocus, LPARAM lInitParam)
{
	// center the dialog on the screen
	CenterWindow();

	// set icons
	HICON hIcon = AtlLoadIconImage(IDR_MAINFRAME, LR_DEFAULTCOLOR, ::GetSystemMetrics(SM_CXICON), ::GetSystemMetrics(SM_CYICON));
	SetIcon(hIcon, TRUE);
	HICON hIconSmall = AtlLoadIconImage(IDR_MAINFRAME, LR_DEFAULTCOLOR, ::GetSystemMetrics(SM_CXSMICON), ::GetSystemMetrics(SM_CYSMICON));
	SetIcon(hIconSmall, FALSE);

	// init virtual desktops and hotkeys
	VirtualDesktopsConfig config;

	m_virtualDesktops.reset(new VirtualDesktops(config));

	for(int i = 0; i < config.numberOfDesktops; i++)
	{
		if(::RegisterHotKey(m_hWnd, i, config.hotkeys[i].fsModifiers | MOD_NOREPEAT, config.hotkeys[i].vk))
		{
			m_registeredHotkeys.push_back(i);
		}
		else
		{
			DEBUG_LOG(logERROR) << "Could not register hotkey for desktop " << (i + 1);
		}
	}

	return TRUE;
}

void CMainDlg::OnHotKey(int nHotKeyID, UINT uModifiers, UINT uVirtKey)
{
	m_virtualDesktops->SwitchDesktop(nHotKeyID);
}

void CMainDlg::OnDestroy()
{
	for(auto a : m_registeredHotkeys)
	{
		::UnregisterHotKey(m_hWnd, a);
	}

	m_virtualDesktops.reset();
}

void CMainDlg::OnAppAbout(UINT uNotifyCode, int nID, CWindow wndCtl)
{
	CSimpleDialog<IDD_ABOUTBOX, FALSE> dlg;
	dlg.DoModal();
}

void CMainDlg::OnOK(UINT uNotifyCode, int nID, CWindow wndCtl)
{
	EndDialog(nID);
}

void CMainDlg::OnCancel(UINT uNotifyCode, int nID, CWindow wndCtl)
{
	EndDialog(nID);
}

LRESULT CMainDlg::OnTaskbarCreated(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	m_virtualDesktops->OnTaskbarCreated();
	return 0;
}
