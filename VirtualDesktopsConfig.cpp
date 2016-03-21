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
		iniFilePath.RenameExtension(_T(".ini"));

		return iniFilePath;
	}

	return CPath();
}

bool VirtualDesktopsConfig::LoadFromIniFile()
{
	CPath iniFilePath = GetIniFilePath();
	if(!iniFilePath)
		return false;

	{
		TCHAR log_path[MAX_PATH];
		if(GetPrivateProfileString(_T("config"), _T("logpath"), NULL, log_path, MAX_PATH, iniFilePath) > 0)
		{
			if(PathIsRelative(log_path))
			{
				TCHAR module_path[MAX_PATH];
				if(GetModuleFileName(NULL, module_path, MAX_PATH) == 0)
				{
					TCHAR combined_path[MAX_PATH];
					PathRemoveFileSpec(module_path);
					PathCombine(combined_path, module_path, log_path);
					_tcscpy_s(log_path, MAX_PATH, combined_path);
				}
			}

			FILE *log_handle = 0;
			if(_tfopen_s(&log_handle, log_path, _T("w+")) == 0)
			{
				Output2FILE::Stream() = log_handle;

				TCHAR log_level[100];
				if(GetPrivateProfileString(_T("config"), _T("loglevel"), NULL, log_level, 100, iniFilePath) > 0)
				{
					FILELog::ReportingLevel() = FILELog::FromString(log_level);
				}
			}
		}
	}

	FILE_LOG(logINFO) << "Loading configuration file " << std::wstring(iniFilePath.m_strPath);

	numberOfDesktops = GetPrivateProfileInt(_T("config"), _T("number_of_desktops"), 4, iniFilePath);
	if(numberOfDesktops < 2)
		numberOfDesktops = 2;
	else if(numberOfDesktops > 20)
		numberOfDesktops = 20;

	hotkeys.resize(numberOfDesktops);
	hotkeys_move.resize(numberOfDesktops);

	static const char pDefaultKeys[] = "1234567890qwertyuiop";
	CString iniKey;
	CString iniValue;

	// switch desktop hotkey
	for(int i = 0; i < numberOfDesktops; ++i)
	{
		iniKey.Format(_T("hotkey%d"), i + 1);
		GetPrivateProfileString(_T("config"), iniKey, NULL, iniValue.GetBuffer(100), 100, iniFilePath);
		iniValue.ReleaseBuffer();

		if(iniKey.IsEmpty() || !HotkeyFromString(iniValue, &hotkeys[i]))
		{
			hotkeys[i].vk = pDefaultKeys[i];
			hotkeys[i].fsModifiers = MOD_ALT;
		}
	}

	// move window hotkey
	for (int i = 0; i < numberOfDesktops; ++i)
	{
		iniKey.Format(_T("hotkey_move%d"), i + 1);
		GetPrivateProfileString(_T("config"), iniKey, NULL, iniValue.GetBuffer(100), 100, iniFilePath);
		iniValue.ReleaseBuffer();

		if (iniKey.IsEmpty() || !HotkeyFromString(iniValue, &hotkeys_move[i]))
		{
			hotkeys_move[i].vk = pDefaultKeys[i];
			hotkeys_move[i].fsModifiers = MOD_ALT | MOD_SHIFT;
		}
	}

	// show all hotkey
	{
		GetPrivateProfileString(_T("config"), _T("hotkey_show_all"), NULL, iniValue.GetBuffer(100), 100, iniFilePath);
		iniValue.ReleaseBuffer();

		if(iniValue.IsEmpty() || !HotkeyFromString(iniValue, &hotkey_show_all))
		{
			hotkey_show_all.vk = '0';
			hotkey_show_all.fsModifiers = MOD_ALT | MOD_SHIFT;
		}
	}

	// ignored executables
	{
		GetPrivateProfileString(_T("config"), _T("ignore"), NULL, iniValue.GetBuffer(100 * MAX_PATH), 100 * MAX_PATH, iniFilePath);
		iniValue.ReleaseBuffer();
		if(!iniValue.IsEmpty())
		{
			int pos = 0;
			CString token = iniValue.Tokenize(_T(";"), pos);
			while(!token.IsEmpty())
			{
				FILE_LOG(logDEBUG) << "Ignoring: " << (LPCTSTR) token;
				ignored_executables.insert((LPCTSTR) token);
				token = iniValue.Tokenize(_T(";"), pos);
			}
		}
	}

	return true;
}

bool VirtualDesktopsConfig::HotkeyFromString(const WCHAR *pszString, VirtualDesktopsConfigHotkey *pHotkey)
{
	static const std::unordered_map<std::wstring, UINT> keys = {
		{_T("0"),                   '0'},
		{_T("1"),                   '1'},
		{_T("2"),                   '2'},
		{_T("3"),                   '3'},
		{_T("4"),                   '4'},
		{_T("5"),                   '5'},
		{_T("6"),                   '6'},
		{_T("7"),                   '7'},
		{_T("8"),                   '8'},
		{_T("9"),                   '9'},
		{_T("A"),                   'A'},
		{_T("B"),                   'B'},
		{_T("C"),                   'C'},
		{_T("D"),                   'D'},
		{_T("E"),                   'E'},
		{_T("F"),                   'F'},
		{_T("G"),                   'G'},
		{_T("H"),                   'H'},
		{_T("I"),                   'I'},
		{_T("J"),                   'J'},
		{_T("K"),                   'K'},
		{_T("L"),                   'L'},
		{_T("M"),                   'M'},
		{_T("N"),                   'N'},
		{_T("O"),                   'O'},
		{_T("P"),                   'P'},
		{_T("Q"),                   'Q'},
		{_T("R"),                   'R'},
		{_T("S"),                   'S'},
		{_T("T"),                   'T'},
		{_T("U"),                   'U'},
		{_T("V"),                   'V'},
		{_T("W"),                   'W'},
		{_T("X"),                   'X'},
		{_T("Y"),                   'Y'},
		{_T("Z"),                   'Z'},
		{_T("BACK"),                VK_BACK},
		{_T("TAB"),                 VK_TAB},
		{_T("CLEAR"),               VK_CLEAR},
		{_T("RETURN"),              VK_RETURN},
		{_T("ENTER"),               VK_RETURN},  // !!! custom alternative name
		{_T("SHIFT"),               VK_SHIFT},
		{_T("CONTROL"),             VK_CONTROL},
		{_T("CTRL"),                VK_CONTROL},  // !!! custom alternative name
		{_T("MENU"),                VK_MENU},
		{_T("ALT"),                 VK_MENU},  // !!! custom alternative name
		{_T("PAUSE"),               VK_PAUSE},
		{_T("CAPITAL"),             VK_CAPITAL},
		{_T("CAPSLOCK"),            VK_CAPITAL},  // !!! custom alternative name
		{_T("KANA"),                VK_KANA},
		{_T("HANGEUL"),             VK_HANGEUL},  // old name - should be here for compatibility
		{_T("HANGUL"),              VK_HANGUL},
		{_T("JUNJA"),               VK_JUNJA},
		{_T("FINAL"),               VK_FINAL},
		{_T("HANJA"),               VK_HANJA},
		{_T("KANJI"),               VK_KANJI},
		{_T("ESCAPE"),              VK_ESCAPE},
		{_T("ESC"),                 VK_ESCAPE},  // !!! custom alternative name
		{_T("CONVERT"),             VK_CONVERT},
		{_T("NONCONVERT"),          VK_NONCONVERT},
		{_T("ACCEPT"),              VK_ACCEPT},
		{_T("MODECHANGE"),          VK_MODECHANGE},
		{_T("SPACE"),               VK_SPACE},
		{_T("PRIOR"),               VK_PRIOR},
		{_T("NEXT"),                VK_NEXT},
		{_T("END"),                 VK_END},
		{_T("HOME"),                VK_HOME},
		{_T("LEFT"),                VK_LEFT},
		{_T("UP"),                  VK_UP},
		{_T("RIGHT"),               VK_RIGHT},
		{_T("DOWN"),                VK_DOWN},
		{_T("SELECT"),              VK_SELECT},
		{_T("PRINT"),               VK_PRINT},
		{_T("EXECUTE"),             VK_EXECUTE},
		{_T("SNAPSHOT"),            VK_SNAPSHOT},
		{_T("INSERT"),              VK_INSERT},
		{_T("DELETE"),              VK_DELETE},
		{_T("DEL"),                 VK_DELETE},  // !!! custom alternative name
		{_T("HELP"),                VK_HELP},
		//{_T("LWIN"),                VK_LWIN}, // !!! NOT SUPPORTED
		{_T("WIN"),                 VK_LWIN},  // !!! custom alternative name
		//{_T("RWIN"),                VK_RWIN}, // !!! NOT SUPPORTED
		{_T("APPS"),                VK_APPS},
		{_T("SLEEP"),               VK_SLEEP},
		{_T("NUMPAD0"),             VK_NUMPAD0},
		{_T("NUMPAD1"),             VK_NUMPAD1},
		{_T("NUMPAD2"),             VK_NUMPAD2},
		{_T("NUMPAD3"),             VK_NUMPAD3},
		{_T("NUMPAD4"),             VK_NUMPAD4},
		{_T("NUMPAD5"),             VK_NUMPAD5},
		{_T("NUMPAD6"),             VK_NUMPAD6},
		{_T("NUMPAD7"),             VK_NUMPAD7},
		{_T("NUMPAD8"),             VK_NUMPAD8},
		{_T("NUMPAD9"),             VK_NUMPAD9},
		{_T("MULTIPLY"),            VK_MULTIPLY},
		{_T("ADD"),                 VK_ADD},
		{_T("SEPARATOR"),           VK_SEPARATOR},
		{_T("SUBTRACT"),            VK_SUBTRACT},
		{_T("DECIMAL"),             VK_DECIMAL},
		{_T("DIVIDE"),              VK_DIVIDE},
		{_T("F1"),                  VK_F1},
		{_T("F2"),                  VK_F2},
		{_T("F3"),                  VK_F3},
		{_T("F4"),                  VK_F4},
		{_T("F5"),                  VK_F5},
		{_T("F6"),                  VK_F6},
		{_T("F7"),                  VK_F7},
		{_T("F8"),                  VK_F8},
		{_T("F9"),                  VK_F9},
		{_T("F10"),                 VK_F10},
		{_T("F11"),                 VK_F11},
		{_T("F12"),                 VK_F12},
		{_T("F13"),                 VK_F13},
		{_T("F14"),                 VK_F14},
		{_T("F15"),                 VK_F15},
		{_T("F16"),                 VK_F16},
		{_T("F17"),                 VK_F17},
		{_T("F18"),                 VK_F18},
		{_T("F19"),                 VK_F19},
		{_T("F20"),                 VK_F20},
		{_T("F21"),                 VK_F21},
		{_T("F22"),                 VK_F22},
		{_T("F23"),                 VK_F23},
		{_T("F24"),                 VK_F24},
		{_T("NUMLOCK"),             VK_NUMLOCK},
		{_T("SCROLL"),              VK_SCROLL},
		{_T("OEM_NEC_EQUAL"),       VK_OEM_NEC_EQUAL},   // '=' key on numpad
		{_T("OEM_FJ_JISHO"),        VK_OEM_FJ_JISHO},   // 'Dictionary' key
		{_T("OEM_FJ_MASSHOU"),      VK_OEM_FJ_MASSHOU},   // 'Unregister word' key
		{_T("OEM_FJ_TOUROKU"),      VK_OEM_FJ_TOUROKU},   // 'Register word' key
		{_T("OEM_FJ_LOYA"),         VK_OEM_FJ_LOYA},   // 'Left OYAYUBI' key
		{_T("OEM_FJ_ROYA"),         VK_OEM_FJ_ROYA},   // 'Right OYAYUBI' key
		//{_T("LSHIFT"),              VK_LSHIFT},  // !! NOT SUPPORTED
		//{_T("RSHIFT"),              VK_RSHIFT},  // !! NOT SUPPORTED
		//{_T("LCONTROL"),            VK_LCONTROL},  // !! NOT SUPPORTED
		//{_T("RCONTROL"),            VK_RCONTROL},  // !! NOT SUPPORTED
		//{_T("LMENU"),               VK_LMENU},  // !! NOT SUPPORTED
		//{_T("RMENU"),               VK_RMENU},  // !! NOT SUPPORTED
		{_T("BROWSER_BACK"),        VK_BROWSER_BACK},
		{_T("BROWSER_FORWARD"),     VK_BROWSER_FORWARD},
		{_T("BROWSER_REFRESH"),     VK_BROWSER_REFRESH},
		{_T("BROWSER_STOP"),        VK_BROWSER_STOP},
		{_T("BROWSER_SEARCH"),      VK_BROWSER_SEARCH},
		{_T("BROWSER_FAVORITES"),   VK_BROWSER_FAVORITES},
		{_T("BROWSER_HOME"),        VK_BROWSER_HOME},
		{_T("VOLUME_MUTE"),         VK_VOLUME_MUTE},
		{_T("VOLUME_DOWN"),         VK_VOLUME_DOWN},
		{_T("VOLUME_UP"),           VK_VOLUME_UP},
		{_T("MEDIA_NEXT_TRACK"),    VK_MEDIA_NEXT_TRACK},
		{_T("MEDIA_PREV_TRACK"),    VK_MEDIA_PREV_TRACK},
		{_T("MEDIA_STOP"),          VK_MEDIA_STOP},
		{_T("MEDIA_PLAY_PAUSE"),    VK_MEDIA_PLAY_PAUSE},
		{_T("LAUNCH_MAIL"),         VK_LAUNCH_MAIL},
		{_T("LAUNCH_MEDIA_SELECT"), VK_LAUNCH_MEDIA_SELECT},
		{_T("LAUNCH_APP1"),         VK_LAUNCH_APP1},
		{_T("LAUNCH_APP2"),         VK_LAUNCH_APP2},
		{_T("OEM_1"),               VK_OEM_1},   // ';:' for US
		{_T("OEM_PLUS"),            VK_OEM_PLUS},   // '+' any country
		{_T("OEM_COMMA"),           VK_OEM_COMMA},   // ',' any country
		{_T("OEM_MINUS"),           VK_OEM_MINUS},   // '-' any country
		{_T("OEM_PERIOD"),          VK_OEM_PERIOD},   // '.' any country
		{_T("OEM_2"),               VK_OEM_2},   // '/?' for US
		{_T("OEM_3"),               VK_OEM_3},   // '`~' for US
		{_T("OEM_4"),               VK_OEM_4},  //  '[{' for US
		{_T("OEM_5"),               VK_OEM_5},  //  '\|' for US
		{_T("OEM_6"),               VK_OEM_6},  //  ']}' for US
		{_T("OEM_7"),               VK_OEM_7},  //  ''"' for US
		{_T("OEM_8"),               VK_OEM_8},
		{_T("OEM_AX"),              VK_OEM_AX},  //  'AX' key on Japanese AX kbd
		{_T("OEM_102"),             VK_OEM_102},  //  "<>" or "\|" on RT 102-key kbd.
		{_T("ICO_HELP"),            VK_ICO_HELP},  //  Help key on ICO
		{_T("ICO_00"),              VK_ICO_00},  //  00 key on ICO
		{_T("PROCESSKEY"),          VK_PROCESSKEY},
		{_T("ICO_CLEAR"),           VK_ICO_CLEAR},
		{_T("PACKET"),              VK_PACKET},
		{_T("OEM_RESET"),           VK_OEM_RESET},
		{_T("OEM_JUMP"),            VK_OEM_JUMP},
		{_T("OEM_PA1"),             VK_OEM_PA1},
		{_T("OEM_PA2"),             VK_OEM_PA2},
		{_T("OEM_PA3"),             VK_OEM_PA3},
		{_T("OEM_WSCTRL"),          VK_OEM_WSCTRL},
		{_T("OEM_CUSEL"),           VK_OEM_CUSEL},
		{_T("OEM_ATTN"),            VK_OEM_ATTN},
		{_T("OEM_FINISH"),          VK_OEM_FINISH},
		{_T("OEM_COPY"),            VK_OEM_COPY},
		{_T("OEM_AUTO"),            VK_OEM_AUTO},
		{_T("OEM_ENLW"),            VK_OEM_ENLW},
		{_T("OEM_BACKTAB"),         VK_OEM_BACKTAB},
		{_T("ATTN"),                VK_ATTN},
		{_T("CRSEL"),               VK_CRSEL},
		{_T("EXSEL"),               VK_EXSEL},
		{_T("EREOF"),               VK_EREOF},
		{_T("PLAY"),                VK_PLAY},
		{_T("ZOOM"),                VK_ZOOM},
		{_T("NONAME"),              VK_NONAME},
		{_T("PA1"),                 VK_PA1},
		{_T("OEM_CLEAR"),           VK_OEM_CLEAR},
	};

	CString tokenizeSrc(pszString);
	int tokenizePos = 0;
	CString strSingleKey;

	UINT vk = 0;
	UINT fsModifiers = 0;

	strSingleKey = tokenizeSrc.Tokenize(_T("+ \t"), tokenizePos);
	while(!strSingleKey.IsEmpty())
	{
		strSingleKey.Trim();
		if(!strSingleKey.IsEmpty())
		{
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

		strSingleKey = tokenizeSrc.Tokenize(_T("+ \t"), tokenizePos);
	}

	if(!vk)
		return false;

	pHotkey->vk = vk;
	pHotkey->fsModifiers = fsModifiers;

	return true;
}

CString VirtualDesktopsConfig::HotkeyToString(const VirtualDesktopsConfigHotkey &hotkey)
{
	static const std::unordered_map<UINT, CString> keyNames = {
		{'0',                   _T("0")},
		{'1',                   _T("1")},
		{'2',                   _T("2")},
		{'3',                   _T("3")},
		{'4',                   _T("4")},
		{'5',                   _T("5")},
		{'6',                   _T("6")},
		{'7',                   _T("7")},
		{'8',                   _T("8")},
		{'9',                   _T("9")},
		{'A',                   _T("A")},
		{'B',                   _T("B")},
		{'C',                   _T("C")},
		{'D',                   _T("D")},
		{'E',                   _T("E")},
		{'F',                   _T("F")},
		{'G',                   _T("G")},
		{'H',                   _T("H")},
		{'I',                   _T("I")},
		{'J',                   _T("J")},
		{'K',                   _T("K")},
		{'L',                   _T("L")},
		{'M',                   _T("M")},
		{'N',                   _T("N")},
		{'O',                   _T("O")},
		{'P',                   _T("P")},
		{'Q',                   _T("Q")},
		{'R',                   _T("R")},
		{'S',                   _T("S")},
		{'T',                   _T("T")},
		{'U',                   _T("U")},
		{'V',                   _T("V")},
		{'W',                   _T("W")},
		{'X',                   _T("X")},
		{'Y',                   _T("Y")},
		{'Z',                   _T("Z")},
		{VK_BACK,                _T("Back")},
		{VK_TAB,                 _T("Tab")},
		{VK_CLEAR,               _T("Clear")},
		//{VK_RETURN,              _T("Return")},
		{VK_RETURN,              _T("Enter")},  // !!! custom alternative name
		{VK_SHIFT,               _T("Shift")},
		//{VK_CONTROL,             _T("Control")},
		{VK_CONTROL,                _T("Ctrl")},  // !!! custom alternative name
		//{VK_MENU,                _T("Menu")},
		{VK_MENU,                 _T("Alt")},  // !!! custom alternative name
		{VK_PAUSE,               _T("Pause")},
		//{VK_CAPITAL,             _T("Capital")},
		{VK_CAPITAL,            _T("Capslock")},  // !!! custom alternative name
		{VK_KANA,                _T("Kana")},
		{VK_HANGEUL,             _T("Hangeul")},  // old name - should be here for compatibility
		{VK_HANGUL,              _T("Hangul")},
		{VK_JUNJA,               _T("Junja")},
		{VK_FINAL,               _T("Final")},
		{VK_HANJA,               _T("Hanja")},
		{VK_KANJI,               _T("Kanji")},
		{VK_ESCAPE,              _T("Escape")},
		//{VK_ESCAPE,                 _T("Esc")},  // !!! custom alternative name
		{VK_CONVERT,             _T("Convert")},
		{VK_NONCONVERT,          _T("Nonconvert")},
		{VK_ACCEPT,              _T("Accept")},
		{VK_MODECHANGE,          _T("Modechange")},
		{VK_SPACE,               _T("Space")},
		{VK_PRIOR,               _T("Prior")},
		{VK_NEXT,                _T("Next")},
		{VK_END,                 _T("End")},
		{VK_HOME,                _T("Home")},
		{VK_LEFT,                _T("Left")},
		{VK_UP,                  _T("Up")},
		{VK_RIGHT,               _T("Right")},
		{VK_DOWN,                _T("Down")},
		{VK_SELECT,              _T("Select")},
		{VK_PRINT,               _T("Print")},
		{VK_EXECUTE,             _T("Execute")},
		{VK_SNAPSHOT,            _T("Snapshot")},
		{VK_INSERT,              _T("Insert")},
		{VK_DELETE,              _T("Delete")},
		//{VK_DELETE,                 _T("Del")},  // !!! custom alternative name
		{VK_HELP,                _T("Help")},
		//{VK_LWIN,                _T("LWin")}, // !!! NOT SUPPORTED
		{VK_LWIN,                 _T("Win")},  // !!! custom alternative name
		//{VK_RWIN,                _T("RWin")}, // !!! NOT SUPPORTED
		{VK_APPS,                _T("Apps")},
		{VK_SLEEP,               _T("Sleep")},
		{VK_NUMPAD0,             _T("Numpad0")},
		{VK_NUMPAD1,             _T("Numpad1")},
		{VK_NUMPAD2,             _T("Numpad2")},
		{VK_NUMPAD3,             _T("Numpad3")},
		{VK_NUMPAD4,             _T("Numpad4")},
		{VK_NUMPAD5,             _T("Numpad5")},
		{VK_NUMPAD6,             _T("Numpad6")},
		{VK_NUMPAD7,             _T("Numpad7")},
		{VK_NUMPAD8,             _T("Numpad8")},
		{VK_NUMPAD9,             _T("Numpad9")},
		{VK_MULTIPLY,            _T("Multiply")},
		{VK_ADD,                 _T("Add")},
		{VK_SEPARATOR,           _T("Separator")},
		{VK_SUBTRACT,            _T("Subtract")},
		{VK_DECIMAL,             _T("Decimal")},
		{VK_DIVIDE,              _T("Divide")},
		{VK_F1,                  _T("F1")},
		{VK_F2,                  _T("F2")},
		{VK_F3,                  _T("F3")},
		{VK_F4,                  _T("F4")},
		{VK_F5,                  _T("F5")},
		{VK_F6,                  _T("F6")},
		{VK_F7,                  _T("F7")},
		{VK_F8,                  _T("F8")},
		{VK_F9,                  _T("F9")},
		{VK_F10,                 _T("F10")},
		{VK_F11,                 _T("F11")},
		{VK_F12,                 _T("F12")},
		{VK_F13,                 _T("F13")},
		{VK_F14,                 _T("F14")},
		{VK_F15,                 _T("F15")},
		{VK_F16,                 _T("F16")},
		{VK_F17,                 _T("F17")},
		{VK_F18,                 _T("F18")},
		{VK_F19,                 _T("F19")},
		{VK_F20,                 _T("F20")},
		{VK_F21,                 _T("F21")},
		{VK_F22,                 _T("F22")},
		{VK_F23,                 _T("F23")},
		{VK_F24,                 _T("F24")},
		{VK_NUMLOCK,             _T("Numlock")},
		{VK_SCROLL,              _T("Scroll")},
		{VK_OEM_NEC_EQUAL,       _T("OEM_NEC_EQUAL")},   // '=' key on numpad
		{VK_OEM_FJ_JISHO,        _T("OEM_FJ_JISHO")},   // 'Dictionary' key
		{VK_OEM_FJ_MASSHOU,      _T("OEM_FJ_MASSHOU")},   // 'Unregister word' key
		{VK_OEM_FJ_TOUROKU,      _T("OEM_FJ_TOUROKU")},   // 'Register word' key
		{VK_OEM_FJ_LOYA,         _T("OEM_FJ_LOYA")},   // 'Left OYAYUBI' key
		{VK_OEM_FJ_ROYA,         _T("OEM_FJ_ROYA")},   // 'Right OYAYUBI' key
		//{VK_LSHIFT,              _T("LShift")},  // !! NOT SUPPORTED
		//{VK_RSHIFT,              _T("RShift")},  // !! NOT SUPPORTED
		//{VK_LCONTROL,            _T("LControl")},  // !! NOT SUPPORTED
		//{VK_RCONTROL,            _T("RControl")},  // !! NOT SUPPORTED
		//{VK_LMENU,               _T("LMenu")},  // !! NOT SUPPORTED
		//{VK_RMENU,               _T("RMenu")},  // !! NOT SUPPORTED
		{VK_BROWSER_BACK,        _T("BROWSER_BACK")},
		{VK_BROWSER_FORWARD,     _T("BROWSER_FORWARD")},
		{VK_BROWSER_REFRESH,     _T("BROWSER_REFRESH")},
		{VK_BROWSER_STOP,        _T("BROWSER_STOP")},
		{VK_BROWSER_SEARCH,      _T("BROWSER_SEARCH")},
		{VK_BROWSER_FAVORITES,   _T("BROWSER_FAVORITES")},
		{VK_BROWSER_HOME,        _T("BROWSER_HOME")},
		{VK_VOLUME_MUTE,         _T("VOLUME_MUTE")},
		{VK_VOLUME_DOWN,         _T("VOLUME_DOWN")},
		{VK_VOLUME_UP,           _T("VOLUME_UP")},
		{VK_MEDIA_NEXT_TRACK,    _T("MEDIA_NEXT_TRACK")},
		{VK_MEDIA_PREV_TRACK,    _T("MEDIA_PREV_TRACK")},
		{VK_MEDIA_STOP,          _T("MEDIA_STOP")},
		{VK_MEDIA_PLAY_PAUSE,    _T("MEDIA_PLAY_PAUSE")},
		{VK_LAUNCH_MAIL,         _T("LAUNCH_MAIL")},
		{VK_LAUNCH_MEDIA_SELECT, _T("LAUNCH_MEDIA_SELECT")},
		{VK_LAUNCH_APP1,         _T("LAUNCH_APP1")},
		{VK_LAUNCH_APP2,         _T("LAUNCH_APP2")},
		{VK_OEM_1,               _T("OEM_1")},   // ';:' for US
		{VK_OEM_PLUS,            _T("OEM_PLUS")},   // '+' any country
		{VK_OEM_COMMA,           _T("OEM_COMMA")},   // ',' any country
		{VK_OEM_MINUS,           _T("OEM_MINUS")},   // '-' any country
		{VK_OEM_PERIOD,          _T("OEM_PERIOD")},   // '.' any country
		{VK_OEM_2,               _T("OEM_2")},   // '/?' for US
		{VK_OEM_3,               _T("OEM_3")},   // '`~' for US
		{VK_OEM_4,               _T("OEM_4")},  //  '[{' for US
		{VK_OEM_5,               _T("OEM_5")},  //  '\|' for US
		{VK_OEM_6,               _T("OEM_6")},  //  ']}' for US
		{VK_OEM_7,               _T("OEM_7")},  //  ''"' for US
		{VK_OEM_8,               _T("OEM_8")},
		{VK_OEM_AX,              _T("OEM_AX")},  //  'AX' key on Japanese AX kbd
		{VK_OEM_102,             _T("OEM_102")},  //  "<>" or "\|" on RT 102-key kbd.
		{VK_ICO_HELP,            _T("ICO_HELP")},  //  Help key on ICO
		{VK_ICO_00,              _T("ICO_00")},  //  00 key on ICO
		{VK_PROCESSKEY,          _T("PROCESSKEY")},
		{VK_ICO_CLEAR,           _T("ICO_CLEAR")},
		{VK_PACKET,              _T("PACKET")},
		{VK_OEM_RESET,           _T("OEM_RESET")},
		{VK_OEM_JUMP,            _T("OEM_JUMP")},
		{VK_OEM_PA1,             _T("OEM_PA1")},
		{VK_OEM_PA2,             _T("OEM_PA2")},
		{VK_OEM_PA3,             _T("OEM_PA3")},
		{VK_OEM_WSCTRL,          _T("OEM_WSCTRL")},
		{VK_OEM_CUSEL,           _T("OEM_CUSEL")},
		{VK_OEM_ATTN,            _T("OEM_ATTN")},
		{VK_OEM_FINISH,          _T("OEM_FINISH")},
		{VK_OEM_COPY,            _T("OEM_COPY")},
		{VK_OEM_AUTO,            _T("OEM_AUTO")},
		{VK_OEM_ENLW,            _T("OEM_ENLW")},
		{VK_OEM_BACKTAB,         _T("OEM_BACKTAB")},
		{VK_ATTN,                _T("ATTN")},
		{VK_CRSEL,               _T("CRSEL")},
		{VK_EXSEL,               _T("EXSEL")},
		{VK_EREOF,               _T("EREOF")},
		{VK_PLAY,                _T("PLAY")},
		{VK_ZOOM,                _T("ZOOM")},
		{VK_NONAME,              _T("NONAME")},
		{VK_PA1,                 _T("PA1")},
		{VK_OEM_CLEAR,           _T("OEM_CLEAR")},
	};

	CString hotkeyName;

	if(hotkey.fsModifiers & MOD_ALT)
		hotkeyName += _T("Alt+");
	if(hotkey.fsModifiers & MOD_CONTROL)
		hotkeyName += _T("Ctrl+");
	if(hotkey.fsModifiers & MOD_SHIFT)
		hotkeyName += _T("Shift+");
	if(hotkey.fsModifiers & MOD_WIN)
		hotkeyName += _T("Win+");

	auto name_it = keyNames.find(hotkey.vk);
	if(name_it != keyNames.end())
		hotkeyName += name_it->second;
	else
		hotkeyName += _T("???");

	return hotkeyName;
}
