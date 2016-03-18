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
		{ '1', MOD_ALT },
		{ '2', MOD_ALT },
		{ '3', MOD_ALT },
		{ '4', MOD_ALT }
	};

	std::vector<VirtualDesktopsConfigHotkey> hotkeys_move = std::vector<VirtualDesktopsConfigHotkey> {
		{ '1', MOD_ALT | MOD_SHIFT },
		{ '2', MOD_ALT | MOD_SHIFT },
		{ '3', MOD_ALT | MOD_SHIFT },
		{ '4', MOD_ALT | MOD_SHIFT },
	};

	VirtualDesktopsConfigHotkey hotkey_show_all = VirtualDesktopsConfigHotkey{'0', MOD_ALT | MOD_SHIFT};

	std::unordered_set<String> ignored_executables;

	static bool HotkeyFromString(const WCHAR *pszString, VirtualDesktopsConfigHotkey *pHotkey);
	static CString HotkeyToString(const VirtualDesktopsConfigHotkey &hotkey);

private:
	CPath GetIniFilePath();
	bool LoadFromIniFile();
};
