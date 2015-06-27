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
	VirtualDesktopsConfigHotkey hotkey_move_window = VirtualDesktopsConfigHotkey{ VK_OEM_3, MOD_ALT };

	static bool HotkeyFromString(const WCHAR *pszString, VirtualDesktopsConfigHotkey *pHotkey);
	static CString HotkeyToString(const VirtualDesktopsConfigHotkey &hotkey);

private:
	CPath GetIniFilePath();
	bool LoadFromIniFile();
};
