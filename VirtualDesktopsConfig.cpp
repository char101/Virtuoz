#include "stdafx.h"
#include "VirtualDesktopsConfig.h"

VirtualDesktopsConfig::VirtualDesktopsConfig(bool loadFromIniFile /*= true*/)
{
	if(loadFromIniFile)
		LoadFromIniFile();
}

CPath VirtualDesktopsConfig::GetIniFilePath()
{
	CPath iniFilePath;

	if(GetModuleFileName(NULL, iniFilePath.m_strPath.GetBuffer(MAX_PATH), MAX_PATH) != 0)
	{
		iniFilePath.m_strPath.ReleaseBuffer();
		iniFilePath.RenameExtension(L".ini");

		return iniFilePath;
	}

	return CPath();
}

bool VirtualDesktopsConfig::LoadFromIniFile()
{
	CPath iniFilePath = GetIniFilePath();
	if(!iniFilePath)
		return false;

	numberOfDesktops = GetPrivateProfileInt(L"config", L"number_of_desktops", 4, iniFilePath);
	if(numberOfDesktops < 2)
		numberOfDesktops = 2;
	else if(numberOfDesktops > 20)
		numberOfDesktops = 20;

	hotkeys.resize(numberOfDesktops);

	for(int i = 0; i < numberOfDesktops; i++)
	{
		CString iniKey;
		iniKey.Format(L"hotkey%d", i + 1);

		CString hotkeyName;
		GetPrivateProfileString(L"config", iniKey, NULL, hotkeyName.GetBuffer(MAX_PATH), MAX_PATH, iniFilePath);
		hotkeyName.ReleaseBuffer();

		if(!HotkeyFromString(hotkeyName, &hotkeys[i]))
		{
			static char *pDefaultKeys = "1234567890qwertyuiop";

			hotkeys[i].vk = pDefaultKeys[i];
			hotkeys[i].fsModifiers = MOD_ALT;
		}
	}

	return true;
}

bool VirtualDesktopsConfig::HotkeyFromString(const WCHAR *pszString, VirtualDesktopsConfigHotkey *pHotkey)
{
	static const std::unordered_map<std::wstring, UINT> keys = {
		{L"0",                   '0'},
		{L"1",                   '1'},
		{L"2",                   '2'},
		{L"3",                   '3'},
		{L"4",                   '4'},
		{L"5",                   '5'},
		{L"6",                   '6'},
		{L"7",                   '7'},
		{L"8",                   '8'},
		{L"9",                   '9'},
		{L"A",                   'A'},
		{L"B",                   'B'},
		{L"C",                   'C'},
		{L"D",                   'D'},
		{L"E",                   'E'},
		{L"F",                   'F'},
		{L"G",                   'G'},
		{L"H",                   'H'},
		{L"I",                   'I'},
		{L"J",                   'J'},
		{L"K",                   'K'},
		{L"L",                   'L'},
		{L"M",                   'M'},
		{L"N",                   'N'},
		{L"O",                   'O'},
		{L"P",                   'P'},
		{L"Q",                   'Q'},
		{L"R",                   'R'},
		{L"S",                   'S'},
		{L"T",                   'T'},
		{L"U",                   'U'},
		{L"V",                   'V'},
		{L"W",                   'W'},
		{L"X",                   'X'},
		{L"Y",                   'Y'},
		{L"Z",                   'Z'},
		{L"BACK",                VK_BACK},
		{L"TAB",                 VK_TAB},
		{L"CLEAR",               VK_CLEAR},
		{L"RETURN",              VK_RETURN},
		{L"SHIFT",               VK_SHIFT},
		{L"CONTROL",             VK_CONTROL},
		{L"CTRL",                VK_CONTROL},  // !!! custom alternative name
		{L"MENU",                VK_MENU},
		{L"ALT",                 VK_MENU},  // !!! custom alternative name
		{L"PAUSE",               VK_PAUSE},
		{L"CAPITAL",             VK_CAPITAL},
		{L"CAPSLOCK",            VK_CAPITAL},  // !!! custom alternative name
		{L"KANA",                VK_KANA},
		{L"HANGEUL",             VK_HANGEUL},  // old name - should be here for compatibility
		{L"HANGUL",              VK_HANGUL},
		{L"JUNJA",               VK_JUNJA},
		{L"FINAL",               VK_FINAL},
		{L"HANJA",               VK_HANJA},
		{L"KANJI",               VK_KANJI},
		{L"ESCAPE",              VK_ESCAPE},
		{L"ESC",                 VK_ESCAPE},  // !!! custom alternative name
		{L"CONVERT",             VK_CONVERT},
		{L"NONCONVERT",          VK_NONCONVERT},
		{L"ACCEPT",              VK_ACCEPT},
		{L"MODECHANGE",          VK_MODECHANGE},
		{L"SPACE",               VK_SPACE},
		{L"PRIOR",               VK_PRIOR},
		{L"NEXT",                VK_NEXT},
		{L"END",                 VK_END},
		{L"HOME",                VK_HOME},
		{L"LEFT",                VK_LEFT},
		{L"UP",                  VK_UP},
		{L"RIGHT",               VK_RIGHT},
		{L"DOWN",                VK_DOWN},
		{L"SELECT",              VK_SELECT},
		{L"PRINT",               VK_PRINT},
		{L"EXECUTE",             VK_EXECUTE},
		{L"SNAPSHOT",            VK_SNAPSHOT},
		{L"INSERT",              VK_INSERT},
		{L"DELETE",              VK_DELETE},
		{L"DEL",                 VK_DELETE},  // !!! custom alternative name
		{L"HELP",                VK_HELP},
		//{L"LWIN",                VK_LWIN}, // !!! NOT SUPPORTED
		{L"WIN",                 VK_LWIN},  // !!! custom alternative name
		//{L"RWIN",                VK_RWIN}, // !!! NOT SUPPORTED
		{L"APPS",                VK_APPS},
		{L"SLEEP",               VK_SLEEP},
		{L"NUMPAD0",             VK_NUMPAD0},
		{L"NUMPAD1",             VK_NUMPAD1},
		{L"NUMPAD2",             VK_NUMPAD2},
		{L"NUMPAD3",             VK_NUMPAD3},
		{L"NUMPAD4",             VK_NUMPAD4},
		{L"NUMPAD5",             VK_NUMPAD5},
		{L"NUMPAD6",             VK_NUMPAD6},
		{L"NUMPAD7",             VK_NUMPAD7},
		{L"NUMPAD8",             VK_NUMPAD8},
		{L"NUMPAD9",             VK_NUMPAD9},
		{L"MULTIPLY",            VK_MULTIPLY},
		{L"ADD",                 VK_ADD},
		{L"SEPARATOR",           VK_SEPARATOR},
		{L"SUBTRACT",            VK_SUBTRACT},
		{L"DECIMAL",             VK_DECIMAL},
		{L"DIVIDE",              VK_DIVIDE},
		{L"F1",                  VK_F1},
		{L"F2",                  VK_F2},
		{L"F3",                  VK_F3},
		{L"F4",                  VK_F4},
		{L"F5",                  VK_F5},
		{L"F6",                  VK_F6},
		{L"F7",                  VK_F7},
		{L"F8",                  VK_F8},
		{L"F9",                  VK_F9},
		{L"F10",                 VK_F10},
		{L"F11",                 VK_F11},
		{L"F12",                 VK_F12},
		{L"F13",                 VK_F13},
		{L"F14",                 VK_F14},
		{L"F15",                 VK_F15},
		{L"F16",                 VK_F16},
		{L"F17",                 VK_F17},
		{L"F18",                 VK_F18},
		{L"F19",                 VK_F19},
		{L"F20",                 VK_F20},
		{L"F21",                 VK_F21},
		{L"F22",                 VK_F22},
		{L"F23",                 VK_F23},
		{L"F24",                 VK_F24},
		{L"NUMLOCK",             VK_NUMLOCK},
		{L"SCROLL",              VK_SCROLL},
		{L"OEM_NEC_EQUAL",       VK_OEM_NEC_EQUAL},   // '=' key on numpad
		{L"OEM_FJ_JISHO",        VK_OEM_FJ_JISHO},   // 'Dictionary' key
		{L"OEM_FJ_MASSHOU",      VK_OEM_FJ_MASSHOU},   // 'Unregister word' key
		{L"OEM_FJ_TOUROKU",      VK_OEM_FJ_TOUROKU},   // 'Register word' key
		{L"OEM_FJ_LOYA",         VK_OEM_FJ_LOYA},   // 'Left OYAYUBI' key
		{L"OEM_FJ_ROYA",         VK_OEM_FJ_ROYA},   // 'Right OYAYUBI' key
		//{L"LSHIFT",              VK_LSHIFT},  // !! NOT SUPPORTED
		//{L"RSHIFT",              VK_RSHIFT},  // !! NOT SUPPORTED
		//{L"LCONTROL",            VK_LCONTROL},  // !! NOT SUPPORTED
		//{L"RCONTROL",            VK_RCONTROL},  // !! NOT SUPPORTED
		//{L"LMENU",               VK_LMENU},  // !! NOT SUPPORTED
		//{L"RMENU",               VK_RMENU},  // !! NOT SUPPORTED
		{L"BROWSER_BACK",        VK_BROWSER_BACK},
		{L"BROWSER_FORWARD",     VK_BROWSER_FORWARD},
		{L"BROWSER_REFRESH",     VK_BROWSER_REFRESH},
		{L"BROWSER_STOP",        VK_BROWSER_STOP},
		{L"BROWSER_SEARCH",      VK_BROWSER_SEARCH},
		{L"BROWSER_FAVORITES",   VK_BROWSER_FAVORITES},
		{L"BROWSER_HOME",        VK_BROWSER_HOME},
		{L"VOLUME_MUTE",         VK_VOLUME_MUTE},
		{L"VOLUME_DOWN",         VK_VOLUME_DOWN},
		{L"VOLUME_UP",           VK_VOLUME_UP},
		{L"MEDIA_NEXT_TRACK",    VK_MEDIA_NEXT_TRACK},
		{L"MEDIA_PREV_TRACK",    VK_MEDIA_PREV_TRACK},
		{L"MEDIA_STOP",          VK_MEDIA_STOP},
		{L"MEDIA_PLAY_PAUSE",    VK_MEDIA_PLAY_PAUSE},
		{L"LAUNCH_MAIL",         VK_LAUNCH_MAIL},
		{L"LAUNCH_MEDIA_SELECT", VK_LAUNCH_MEDIA_SELECT},
		{L"LAUNCH_APP1",         VK_LAUNCH_APP1},
		{L"LAUNCH_APP2",         VK_LAUNCH_APP2},
		{L"OEM_1",               VK_OEM_1},   // ';:' for US
		{L"OEM_PLUS",            VK_OEM_PLUS},   // '+' any country
		{L"OEM_COMMA",           VK_OEM_COMMA},   // ',' any country
		{L"OEM_MINUS",           VK_OEM_MINUS},   // '-' any country
		{L"OEM_PERIOD",          VK_OEM_PERIOD},   // '.' any country
		{L"OEM_2",               VK_OEM_2},   // '/?' for US
		{L"OEM_3",               VK_OEM_3},   // '`~' for US
		{L"OEM_4",               VK_OEM_4},  //  '[{' for US
		{L"OEM_5",               VK_OEM_5},  //  '\|' for US
		{L"OEM_6",               VK_OEM_6},  //  ']}' for US
		{L"OEM_7",               VK_OEM_7},  //  ''"' for US
		{L"OEM_8",               VK_OEM_8},
		{L"OEM_AX",              VK_OEM_AX},  //  'AX' key on Japanese AX kbd
		{L"OEM_102",             VK_OEM_102},  //  "<>" or "\|" on RT 102-key kbd.
		{L"ICO_HELP",            VK_ICO_HELP},  //  Help key on ICO
		{L"ICO_00",              VK_ICO_00},  //  00 key on ICO
		{L"PROCESSKEY",          VK_PROCESSKEY},
		{L"ICO_CLEAR",           VK_ICO_CLEAR},
		{L"PACKET",              VK_PACKET},
		{L"OEM_RESET",           VK_OEM_RESET},
		{L"OEM_JUMP",            VK_OEM_JUMP},
		{L"OEM_PA1",             VK_OEM_PA1},
		{L"OEM_PA2",             VK_OEM_PA2},
		{L"OEM_PA3",             VK_OEM_PA3},
		{L"OEM_WSCTRL",          VK_OEM_WSCTRL},
		{L"OEM_CUSEL",           VK_OEM_CUSEL},
		{L"OEM_ATTN",            VK_OEM_ATTN},
		{L"OEM_FINISH",          VK_OEM_FINISH},
		{L"OEM_COPY",            VK_OEM_COPY},
		{L"OEM_AUTO",            VK_OEM_AUTO},
		{L"OEM_ENLW",            VK_OEM_ENLW},
		{L"OEM_BACKTAB",         VK_OEM_BACKTAB},
		{L"ATTN",                VK_ATTN},
		{L"CRSEL",               VK_CRSEL},
		{L"EXSEL",               VK_EXSEL},
		{L"EREOF",               VK_EREOF},
		{L"PLAY",                VK_PLAY},
		{L"ZOOM",                VK_ZOOM},
		{L"NONAME",              VK_NONAME},
		{L"PA1",                 VK_PA1},
		{L"OEM_CLEAR",           VK_OEM_CLEAR},
	};

	CString tokenizeSrc(pszString);
	int tokenizePos = 0;
	CString strSingleKey;

	UINT vk = 0;
	UINT fsModifiers = 0;

	strSingleKey = tokenizeSrc.Tokenize(L"+ \t", tokenizePos);
	while(!strSingleKey.IsEmpty())
	{
		strSingleKey.Trim();
		if(!strSingleKey.IsEmpty())
		{
			DEBUG_LOG(logINFO) << "Processing key " << CStringA(strSingleKey);

			strSingleKey.MakeUpper();
			auto key_it = keys.find(std::wstring(strSingleKey));
			if(key_it == keys.end())
				return false;

			UINT key = key_it->second;

			switch(key)
			{
			case VK_MENU:
				fsModifiers |= MOD_ALT;
				break;

			case VK_CONTROL:
				fsModifiers |= MOD_CONTROL;
				break;

			case VK_SHIFT:
				fsModifiers |= MOD_SHIFT;
				break;

			case VK_LWIN:
				fsModifiers |= MOD_WIN;
				break;

			default:
				if(vk)
					return false;

				vk = key;
				break;
			}
		}

		strSingleKey = tokenizeSrc.Tokenize(L"+ \t", tokenizePos);
	}

	if(!vk)
		return false;

	pHotkey->vk = vk;
	pHotkey->fsModifiers = fsModifiers;

	return true;
}
