#include "stdafx.h"
#include "VirtualDesktops.h"

#define WAIT_FOR_WINDOWS_TIMEOUT 500
#define FOO_WND_CLASS_NAME       L"VirtuozFooWnd"

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
	void WaitForWindows(std::vector<HWND> hiddenWindows, std::vector<HWND> shownWindows, DWORD dwTimeout = WAIT_FOR_WINDOWS_TIMEOUT);
	void WaitForShownWindows(std::vector<HWND> windows, bool shown, DWORD dwTimeout);
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
		WaitForWindows(std::vector<HWND>(), windowsShown);
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
	wndclass.lpszClassName = FOO_WND_CLASS_NAME;

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
	UnregisterClass(FOO_WND_CLASS_NAME, GetModuleHandle(NULL));
}

void VirtualDesktops::SwitchDesktopWindows(int desktopId)
{
	assert(desktopId != m_currentDesktopId);

	WindowsAnimationSuppressor suppressor(false);

	std::vector<HWND> windowsHidden, windowsShown;

	bool currentProcessIsForeground = false;
	CWindow foregroundWindow = GetForegroundWindow();
	if(foregroundWindow)
	{
		if(foregroundWindow.GetWindowProcessID() == GetCurrentProcessId())
		{
			currentProcessIsForeground = true;
		}
		else
		{
			m_desktops[m_currentDesktopId].windowsInfo.hForegroundWindow = GetForegroundWindow();
			SetForegroundWindow(GetDesktopWindow());
		}
	}

	struct CALLBACK_PARAM {
		VirtualDesktops *pThis;
		WindowsAnimationSuppressor *pSuppressor;
		std::vector<HWND> *pWindowsHidden;
	} param = { this, &suppressor, &windowsHidden };

	EnumWindows([](HWND hWnd, LPARAM lParam) {
		CWindow window(hWnd);
		CALLBACK_PARAM *pParam = reinterpret_cast<CALLBACK_PARAM *>(lParam);
		VirtualDesktops *pThis = pParam->pThis;
		WindowsAnimationSuppressor *pSuppressor = pParam->pSuppressor;
		std::vector<HWND> *pWindowsHidden = pParam->pWindowsHidden;

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
					pWindowsHidden->push_back(hWnd);
				}
			}
		}

		return TRUE;
	}, reinterpret_cast<LPARAM>(&param));

	m_currentDesktopId = desktopId;

	auto &windows = m_desktops[m_currentDesktopId].windowsInfo.zOrderedWindows;
	HWND hNewForegroundWindow = NULL;

	if(!currentProcessIsForeground)
		hNewForegroundWindow = m_desktops[m_currentDesktopId].windowsInfo.hForegroundWindow;

	for(auto i = windows.rbegin(); i != windows.rend(); ++i)
	{
		// Suppress animation lazily.
		suppressor.Suppress();

		CWindow window(*i);
		if(ShowWindowOnSwitch(window, true))
		{
			if(window == hNewForegroundWindow)
				SetForegroundWindow(window);

			windowsShown.push_back(window);
		}
	}

	m_desktops[m_currentDesktopId].windowsInfo = WindowsInfo();

	// We wait even if animation is not suppressed,
	// because we want the taskbars to be ready for reordering.
	//if(suppressor.IsSuppressed())
	{
		WaitForWindows(windowsHidden, windowsShown);
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

	const TaskbarInfo &taskbarInfo = m_desktops[m_currentDesktopId].monitorsInfo[systemMonitorInfo.szDevice].taskbarInfo;

	int nButtonGroupCount;
	if(TTLib_GetButtonGroupCount(hTaskbar, &nButtonGroupCount))
	{
		int nButtonGroupPosition = 0;
		std::vector<int> nButtonPositions;

		for(const auto &taskbarItem : taskbarInfo.taskbarItems)
		{
			if(taskbarItem.pinned)
			{
				if(!PlacePinnedItem(hTaskbar, nButtonGroupCount, nButtonGroupPosition, nButtonPositions, taskbarItem.appId))
				{
					OnPlacePinnedItemFailed(hTaskbar, nButtonGroupCount, nButtonGroupPosition, nButtonPositions, taskbarInfo.taskbarItems, taskbarItem);
				}
			}
			else
			{
				bool placed = false;
				for(HWND hIterWnd : taskbarItem.windows)
				{
					if(PlaceButtonItem(hTaskbar, nButtonGroupCount, nButtonGroupPosition, nButtonPositions, hIterWnd))
						placed = true;
				}

				if(!placed)
				{
					OnPlaceButtonItemFailed(hTaskbar, nButtonGroupCount, nButtonGroupPosition, nButtonPositions, taskbarInfo.taskbarItems, taskbarItem);
				}
			}
		}
	}
}

bool VirtualDesktops::PlacePinnedItem(HANDLE hTaskbar, int nButtonGroupCount, int &nButtonGroupPosition, std::vector<int> &nButtonPositions, const WCHAR *pszAppId)
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

		if(wcscmp(szAppId, pszAppId) == 0)
		{
			if(i > nButtonGroupPosition)
				TTLib_ButtonGroupMove(hTaskbar, i, nButtonGroupPosition);
			else
				assert(i == nButtonGroupPosition);

			nButtonGroupPosition++;
			nButtonPositions.push_back(0);
			assert(nButtonGroupPosition == nButtonPositions.size());
			return true;
		}
	}

	return false;
}

void VirtualDesktops::OnPlacePinnedItemFailed(HANDLE hTaskbar, int nButtonGroupCount, int &nButtonGroupPosition, std::vector<int> &nButtonPositions, const std::vector<TaskbarItem> &taskbarItems, const TaskbarItem &failedTaskbarItem)
{
	assert(failedTaskbarItem.pinned == true);

	// Make sure that there are no more items in taskbarItems with that AppId.
	for(const auto &taskbarItem : taskbarItems)
	{
		if(&taskbarItem == &failedTaskbarItem)
			continue;

		if(taskbarItem.appId == failedTaskbarItem.appId)
			return; // there's an additional item in the array with that AppId
	}

	// Make sure that there's exactly one (non-pinned) button group on the taskbar with that AppId.
	HANDLE hFoundButtonGroup = NULL;
	int nFoundButtonGroupIndex = 0;
	TTLIB_GROUPTYPE nFoundButtonGroupType = TTLIB_GROUPTYPE_UNKNOWN;

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

		WCHAR szAppId[MAX_APPID_LENGTH];
		TTLib_GetButtonGroupAppId(hButtonGroup, szAppId, MAX_APPID_LENGTH);

		if(wcscmp(szAppId, failedTaskbarItem.appId) == 0)
		{
			if(hFoundButtonGroup)
				return; // there's more than one such button group

			hFoundButtonGroup = hButtonGroup;
			nFoundButtonGroupIndex = i;
			nFoundButtonGroupType = nButtonGroupType;
		}
	}

	if(!hFoundButtonGroup)
		return; // not found

	if(nFoundButtonGroupType == TTLIB_GROUPTYPE_PINNED)
	{
		DEBUG_LOG(logWARNING) << "nFoundButtonGroupType == TTLIB_GROUPTYPE_PINNED";
		return;
	}

	// Make sure that all the windows of that button group are new, i.e. they don't exist in taskbarItems.
	int nFoundButtonCount;
	if(TTLib_GetButtonCount(hFoundButtonGroup, &nFoundButtonCount))
	{
		for(int i = 0; i < nFoundButtonCount; i++)
		{
			HANDLE hFoundButton = TTLib_GetButton(hFoundButtonGroup, i);
			HWND hFoundWnd = TTLib_GetButtonWindow(hFoundButton);

			for(const auto &taskbarItem : taskbarItems)
			{
				if(!taskbarItem.pinned)
				{
					for(auto hWnd : taskbarItem.windows)
					{
						if(hFoundWnd == hWnd)
							return; // one of the windows exists in taskbarItems.
					}
				}
			}
		}
	}
	else
	{
		DEBUG_LOG(logERROR) << "TTLib_GetButtonCount failed";
		return;
	}

	// Move it.
	if(nFoundButtonGroupIndex > nButtonGroupPosition)
		TTLib_ButtonGroupMove(hTaskbar, nFoundButtonGroupIndex, nButtonGroupPosition);
	else
		assert(nFoundButtonGroupIndex == nButtonGroupPosition);

	nButtonGroupPosition++;
	nButtonPositions.push_back(nFoundButtonCount);
	assert(nButtonGroupPosition == nButtonPositions.size());
}

bool VirtualDesktops::PlaceButtonItem(HANDLE hTaskbar, int nButtonGroupCount, int &nButtonGroupPosition, std::vector<int> &nButtonPositions, HWND hPlaceWnd)
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

			for(int j = nStartIndex; j < nButtonCount; j++)
			{
				HANDLE hButton = TTLib_GetButton(hButtonGroup, j);
				HWND hWnd = TTLib_GetButtonWindow(hButton);
				if(hWnd == hPlaceWnd)
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

					return true;
				}
			}
		}
	}

	return false;
}

void VirtualDesktops::OnPlaceButtonItemFailed(HANDLE hTaskbar, int nButtonGroupCount, int &nButtonGroupPosition, std::vector<int> &nButtonPositions, const std::vector<TaskbarItem> &taskbarItems, const TaskbarItem &failedTaskbarItem)
{
	assert(failedTaskbarItem.pinned == false);

	// Make sure that there are no more items in taskbarItems with that AppId.
	for(const auto &taskbarItem : taskbarItems)
	{
		if(&taskbarItem == &failedTaskbarItem)
			continue;

		if(taskbarItem.appId == failedTaskbarItem.appId)
			return; // there's an additional item in the array with that AppId
	}

	// Make sure that there's exactly one button group on the taskbar with that AppId.
	HANDLE hFoundButtonGroup = NULL;
	int nFoundButtonGroupIndex = 0;
	TTLIB_GROUPTYPE nFoundButtonGroupType = TTLIB_GROUPTYPE_UNKNOWN;

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

		WCHAR szAppId[MAX_APPID_LENGTH];
		TTLib_GetButtonGroupAppId(hButtonGroup, szAppId, MAX_APPID_LENGTH);

		if(wcscmp(szAppId, failedTaskbarItem.appId) == 0)
		{
			if(hFoundButtonGroup)
				return; // there's more than one such button group

			hFoundButtonGroup = hButtonGroup;
			nFoundButtonGroupIndex = i;
			nFoundButtonGroupType = nButtonGroupType;
		}
	}

	if(!hFoundButtonGroup)
		return; // not found

	// If the button group is non-pinned, make sure that all the windows
	// of that button group are new, i.e. they don't exist in taskbarItems.
	int nFoundButtonCount = 0;
	if(nFoundButtonGroupType != TTLIB_GROUPTYPE_PINNED)
	{
		if(TTLib_GetButtonCount(hFoundButtonGroup, &nFoundButtonCount))
		{
			for(int i = 0; i < nFoundButtonCount; i++)
			{
				HANDLE hFoundButton = TTLib_GetButton(hFoundButtonGroup, i);
				HWND hFoundWnd = TTLib_GetButtonWindow(hFoundButton);

				for(const auto &taskbarItem : taskbarItems)
				{
					if(!taskbarItem.pinned)
					{
						for(auto hWnd : taskbarItem.windows)
						{
							if(hFoundWnd == hWnd)
								return; // one of the windows exists in taskbarItems.
						}
					}
				}
			}
		}
		else
		{
			DEBUG_LOG(logERROR) << "TTLib_GetButtonCount failed";
			return;
		}
	}

	// Move it.
	if(nFoundButtonGroupIndex > nButtonGroupPosition)
		TTLib_ButtonGroupMove(hTaskbar, nFoundButtonGroupIndex, nButtonGroupPosition);
	else
		assert(nFoundButtonGroupIndex == nButtonGroupPosition);

	nButtonGroupPosition++;
	nButtonPositions.push_back(nFoundButtonCount);
	assert(nButtonGroupPosition == nButtonPositions.size());
}

void VirtualDesktops::WaitForTaskbarIdle()
{
	if(!m_TTLIbLoaded || !m_fooWindow)
		return;

	m_fooWindow.ShowWindow(SW_SHOWNA);

	while(!FindWindowOnTaskbars(m_fooWindow))
	{
		Sleep(10);
	}

	m_fooWindow.ShowWindow(SW_HIDE);

	while(FindWindowOnTaskbars(m_fooWindow))
	{
		Sleep(10);
	}
}

bool VirtualDesktops::FindWindowOnTaskbars(HWND hWnd)
{
	TaskbarManipulator taskbarManipulator;
	if(!taskbarManipulator.IsManipulating())
		return false;

	HANDLE hTaskbar = TTLib_GetMainTaskbar();
	if(FindWindowOnTaskbar(hTaskbar, hWnd))
	{
		return true;
	}

	int nCount;
	if(TTLib_GetSecondaryTaskbarCount(&nCount))
	{
		for(int i = 0; i < nCount; i++)
		{
			hTaskbar = TTLib_GetSecondaryTaskbar(i);
			if(FindWindowOnTaskbar(hTaskbar, m_fooWindow))
			{
				return true;
			}
		}
	}

	return false;
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

	std::vector<CWindow> taskListWindows;

	{
		TaskbarManipulator taskbarManipulator;
		if(!taskbarManipulator.IsManipulating())
			return;

		HANDLE hTaskbar = TTLib_GetMainTaskbar();
		taskListWindows.push_back(TTLib_GetTaskListWindow(hTaskbar));

		int nCount;
		if(TTLib_GetSecondaryTaskbarCount(&nCount))
		{
			for(int i = 0; i < nCount; i++)
			{
				hTaskbar = TTLib_GetSecondaryTaskbar(i);
				taskListWindows.push_back(TTLib_GetTaskListWindow(hTaskbar));
			}
		}
	}

	for(auto taskListWindow : taskListWindows)
	{
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
			(activate ? 0 : SWP_NOACTIVATE) | SWP_NOZORDER |
			(show ? SWP_SHOWWINDOW : SWP_HIDEWINDOW);
		if(!SetWindowPos(hWnd, activate ? HWND_TOP : NULL, 0, 0, 0, 0, dwSWPflags))
		{
			DEBUG_LOG(logWARNING) << "SetWindowPos failed with " << GetLastError() << " for window " << hWnd;
			return false;
		}

		return true;
	}

	void WaitForWindows(std::vector<HWND> hiddenWindows, std::vector<HWND> shownWindows, DWORD dwTimeout /*= WAIT_FOR_WINDOWS_TIMEOUT*/)
	{
		DWORD dwStartTime = GetTickCount();

		WaitForShownWindows(hiddenWindows, false, dwTimeout);

		DWORD dwTime = GetTickCount();
		if(dwTime >= dwStartTime + dwTimeout)
			return;

		WaitForShownWindows(shownWindows, true, (dwStartTime + dwTimeout) - dwTime);
	}

	void WaitForShownWindows(std::vector<HWND> windows, bool shown, DWORD dwTimeout)
	{
		DWORD dwStartTime = GetTickCount();

		for(auto &hWnd : windows)
		{
			for(;;)
			{
				if(!IsWindow(hWnd))
				{
					DEBUG_LOG(logWARNING) << "IsWindow failed for window " << hWnd;
					break;
				}

				SetLastError(ERROR_SUCCESS);
				BOOL bVisible = IsWindowVisible(hWnd);
				if(!bVisible)
				{
					DWORD dwError = GetLastError();
					if(dwError != ERROR_SUCCESS)
					{
						DEBUG_LOG(logWARNING) << "IsWindowVisible failed with " << dwError << " for window " << hWnd;
						break;
					}
				}

				if(shown ? bVisible : !bVisible)
					break;

				Sleep(10);

				if(GetTickCount() >= dwStartTime + dwTimeout)
				{
					DEBUG_LOG(logWARNING) << "WaitForWindows timeout for window " << hWnd;
					return;
				}
			}
		}
	}
}
