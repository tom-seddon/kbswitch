#include "../common.inl"

#define DLL_NAME "kbswitch2_dll.dll"

#ifdef _M_X64
#define HOOKNAME(X) (X)
#else
#define HOOKNAME(X) ("_" X "@12")
#endif

static const TCHAR g_aWndClassName[]=_T("kbswitch2_helper_wnd");

static LRESULT CALLBACK WndProc(HWND hWnd,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
	return DefWindowProc(hWnd,uMsg,wParam,lParam);
}

static HWND CreateWnd(void)
{
	WNDCLASSEX w;
	HWND wnd_h;

	w.cbClsExtra=0;
	w.cbSize=sizeof w;
	w.cbWndExtra=0;
	w.hbrBackground=0;//GetStockObject(NULL_BRUSH);
	w.hCursor=0;//LoadCursor(0,IDC_ARROW);
	w.hIcon=0;//LoadIcon(0,IDI_APPLICATION);
	w.hIconSm=w.hIcon;
	w.hInstance=GetModuleHandle(0);
	w.lpfnWndProc=&WndProc;
	w.lpszClassName=g_aWndClassName;
	w.lpszMenuName=0;
	w.style=CS_HREDRAW|CS_VREDRAW;

	if(!RegisterClassEx(&w))
		return NULL;

	wnd_h=CreateWindow(g_aWndClassName,_T("kbswitch"),WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,0,0,
		GetModuleHandle(0),0);
	if(!wnd_h)
		return NULL;

#ifdef _DEBUG
	ShowWindow(wnd_h,SW_SHOW);
#endif//_DEBUG

	return wnd_h;
}

static HHOOK Hook(HMODULE dll_h,UINT type,const char *proc_name)
{
	HOOKPROC proc=(HOOKPROC)GetProcAddress(dll_h,proc_name);

	if(!proc)
	{
		dprintf("hook proc not found: %s\n",proc_name);
		return NULL;
	}

	return SetWindowsHookEx(type,proc,dll_h,0);
}

void Entry(void)
{
	HHOOK hooks[10];
	int num_hooks=0;
	HMODULE dll_h=NULL;
	HWND wnd_h=NULL;
	int result=1;

	dll_h=LoadLibrary(_T(DLL_NAME));
	if(!dll_h)
	{
		MessageBox(NULL,_T("Failed to load DLL \"") _T(DLL_NAME) _T("\"."),_T("Startup failed"),MB_OK|MB_ICONERROR);
		goto done;
	}

	hooks[num_hooks++]=Hook(dll_h,WH_CBT,HOOKNAME("KBSwitchHookProc"));
	hooks[num_hooks++]=Hook(dll_h,WH_CALLWNDPROCRET,HOOKNAME("KBSwitchCallWndProcRet"));

	wnd_h=CreateWnd();

	if(wnd_h)
	{
		for(;;)
		{
			int r;
			MSG msg;

			r=GetMessage(&msg,wnd_h,0,0);
			if(r==0||r==-1)
				break;

			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		result=0;
	}

done:
	if(wnd_h)
	{
		DestroyWindow(wnd_h);
		wnd_h=NULL;
	}

	{
		int i;

		for(i=0;i<num_hooks;++i)
			UnhookWindowsHookEx(hooks[i]);
	}

	if(dll_h)
	{
		FreeLibrary(dll_h);
		dll_h=NULL;
	}

	ExitProcess(result);
}
