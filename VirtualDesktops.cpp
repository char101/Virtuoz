#include "stdafx.h"
#include "VirtualDesktops.h"

// http://stackoverflow.com/a/6088475
class WindowsAnimationSuppressor
{
public:
	WindowsAnimationSuppressor(bool start_suppressed = true)
	{
		if(start_suppressed)
			Suppress();
	}

	~WindowsAnimationSuppressor()
	{
		Unsuppress();
	}

	bool Suppress()
	{
		if(m_suppressed)
			return true;

		m_settings.cbSize = sizeof(ANIMATIONINFO);
		if(!::SystemParametersInfo(SPI_GETANIMATION, sizeof(ANIMATIONINFO), &m_settings, 0))
			return false;

		if(m_settings.iMinAnimate == 0)
			return true;

		int iOldMinAnimate = m_settings.iMinAnimate;
		m_settings.iMinAnimate = 0;
		if(!::SystemParametersInfo(SPI_SETANIMATION, sizeof(ANIMATIONINFO), &m_settings, 0))
			return false;

		m_settings.iMinAnimate = iOldMinAnimate;
		m_suppressed = true;
		return true;
	}

	bool Unsuppress()
	{
		if(!m_suppressed)
			return true;

		if(!::SystemParametersInfo(SPI_SETANIMATION, sizeof(ANIMATIONINFO), &m_settings, 0))
			return false;

		m_suppressed = false;
		return true;
	}

private:
	bool m_suppressed = false;
	ANIMATIONINFO m_settings;
};

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
	WindowsAnimationSuppressor suppressor(false);

	for(DesktopInfo &desktop : m_desktops)
	{
		auto &windows = desktop.zOrderedWindows;
		for(auto i = windows.rbegin(); i != windows.rend(); ++i)
		{
			// Suppress animation lazily.
			suppressor.Suppress();

			CWindow window(*i);
			ShowWindowOnSwitch(window, true);
		}
	}
}

void VirtualDesktops::SwitchDesktop(int desktopId)
{
	if(desktopId == m_currentDesktopId)
		return;

	WindowsAnimationSuppressor suppressor;

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
				ShowWindowOnSwitch(hWnd, false);
			}
		}

		return TRUE;
	}, reinterpret_cast<LPARAM>(this));

	m_currentDesktopId = desktopId;

	auto &windows = m_desktops[m_currentDesktopId].zOrderedWindows;
	for(auto i = windows.rbegin(); i != windows.rend(); ++i)
	{
		CWindow window(*i);
		ShowWindowOnSwitch(window, true);
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

bool VirtualDesktops::ShowWindowOnSwitch(HWND hWnd, bool show)
{
	return FALSE != ShowWindow(hWnd, show ? SW_SHOWNA : SW_HIDE);
}
