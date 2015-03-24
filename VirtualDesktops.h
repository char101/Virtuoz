#pragma once

#include "VirtualDesktopsConfig.h"

struct DesktopInfo;

class VirtualDesktops
{
public:
	VirtualDesktops(VirtualDesktopsConfig config = VirtualDesktopsConfig());
	~VirtualDesktops();

	void SwitchDesktop(int desktopId);

private:
	static bool IsWindowVisibleOnScreen(HWND hWnd);
	static bool ShowWindowOnSwitch(HWND hWnd, bool show);

	VirtualDesktopsConfig m_config;
	std::vector<DesktopInfo> m_desktops;
	int m_currentDesktopId = 0;
};
