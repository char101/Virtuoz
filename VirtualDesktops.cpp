#include "stdafx.h"
#include "VirtualDesktops.h"

struct MonitorInfo
{
	std::vector<HWND> taskbarOrderedWindows;
	//CString wallpaper;
};

struct DesktopInfo
{
	std::vector<HWND> zOrderedWindows;
	std::vector<MonitorInfo> monitors;
};

VirtualDesktops::VirtualDesktops(VirtualDesktopsConfig config /*= VirtualDesktopsConfig()*/)
	: m_config(config)
{
	m_desktops.resize(config.numberOfDesktops);
}

VirtualDesktops::~VirtualDesktops()
{
	for(DesktopInfo &desktop : m_desktops)
	{
		auto &windows = desktop.zOrderedWindows;
		for(auto i = windows.rbegin(); i != windows.rend(); ++i)
		{
			CWindow window(*i);
			window.ShowWindow(SW_SHOWNA);
		}
	}
}

void VirtualDesktops::SwitchDesktop(int desktopId)
{
	if(desktopId == m_currentDesktopId)
		return;

	EnumWindows([](HWND hWnd, LPARAM lParam) {
		CWindow window(hWnd);
		VirtualDesktops *pThis = reinterpret_cast<VirtualDesktops *>(lParam);

		DesktopInfo &currentDesktop = pThis->m_desktops[pThis->m_currentDesktopId];

		if(window.IsWindowVisible())
		{
			DWORD dwExStyle = window.GetExStyle();
			if((dwExStyle & WS_EX_APPWINDOW) || !(dwExStyle & WS_EX_TOOLWINDOW)/* || pThis->IsWindowVisibleOnScreen(window)*/)
			{
				currentDesktop.zOrderedWindows.push_back(hWnd);
				window.ShowWindow(SW_HIDE);
			}
		}

		return TRUE;
	}, reinterpret_cast<LPARAM>(this));

	m_currentDesktopId = desktopId;

	auto &windows = m_desktops[m_currentDesktopId].zOrderedWindows;
	for(auto i = windows.rbegin(); i != windows.rend(); ++i)
	{
		CWindow window(*i);
		window.ShowWindow(SW_SHOWNA);
	}

	windows.clear();
}

bool VirtualDesktops::IsWindowVisibleOnScreen(HWND hWnd)
{
	CWindow window(hWnd);

	struct CALLBACK_PARAM {
		CRect rect;
		bool isVisible;
	} param;

	if(!window.GetWindowRect(&param.rect) || param.rect.IsRectEmpty())
		return false;

	param.isVisible = false;

	EnumDisplayMonitors(NULL, NULL, [](HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) {
		CALLBACK_PARAM *pParam = reinterpret_cast<CALLBACK_PARAM *>(dwData);

		MONITORINFO info;
		info.cbSize = sizeof(MONITORINFO);
		if(GetMonitorInfo(hMonitor, &info))
		{
			CRect temp;
			if(temp.IntersectRect(&pParam->rect, &info.rcWork))
			{
				pParam->isVisible = true;
				return FALSE;
			}
		}

		return TRUE;
	}, reinterpret_cast<LPARAM>(&param));

	return param.isVisible;
}
