#pragma once

struct VirtualDesktopsConfigHotkey
{
	UINT vk;
	UINT fsModifiers;
};

class VirtualDesktopsConfig
{
public:
	VirtualDesktopsConfig(bool loadFromIniFile = true);

	int numberOfDesktops = 4;
	std::vector<VirtualDesktopsConfigHotkey> hotkeys = std::vector<VirtualDesktopsConfigHotkey> {
		{ '1', MOD_ALT }, { '2', MOD_ALT }, { '3', MOD_ALT }, { '4', MOD_ALT }
	};

private:
	CPath GetIniFilePath();
	bool LoadFromIniFile();
	static bool HotkeyFromString(const WCHAR *pszString, VirtualDesktopsConfigHotkey *pHotkey);
};
