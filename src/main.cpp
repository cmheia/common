#include "stdafx.h"
#include "../res/resource.h"

#include "msg.h"

static char* __THIS_FILE__ = __FILE__;

Common::CComConfig* comcfg;

void com_load_config(void)
{
	char mp[MAX_PATH]={0};
	GetModuleFileName(NULL, mp, __ARRAY_SIZE(mp));
	strcpy(strrchr(mp, '\\')+1, "common.ini");
	comcfg = new Common::CComConfig;
	comcfg->LoadFile(mp);
}

void com_unload_config(void)
{
	if (auto item = comcfg->get_key("app.config.saveonexit")) {
		if (!item->get_bool()) {
			debug_puts("不保存配置");
			delete comcfg;
			return;
		}
	}
	debug_puts("保存配置...");
	comcfg->SaveFile();
	delete comcfg;
}

#ifdef _DEBUG
void init_log_env(LPSTR lpCmdLine)
{
	if (0 != strncmp("log", lpCmdLine, 3)) {
		if (!AllocConsole()) {
			return;
		}
		freopen("CONIN$", "r", stdin);
		freopen("CONOUT$", "w", stdout);
		freopen("CONOUT$", "w", stderr);
		do {
			HANDLE hStdout;
			BOOL ok;

			hStdout = GetStdHandle(STD_OUTPUT_HANDLE);

			CONSOLE_SCREEN_BUFFER_INFO bInfo; 
			GetConsoleScreenBufferInfo(hStdout, &bInfo);

			COORD coord;
			coord.X = GetSystemMetrics(SM_CXMIN) + 30;
			coord.Y = GetSystemMetrics(SM_CYMIN) + 30;
			// https://msdn.microsoft.com/en-us/library/windows/desktop/ms686044(v=vs.85).aspx
			ok = SetConsoleScreenBufferSize(hStdout, coord);

			printf("SM_C[XY]MIN = {%d, %d}\n", coord.X, coord.Y);

			SMALL_RECT rc;
			rc.Left = 0;
			rc.Top = 0;
			rc.Right = coord.X - 1;
			rc.Bottom = coord.Y - 1;

			// https://msdn.microsoft.com/en-us/library/windows/desktop/ms686125(v=vs.85).aspx
			ok = SetConsoleWindowInfo(hStdout, ok, &rc);

			// top left
			char console_title[32];
			SYSTEMTIME sys_time;

			GetLocalTime(&sys_time);
			srand(sys_time.wSecond * 1000 + sys_time.wMilliseconds);
			snprintf(console_title, sizeof console_title, "common-%08X", rand());
			SetConsoleTitle(console_title);
			Sleep(10);
			HWND console_hwnd = ::FindWindow("ConsoleWindowClass", console_title);
			printf("console_hwnd:%08X\n", (unsigned int)console_hwnd);
			::SetWindowPos(console_hwnd, HWND_TOP, 0 - 50 - ::GetSystemMetrics(SM_CXFRAME), 0, 0, 0, SWP_NOSIZE);
		} while (0);
	}
}
#endif

Common::c_the_app theApp;

int CALLBACK WinMain(HINSTANCE hInstance,HINSTANCE hPrevInstance,LPSTR lpCmdLine,int nShowCmd)
{
	//InitCommonControls();
	LoadLibrary("RichEd20.dll");

#ifdef _DEBUG
	init_log_env(lpCmdLine);
	debug_printl("CmdLine:[%s]", lpCmdLine);
#endif
	debug_out(("程序已运行\n"));

	com_load_config();

	Common::CComWnd maindlg;
	maindlg.Create(nullptr, MAKEINTRESOURCE(IDD_DLG_MAIN));
	maindlg.ShowWindow();

	Common::CWindowManager::MessageLoop();
	
	com_unload_config();

	debug_out(("程序已结束\n"));
#ifdef _DEBUG
	if (0 != strncmp("log", lpCmdLine, 3)) {
		Sleep(500);
		FreeConsole();
	}
#endif
	MessageBeep(MB_OK);
	return 0;
}

