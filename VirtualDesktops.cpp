#include "stdafx.h"
#include "VirtualDesktops.h"

#include "SetWindowPosTimeout.h"

#define SHOW_WINDOW_TIMEOUT      100
#define WAIT_FOR_WINDOWS_TIMEOUT 500

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
	bool ShowWindowOnSwitch(HWND hWnd, bool show, bool activate = false, DWORD dwTimeout = SHOW_WINDOW_TIMEOUT);
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
	CreateFooWindow();

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

	std::vector<HWND> windowsShown;

	for(DesktopInfo &desktop : m_desktops)
	{
		auto &windows = desktop.windowsInfo.zOrderedWindows;
		for(auto i = windows.rbegin(); i != windows.rend(); ++i)
		{
			// Suppress animation lazily.
			suppressor.Suppress();

			CWindow window(*i);
			if(ShowWindowOnSwitch(window, true))
			{
				windowsShown.push_back(window);
			}
		}
	}

	if(suppressor.IsSuppressed())
	{
		WaitForWindows(windowsShown, WAIT_FOR_WINDOWS_TIMEOUT);
	}

	DestroyFooWindow();
}

void VirtualDesktops::SwitchDesktop(int desktopId)
{
	if(desktopId == m_currentDesktopId)
		return;

	EnableTaskbarsRedrawind(false);
	SaveMonitorsInfo();

	SwitchDesktopWindows(desktopId);

	WaitForTaskbarIdle();

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

int VirtualDesktops::GetNumberOfDesktops()
{
	return static_cast<int>(m_desktops.size());
}

int VirtualDesktops::GetCurrentDesktop()
{
	return m_currentDesktopId;
}

bool VirtualDesktops::CanMoveWindowToDesktop(HWND hWnd)
{
	CWindow window(hWnd);

	if(window.GetWindowProcessID() != GetCurrentProcessId() &&
		window.IsWindowVisible())
	{
		DWORD dwExStyle = window.GetExStyle();
		if((dwExStyle & WS_EX_APPWINDOW) || !(dwExStyle & WS_EX_TOOLWINDOW)/* || IsWindowVisibleOnScreen(window)*/)
		{
			return true;
		}
	}

	return false;
}

bool VirtualDesktops::MoveWindowToDesktop(HWND hWnd, int desktopId)
{
	if(desktopId == m_currentDesktopId)
		return false;

	CWindow window(hWnd);
	DesktopInfo &targetDesktop = m_desktops[desktopId];

	if(window.GetWindowProcessID() != GetCurrentProcessId() &&
		window.IsWindowVisible())
	{
		DWORD dwExStyle = window.GetExStyle();
		if((dwExStyle & WS_EX_APPWINDOW) || !(dwExStyle & WS_EX_TOOLWINDOW)/* || IsWindowVisibleOnScreen(window)*/)
		{
			if(ShowWindowOnSwitch(hWnd, false))
			{
				targetDesktop.windowsInfo.zOrderedWindows.push_back(hWnd);
				return true;
			}
		}
	}

	return false;
}

bool VirtualDesktops::CreateFooWindow()
{
	WNDCLASS wndclass = { 0 };
	wndclass.lpfnWndProc = DefWindowProc;
	wndclass.hInstance = GetModuleHandle(NULL);
	wndclass.lpszClassName = L"VirtuozFooWnd";

	ATOM atom = RegisterClass(&wndclass);
	if(atom)
	{
		if(m_fooWindow.Create(wndclass.lpszClassName, NULL, NULL, NULL, WS_POPUP | WS_DISABLED))
		{
			return true;
		}

		UnregisterClass(MAKEINTATOM(atom), wndclass.hInstance);
	}

	DEBUG_LOG(logERROR) << "Something went wrong while creating fooWindow";

	return false;
}

void VirtualDesktops::DestroyFooWindow()
{
	if(!m_fooWindow)
	{
		return;
	}

	m_fooWindow.DestroyWindow();
	UnregisterClass(L"VirtuozFooWnd", GetModuleHandle(NULL));
}

void VirtualDesktops::SwitchDesktopWindows(int desktopId)
{
	assert(desktopId != m_currentDesktopId);

	WindowsAnimationSuppressor suppressor(false);

	std::vector<HWND> windowsShown;

	struct CALLBACK_PARAM {
		VirtualDesktops *pThis;
		WindowsAnimationSuppressor *pSuppressor;
		std::vector<HWND> *pWindowsShown;
	} param = { this, &suppressor, &windowsShown };

	m_desktops[m_currentDesktopId].windowsInfo.hForegroundWindow = GetForegroundWindow();

	EnumWindows([](HWND hWnd, LPARAM lParam) {
		CWindow window(hWnd);
		CALLBACK_PARAM *pParam = reinterpret_cast<CALLBACK_PARAM *>(lParam);
		VirtualDesktops *pThis = pParam->pThis;
		WindowsAnimationSuppressor *pSuppressor = pParam->pSuppressor;
		std::vector<HWND> *pWindowsShown = pParam->pWindowsShown;

		DesktopInfo &currentDesktop = pThis->m_desktops[pThis->m_currentDesktopId];

		if(window.GetWindowProcessID() != GetCurrentProcessId() &&
			window.IsWindowVisible())
		{
			DWORD dwExStyle = window.GetExStyle();
			if((dwExStyle & WS_EX_APPWINDOW) || !(dwExStyle & WS_EX_TOOLWINDOW)/* || IsWindowVisibleOnScreen(window)*/)
			{
				// Suppress animation lazily.
				pSuppressor->Suppress();

				if(ShowWindowOnSwitch(hWnd, false))
				{
					currentDesktop.windowsInfo.zOrderedWindows.push_back(hWnd);
					pWindowsShown->push_back(hWnd);
				}
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
		if(ShowWindowOnSwitch(window, true, window == hForegroundWindow))
		{
			windowsShown.push_back(window);
		}
	}

	m_desktops[m_currentDesktopId].windowsInfo = WindowsInfo();

	// We wait even if animation is not suppressed,
	// because we want the taskbars to be ready for reordering.
	//if(suppressor.IsSuppressed())
	{
		WaitForWindows(windowsShown, WAIT_FOR_WINDOWS_TIMEOUT);
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

void VirtualDesktops::WaitForTaskbarIdle()
{
	if(!m_TTLIbLoaded || !m_fooWindow)
		return;

	m_fooWindow.ShowWindow(SW_SHOWNA);

	bool found = false;
	while(!found)
	{
		TaskbarManipulator taskbarManipulator;
		if(!taskbarManipulator.IsManipulating())
			break;

		HANDLE hTaskbar = TTLib_GetMainTaskbar();
		if(FindWindowOnTaskbar(hTaskbar, m_fooWindow))
		{
			found = true;
			break;
		}

		int nCount;
		if(TTLib_GetSecondaryTaskbarCount(&nCount))
		{
			for(int i = 0; i < nCount; i++)
			{
				hTaskbar = TTLib_GetSecondaryTaskbar(i);
				if(FindWindowOnTaskbar(hTaskbar, m_fooWindow))
				{
					found = true;
					break;
				}
			}
		}
	}

	m_fooWindow.ShowWindow(SW_HIDE);

	while(found)
	{
		found = false;

		TaskbarManipulator taskbarManipulator;
		if(!taskbarManipulator.IsManipulating())
			break;

		HANDLE hTaskbar = TTLib_GetMainTaskbar();
		if(FindWindowOnTaskbar(hTaskbar, m_fooWindow))
		{
			found = true;
			continue;
		}

		int nCount;
		if(TTLib_GetSecondaryTaskbarCount(&nCount))
		{
			for(int i = 0; i < nCount; i++)
			{
				hTaskbar = TTLib_GetSecondaryTaskbar(i);
				if(FindWindowOnTaskbar(hTaskbar, m_fooWindow))
				{
					found = true;
					break;
				}
			}
		}
	}
}

bool VirtualDesktops::FindWindowOnTaskbar(HANDLE hTaskbar, HWND hWnd)
{
	int nButtonGroupCount;
	if(TTLib_GetButtonGroupCount(hTaskbar, &nButtonGroupCount))
	{
		for(int i = 0; i < nButtonGroupCount; i++)
		{
			HANDLE hButtonGroup = TTLib_GetButtonGroup(hTaskbar, i);

			TTLIB_GROUPTYPE nButtonGroupType;
			if(!TTLib_GetButtonGroupType(hButtonGroup, &nButtonGroupType))
			{
				continue;
			}

			if(nButtonGroupType != TTLIB_GROUPTYPE_NORMAL &&
				nButtonGroupType != TTLIB_GROUPTYPE_COMBINED)
			{
				continue;
			}

			int nButtonCount;
			if(TTLib_GetButtonCount(hButtonGroup, &nButtonCount))
			{
				for(int j = 0; j < nButtonCount; j++)
				{
					HANDLE hButton = TTLib_GetButton(hButtonGroup, j);
					if(hWnd == TTLib_GetButtonWindow(hButton))
					{
						return true;
					}
				}
			}
		}
	}

	return false;
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
	if(enable)
	{
		taskListWindow.EnableWindow(TRUE);
		taskListWindow.SetRedraw(TRUE);
	}
	else
	{
		taskListWindow.SetRedraw(FALSE);
		taskListWindow.EnableWindow(FALSE);
	}

	int nCount;
	if(TTLib_GetSecondaryTaskbarCount(&nCount))
	{
		for(int i = 0; i < nCount; i++)
		{
			hTaskbar = TTLib_GetSecondaryTaskbar(i);
			taskListWindow = TTLib_GetTaskListWindow(hTaskbar);
			if(enable)
			{
				taskListWindow.EnableWindow(TRUE);
				taskListWindow.SetRedraw(TRUE);
			}
			else
			{
				taskListWindow.SetRedraw(FALSE);
				taskListWindow.EnableWindow(FALSE);
			}
		}
	}
}

void VirtualDesktops::WaitForWindows(std::vector<HWND> windows, DWORD dwTimeout)
{
	DWORD dwStartTime = GetTickCount();
	DWORD dwTimeLeft = dwTimeout;

	for(auto &hWnd : windows)
	{
		if(!SendMessageTimeout(hWnd, WM_NULL, 0, 0, SMTO_ABORTIFHUNG, dwTimeLeft, NULL))
		{
			DEBUG_LOG(logWARNING) << "SendMessageTimeout failed for window " << hWnd;
		}

		DWORD dwTime = GetTickCount();
		if(dwTime >= dwStartTime + dwTimeout)
		{
			DEBUG_LOG(logWARNING) << "WaitForWindows timeout for window " << hWnd;
			return;
		}

		dwTimeLeft = (dwStartTime + dwTimeout) - dwTime;
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

	bool ShowWindowOnSwitch(HWND hWnd, bool show, bool activate /*= false*/, DWORD dwTimeout /*= SHOW_WINDOW_TIMEOUT*/)
	{
		assert(show || !activate); // can't hide and activate

		DWORD dwSWPflags = SWP_NOMOVE | SWP_NOSIZE | //SWP_ASYNCWINDOWPOS |
			(activate ? 0 : SWP_NOACTIVATE) | SWP_NOZORDER |
			(show ? SWP_SHOWWINDOW : SWP_HIDEWINDOW);
		bool result = FALSE != SetWindowPosTimeout(hWnd, activate ? HWND_TOP : NULL, 0, 0, 0, 0, dwSWPflags, dwTimeout);

		if(!result)
		{
			DEBUG_LOG(logWARNING) << "SetWindowPos failed with " << GetLastError() << " for window " << hWnd;
		}

		return result;
	}
}
