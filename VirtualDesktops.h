#pragma once

#include "VirtualDesktopsConfig.h"

struct DesktopInfo;
struct TaskbarItem;

class VirtualDesktops
{
public:
	VirtualDesktops(VirtualDesktopsConfig config = VirtualDesktopsConfig());
	~VirtualDesktops();

	VirtualDesktops(const VirtualDesktops &) = delete;
	VirtualDesktops &operator=(const VirtualDesktops &) = delete;

	void SwitchDesktop(int desktopId);
	void OnTaskbarCreated();
	int GetNumberOfDesktops();
	int GetCurrentDesktop();
	bool CanMoveWindowToDesktop(HWND hWnd);
	bool MoveWindowToDesktop(HWND hWnd, int desktopId);

private:
	bool CreateFooWindow();
	void DestroyFooWindow();

	void SwitchDesktopWindows(int desktopId);

	void SaveMonitorsInfo();
	void SaveTaskbarsInfo();
	void SaveTaskbarInfo(HANDLE hTaskbar);

	void RestoreMonitorsInfo();
	void RestoreTaskbarsInfo();
	void RestoreTaskbarInfo(HANDLE hTaskbar);
	bool PlacePinnedItem(HANDLE hTaskbar, int nButtonGroupCount, int &nButtonGroupPosition, std::vector<int> &nButtonPositions, const WCHAR *pszAppId);
	void OnPlacePinnedItemFailed(HANDLE hTaskbar, int nButtonGroupCount, int &nButtonGroupPosition, std::vector<int> &nButtonPositions, const std::vector<TaskbarItem> &taskbarItems, const TaskbarItem &failedTaskbarItem);
	bool PlaceButtonItem(HANDLE hTaskbar, int nButtonGroupCount, int &nButtonGroupPosition, std::vector<int> &nButtonPositions, HWND hPlaceWnd);
	void OnPlaceButtonItemFailed(HANDLE hTaskbar, int nButtonGroupCount, int &nButtonGroupPosition, std::vector<int> &nButtonPositions, const std::vector<TaskbarItem> &taskbarItems, const TaskbarItem &failedTaskbarItem);

	void WaitForTaskbarIdle();
	bool FindWindowOnTaskbars(HWND hWnd);
	bool FindWindowOnTaskbar(HANDLE hTaskbar, HWND hWnd);
	void EnableTaskbarsRedrawind(bool enable);

	VirtualDesktopsConfig m_config;
	std::vector<DesktopInfo> m_desktops;
	int m_currentDesktopId = 0;
	bool m_TTLIbLoaded = false;
	CWindow m_fooWindow;
};
