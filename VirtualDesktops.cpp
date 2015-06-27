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

class TaskbarManipulator
{
public:
	TaskbarManipulator()
	{
		if(TTLib_ManipulationStart())
			m_manipulating = true;
		else
			DEBUG_LOG(logERROR) << "TTLib_ManipulationStart failed";
	}

	~TaskbarManipulator()
	{
		if(m_manipulating)
		{
			TTLib_ManipulationEnd();
			m_manipulating = false;
		}
	}

	bool IsManipulating()
	{
		return m_manipulating;
	}

private:
	bool m_manipulating = false;
};

namespace
{
	bool IsWindowVisibleOnScreen(HWND hWnd);
	bool ShowWindowOnSwitch(HWND hWnd, bool show, bool activate = false);
}

struct TaskbarItem
{
	bool pinned = false;
	CString appId;
	std::vector<HWND> windows;
};

struct TaskbarInfo
{
	std::vector<TaskbarItem> taskbarItems;
};

struct MonitorInfo
{
	TaskbarInfo taskbarInfo;
	//CString wallpaper;
};

struct WindowsInfo
{
	HWND hForegroundWindow = NULL;
	std::vector<HWND> zOrderedWindows;
};

struct DesktopInfo
{
	WindowsInfo windowsInfo;
	std::map<CString, MonitorInfo> monitorsInfo;
};

VirtualDesktops::VirtualDesktops(VirtualDesktopsConfig config /*= VirtualDesktopsConfig()*/)
	: m_config(config)
{
	m_desktops.resize(config.numberOfDesktops);

	DWORD dwError = TTLib_Init();
	if(dwError == TTLIB_OK)
	{
		m_TTLIbLoaded = true;

		dwError = TTLib_LoadIntoExplorer();
		if(dwError != TTLIB_OK)
		{
			DEBUG_LOG(logERROR) << "TTLib_LoadIntoExplorer failed with error " << dwError;
		}
	}
	else
	{
		DEBUG_LOG(logERROR) << "TTLib_Init failed with error " << dwError;
	}
}

VirtualDesktops::~VirtualDesktops()
{
	if(m_TTLIbLoaded)
	{
		TTLib_Uninit();
		m_TTLIbLoaded = false;
	}

	WindowsAnimationSuppressor suppressor(false);

	for(DesktopInfo &desktop : m_desktops)
	{
		auto &windows = desktop.windowsInfo.zOrderedWindows;
		for(auto i = windows.rbegin(); i != windows.rend(); ++i)
		{
			// Suppress animation lazily.
			suppressor.Suppress();

			CWindow window(*i);
			ShowWindowOnSwitch(window, true);
		}
	}

	if(suppressor.IsSuppressed())
	{
		if(!SendMessageTimeout(HWND_BROADCAST, WM_NULL, 0, 0, SMTO_ABORTIFHUNG, 2000, NULL))
		{
			DEBUG_LOG(logWARNING) << "SendMessageTimeout failed with " << GetLastError();
		}
	}
}

void VirtualDesktops::SwitchDesktop(int desktopId)
{
	if(desktopId == m_currentDesktopId)
		return;

	EnableTaskbarsRedrawind(false);
	SaveMonitorsInfo();

	SwitchDesktopWindows(desktopId);

	RestoreMonitorsInfo();
	EnableTaskbarsRedrawind(true);
}

void VirtualDesktops::OnTaskbarCreated()
{
	if(m_TTLIbLoaded)
	{
		if(!TTLib_IsLoadedIntoExplorer())
		{
			DWORD dwError = TTLib_LoadIntoExplorer();
			if(dwError != TTLIB_OK)
			{
				DEBUG_LOG(logERROR) << "TTLib_LoadIntoExplorer failed with error " << dwError;
			}
		}
		else
			assert(false);
	}
}

void VirtualDesktops::SwitchDesktopWindows(int desktopId)
{
	assert(desktopId != m_currentDesktopId);

	WindowsAnimationSuppressor suppressor(false);

	struct CALLBACK_PARAM {
		VirtualDesktops *pThis;
		WindowsAnimationSuppressor *pSuppressor;
	} param = { this, &suppressor };

	m_desktops[m_currentDesktopId].windowsInfo.hForegroundWindow = GetForegroundWindow();

	EnumWindows([](HWND hWnd, LPARAM lParam) {
		CWindow window(hWnd);
		CALLBACK_PARAM *pParam = reinterpret_cast<CALLBACK_PARAM *>(lParam);
		VirtualDesktops *pThis = pParam->pThis;
		WindowsAnimationSuppressor *pSuppressor = pParam->pSuppressor;

		DesktopInfo &currentDesktop = pThis->m_desktops[pThis->m_currentDesktopId];

		if(window.GetWindowProcessID() != GetCurrentProcessId() &&
			window.IsWindowVisible())
		{
			DWORD dwExStyle = window.GetExStyle();
			if((dwExStyle & WS_EX_APPWINDOW) || !(dwExStyle & WS_EX_TOOLWINDOW)/* || IsWindowVisibleOnScreen(window)*/)
			{
				// Suppress animation lazily.
				pSuppressor->Suppress();

				currentDesktop.windowsInfo.zOrderedWindows.push_back(hWnd);
				ShowWindowOnSwitch(hWnd, false);
			}
		}

		return TRUE;
	}, reinterpret_cast<LPARAM>(&param));

	m_currentDesktopId = desktopId;

	auto &windows = m_desktops[m_currentDesktopId].windowsInfo.zOrderedWindows;
	HWND hForegroundWindow = m_desktops[m_currentDesktopId].windowsInfo.hForegroundWindow;
	for(auto i = windows.rbegin(); i != windows.rend(); ++i)
	{
		// Suppress animation lazily.
		suppressor.Suppress();

		CWindow window(*i);
		ShowWindowOnSwitch(window, true, window == hForegroundWindow);
	}

	m_desktops[m_currentDesktopId].windowsInfo = WindowsInfo();

	// We wait even if animation is not suppressed,
	// because we want the taskbars to be ready for reordering.
	//if(suppressor.IsSuppressed())
	{
		if(!SendMessageTimeout(HWND_BROADCAST, WM_NULL, 0, 0, SMTO_ABORTIFHUNG, 2000, NULL))
		{
			DEBUG_LOG(logWARNING) << "SendMessageTimeout failed with " << GetLastError();
		}
	}
}

void VirtualDesktops::SaveMonitorsInfo()
{
	SaveTaskbarsInfo();
/*
	// TODO: Wallpaper?
	struct CALLBACK_PARAM {
		VirtualDesktops *pThis;
	} param = { this };

	EnumDisplayMonitors(NULL, NULL, [](HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) {
		CALLBACK_PARAM *pParam = reinterpret_cast<CALLBACK_PARAM *>(dwData);
		VirtualDesktops *pThis = pParam->pThis;

		MONITORINFOEX systemMonitorInfo;
		systemMonitorInfo.cbSize = sizeof(MONITORINFOEX);
		if(GetMonitorInfo(hMonitor, &systemMonitorInfo))
		{
			MonitorInfo monitorInfo;
			pThis->SaveMonitorInfo(hMonitor, monitorInfo);
			pThis->m_desktops[pThis->m_currentDesktopId].monitors[systemMonitorInfo.szDevice] = std::move(monitorInfo);
		}

		return TRUE;
	}, reinterpret_cast<LPARAM>(&param));
*/
}

void VirtualDesktops::SaveTaskbarsInfo()
{
	if(!m_TTLIbLoaded)
		return;

	TaskbarManipulator taskbarManipulator;
	if(!taskbarManipulator.IsManipulating())
		return;

	HANDLE hTaskbar = TTLib_GetMainTaskbar();
	SaveTaskbarInfo(hTaskbar);

	int nCount;
	if(TTLib_GetSecondaryTaskbarCount(&nCount))
	{
		for(int i = 0; i < nCount; i++)
		{
			hTaskbar = TTLib_GetSecondaryTaskbar(i);
			SaveTaskbarInfo(hTaskbar);
		}
	}
}

void VirtualDesktops::SaveTaskbarInfo(HANDLE hTaskbar)
{
	HMONITOR hMonitor = TTLib_GetTaskbarMonitor(hTaskbar);

	MONITORINFOEX systemMonitorInfo;
	systemMonitorInfo.cbSize = sizeof(MONITORINFOEX);
	if(!GetMonitorInfo(hMonitor, &systemMonitorInfo))
	{
		DEBUG_LOG(logERROR) << "GetMonitorInfo failed for monitor " << hMonitor;
		return;
	}

	TaskbarInfo &taskbarInfo = m_desktops[m_currentDesktopId].monitorsInfo[systemMonitorInfo.szDevice].taskbarInfo;

	int nButtonGroupCount;
	if(TTLib_GetButtonGroupCount(hTaskbar, &nButtonGroupCount))
	{
		for(int i = 0; i < nButtonGroupCount; i++)
		{
			HANDLE hButtonGroup = TTLib_GetButtonGroup(hTaskbar, i);

			TTLIB_GROUPTYPE nButtonGroupType;
			if(!TTLib_GetButtonGroupType(hButtonGroup, &nButtonGroupType) ||
				nButtonGroupType == TTLIB_GROUPTYPE_UNKNOWN ||
				nButtonGroupType == TTLIB_GROUPTYPE_TEMPORARY)
			{
				continue;
			}

			TaskbarItem taskbarItem;

			TTLib_GetButtonGroupAppId(hButtonGroup, taskbarItem.appId.GetBuffer(MAX_APPID_LENGTH), MAX_APPID_LENGTH);
			taskbarItem.appId.ReleaseBuffer();

			if(nButtonGroupType != TTLIB_GROUPTYPE_PINNED)
			{
				taskbarItem.pinned = false;

				int nButtonCount;
				if(TTLib_GetButtonCount(hButtonGroup, &nButtonCount))
				{
					for(int j = 0; j < nButtonCount; j++)
					{
						HANDLE hButton = TTLib_GetButton(hButtonGroup, j);
						HWND hWnd = TTLib_GetButtonWindow(hButton);
						taskbarItem.windows.push_back(hWnd);
					}
				}
			}
			else
			{
				taskbarItem.pinned = true;
			}

			taskbarInfo.taskbarItems.push_back(std::move(taskbarItem));
		}
	}
}

void VirtualDesktops::RestoreMonitorsInfo()
{
	RestoreTaskbarsInfo();

	// TODO: Wallpaper?

	m_desktops[m_currentDesktopId].monitorsInfo.clear();
}

void VirtualDesktops::RestoreTaskbarsInfo()
{
	if(!m_TTLIbLoaded)
		return;

	TaskbarManipulator taskbarManipulator;
	if(!taskbarManipulator.IsManipulating())
		return;

	HANDLE hTaskbar = TTLib_GetMainTaskbar();
	RestoreTaskbarInfo(hTaskbar);

	int nCount;
	if(TTLib_GetSecondaryTaskbarCount(&nCount))
	{
		for(int i = 0; i < nCount; i++)
		{
			hTaskbar = TTLib_GetSecondaryTaskbar(i);
			RestoreTaskbarInfo(hTaskbar);
		}
	}
}

void VirtualDesktops::RestoreTaskbarInfo(HANDLE hTaskbar)
{
	HMONITOR hMonitor = TTLib_GetTaskbarMonitor(hTaskbar);

	MONITORINFOEX systemMonitorInfo;
	systemMonitorInfo.cbSize = sizeof(MONITORINFOEX);
	if(!GetMonitorInfo(hMonitor, &systemMonitorInfo))
	{
		DEBUG_LOG(logERROR) << "GetMonitorInfo failed for monitor " << hMonitor;
		return;
	}

	TaskbarInfo &taskbarInfo = m_desktops[m_currentDesktopId].monitorsInfo[systemMonitorInfo.szDevice].taskbarInfo;

	int nButtonGroupCount;
	if(TTLib_GetButtonGroupCount(hTaskbar, &nButtonGroupCount))
	{
		int nButtonGroupPosition = 0;
		std::vector<int> nButtonPositions;

		for(const auto &taskbarItem : taskbarInfo.taskbarItems)
		{
			if(taskbarItem.pinned)
			{
				for(int i = nButtonGroupPosition; i < nButtonGroupCount; i++)
				{
					HANDLE hButtonGroup = TTLib_GetButtonGroup(hTaskbar, i);

					TTLIB_GROUPTYPE nButtonGroupType;
					if(!TTLib_GetButtonGroupType(hButtonGroup, &nButtonGroupType) ||
						nButtonGroupType == TTLIB_GROUPTYPE_UNKNOWN ||
						nButtonGroupType == TTLIB_GROUPTYPE_TEMPORARY)
					{
						continue;
					}

					if(nButtonGroupType != TTLIB_GROUPTYPE_PINNED)
					{
						continue;
					}

					WCHAR szAppId[MAX_APPID_LENGTH];
					TTLib_GetButtonGroupAppId(hButtonGroup, szAppId, MAX_APPID_LENGTH);

					if(taskbarItem.appId == szAppId)
					{
						if(i > nButtonGroupPosition)
							TTLib_ButtonGroupMove(hTaskbar, i, nButtonGroupPosition);
						else
							assert(i == nButtonGroupPosition);

						nButtonGroupPosition++;
						nButtonPositions.push_back(0);
						assert(nButtonGroupPosition == nButtonPositions.size());
						break;
					}
				}
			}
			else
			{
				for(HWND hIterWnd : taskbarItem.windows)
				{
					for(int i = 0; i < nButtonGroupCount; i++)
					{
						HANDLE hButtonGroup = TTLib_GetButtonGroup(hTaskbar, i);

						TTLIB_GROUPTYPE nButtonGroupType;
						if(!TTLib_GetButtonGroupType(hButtonGroup, &nButtonGroupType) ||
							nButtonGroupType == TTLIB_GROUPTYPE_UNKNOWN ||
							nButtonGroupType == TTLIB_GROUPTYPE_TEMPORARY)
						{
							continue;
						}

						if(nButtonGroupType == TTLIB_GROUPTYPE_PINNED)
						{
							continue;
						}

						int nButtonCount;
						if(TTLib_GetButtonCount(hButtonGroup, &nButtonCount))
						{
							int nStartIndex = 0;
							if(i < nButtonGroupPosition)
							{
								nStartIndex = nButtonPositions[i];
								assert(nStartIndex > 0);
							}

							bool found = false;
							for(int j = nStartIndex; j < nButtonCount; j++)
							{
								HANDLE hButton = TTLib_GetButton(hButtonGroup, j);
								HWND hWnd = TTLib_GetButtonWindow(hButton);
								if(hWnd == hIterWnd)
								{
									int nTargetButtonIndex;

									if(i >= nButtonGroupPosition)
									{
										if(i > nButtonGroupPosition)
											TTLib_ButtonGroupMove(hTaskbar, i, nButtonGroupPosition);

										nTargetButtonIndex = 0;

										nButtonGroupPosition++;
										nButtonPositions.push_back(1);
										assert(nButtonGroupPosition == nButtonPositions.size());
									}
									else
									{
										nTargetButtonIndex = nButtonPositions[i];
										nButtonPositions[i]++;
									}

									if(j > nTargetButtonIndex)
										TTLib_ButtonMoveInButtonGroup(hButtonGroup, j, nTargetButtonIndex);
									else
										assert(j == nTargetButtonIndex);

									found = true;
									break;
								}
							}

							if(found)
								break;
						}
					}
				}
			}
		}
	}
}

void VirtualDesktops::EnableTaskbarsRedrawind(bool enable)
{
	if(!m_TTLIbLoaded)
		return;

	TaskbarManipulator taskbarManipulator;
	if(!taskbarManipulator.IsManipulating())
		return;

	HANDLE hTaskbar = TTLib_GetMainTaskbar();
	CWindow taskListWindow = TTLib_GetTaskListWindow(hTaskbar);
	taskListWindow.SetRedraw(enable);

	int nCount;
	if(TTLib_GetSecondaryTaskbarCount(&nCount))
	{
		for(int i = 0; i < nCount; i++)
		{
			hTaskbar = TTLib_GetSecondaryTaskbar(i);
			taskListWindow = TTLib_GetTaskListWindow(hTaskbar);
			taskListWindow.SetRedraw(enable);
		}
	}
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

	bool ShowWindowOnSwitch(HWND hWnd, bool show, bool activate /*= false*/)
	{
		assert(show || !activate); // can't hide and activate

		DWORD dwSWPflags = SWP_NOMOVE | SWP_NOSIZE | SWP_ASYNCWINDOWPOS |
			(activate ? 0 : SWP_NOACTIVATE | SWP_NOZORDER) |
			(show ? SWP_SHOWWINDOW : SWP_HIDEWINDOW);
		bool result = FALSE != SetWindowPos(hWnd, activate ? HWND_TOP : NULL, 0, 0, 0, 0, dwSWPflags);

		if(!result)
		{
			DEBUG_LOG(logWARNING) << "SetWindowPos failed with " << GetLastError() << " for window " << hWnd;
		}

		// Wait for the window to actually show/hide, with a timeout of 200 ms.
		//if(result && !SendMessageTimeout(hWnd, WM_NULL, 0, 0, SMTO_ABORTIFHUNG, 200, NULL))
		//{
		//	DEBUG_LOG(logWARNING) << "SendMessageTimeout failed with " << GetLastError() << " for window " << hWnd;
		//}

		return result;
	}
}
