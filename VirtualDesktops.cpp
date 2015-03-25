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

	bool IsSuppressed()
	{
		return m_suppressed;
	}

private:
	bool m_suppressed = false;
	ANIMATIONINFO m_settings;
};

namespace
{
	bool IsWindowVisibleOnScreen(HWND hWnd);
	bool ShowWindowOnSwitch(HWND hWnd, bool show, bool waitForCompletion, bool activate = false);
}

struct MonitorInfo
{
	std::vector<HWND> taskbarOrderedWindows;
	//CString wallpaper;
};

struct DesktopInfo
{
	HWND hForegroundWindow;
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
			ShowWindowOnSwitch(window, true, suppressor.IsSuppressed());
		}
	}
}

void VirtualDesktops::SwitchDesktop(int desktopId)
{
	if(desktopId == m_currentDesktopId)
		return;

	WindowsAnimationSuppressor suppressor(false);

	struct CALLBACK_PARAM {
		VirtualDesktops *pThis;
		WindowsAnimationSuppressor *pSuppressor;
	} param = { this, &suppressor };

	m_desktops[m_currentDesktopId].hForegroundWindow = GetForegroundWindow();

	EnumWindows([](HWND hWnd, LPARAM lParam) {
		CWindow window(hWnd);
		CALLBACK_PARAM *pParam = reinterpret_cast<CALLBACK_PARAM *>(lParam);
		VirtualDesktops *pThis = pParam->pThis;
		WindowsAnimationSuppressor *pSuppressor = pParam->pSuppressor;

		DesktopInfo &currentDesktop = pThis->m_desktops[pThis->m_currentDesktopId];

		if(window.IsWindowVisible())
		{
			DWORD dwExStyle = window.GetExStyle();
			if((dwExStyle & WS_EX_APPWINDOW) || !(dwExStyle & WS_EX_TOOLWINDOW)/* || IsWindowVisibleOnScreen(window)*/)
			{
				// Suppress animation lazily.
				pSuppressor->Suppress();

				currentDesktop.zOrderedWindows.push_back(hWnd);
				ShowWindowOnSwitch(hWnd, false, pSuppressor->IsSuppressed());
			}
		}

		return TRUE;
	}, reinterpret_cast<LPARAM>(&param));

	m_currentDesktopId = desktopId;

	auto &windows = m_desktops[m_currentDesktopId].zOrderedWindows;
	for(auto i = windows.rbegin(); i != windows.rend(); ++i)
	{
		// Suppress animation lazily.
		suppressor.Suppress();

		CWindow window(*i);
		ShowWindowOnSwitch(window, true, suppressor.IsSuppressed(), 
			window == m_desktops[m_currentDesktopId].hForegroundWindow);
	}

	windows.clear();
}

namespace
{
	bool IsWindowVisibleOnScreen(HWND hWnd)
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

	bool ShowWindowOnSwitch(HWND hWnd, bool show, bool waitForCompletion, bool activate /*= false*/)
	{
		assert(show || !activate); // can't hide and activate

		DWORD dwSWPflags = SWP_NOMOVE | SWP_NOSIZE | SWP_ASYNCWINDOWPOS |
			(activate ? 0 : SWP_NOACTIVATE | SWP_NOZORDER) |
			(show ? SWP_SHOWWINDOW : SWP_HIDEWINDOW);
		bool result = FALSE != SetWindowPos(hWnd, activate ? HWND_TOP : NULL, 0, 0, 0, 0, dwSWPflags);

		if(waitForCompletion)
		{
			// Wait for the window to actually show/hide, with a timeout of 200 ms
			SendMessageTimeout(hWnd, WM_NULL, 0, 0, SMTO_ABORTIFHUNG, 200, NULL);
		}

		return result;
	}
}
