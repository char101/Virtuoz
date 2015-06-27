#pragma once

#include "VirtualDesktopsConfig.h"

struct DesktopInfo;

class VirtualDesktops
{
public:
	VirtualDesktops(VirtualDesktopsConfig config = VirtualDesktopsConfig());
	~VirtualDesktops();

	VirtualDesktops(const VirtualDesktops &) = delete;
	VirtualDesktops &operator=(const VirtualDesktops &) = delete;

	void SwitchDesktop(int desktopId);
	void OnTaskbarCreated();

private:
	void SwitchDesktopWindows(int desktopId);
	void SaveMonitorsInfo();
	void SaveTaskbarsInfo();
	void SaveTaskbarInfo(HANDLE hTaskbar);
	void RestoreMonitorsInfo();
	void RestoreTaskbarsInfo();
	void RestoreTaskbarInfo(HANDLE hTaskbar);
	void EnableTaskbarsRedrawind(bool enable);

	VirtualDesktopsConfig m_config;
	std::vector<DesktopInfo> m_desktops;
	int m_currentDesktopId = 0;
	bool m_TTLIbLoaded = false;
};
