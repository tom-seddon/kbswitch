#include "../common.inl"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#ifdef _M_X64
#define HOOKNAME(X) (X)
#else
#define HOOKNAME(X) ("_" X "@12")
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const TCHAR WND_CLASS_NAME[]=_T("kbswitch2_helper_wnd");

static const char DLL_NAME[]="kbswitch2_dll_" PLATFORM_SUFFIX BUILD_SUFFIX ".dll";

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static UINT g_quitMsg;
static HMODULE g_hDLL;

typedef void (*SetKeyboardLayoutFn)(HKL);
static SetKeyboardLayoutFn g_pfnSetKeyboardLayout;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static LRESULT CALLBACK WndProc(HWND hWnd,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
	if(uMsg==g_quitMsg||uMsg==WM_CLOSE)
	{
		LOG("%s: msg=%u: time to quit.\n",__FUNCTION__,uMsg);

		PostQuitMessage(0);
		return 0;
	}
	else if(uMsg==WM_INPUTLANGCHANGEREQUEST)
	{
		LOG("%s: WM_INPUTLANGCHANGEREQUEST: HKL=%p.\n",__FUNCTION__,(HKL)lParam);

		(*g_pfnSetKeyboardLayout)((HKL)lParam);
	}

	return DefWindowProc(hWnd,uMsg,wParam,lParam);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static HWND CreateWnd(void)
{
	WNDCLASSEX w;
	HWND hWnd;

	w.cbClsExtra=0;
	w.cbSize=sizeof w;
	w.cbWndExtra=0;
	w.hbrBackground=0;//GetStockObject(NULL_BRUSH);
	w.hCursor=0;//LoadCursor(0,IDC_ARROW);
	w.hIcon=0;//LoadIcon(0,IDI_APPLICATION);
	w.hIconSm=w.hIcon;
	w.hInstance=GetModuleHandle(0);
	w.lpfnWndProc=&WndProc;
	w.lpszClassName=WND_CLASS_NAME;
	w.lpszMenuName=0;
	w.style=CS_HREDRAW|CS_VREDRAW;

	if(!RegisterClassEx(&w))
		return NULL;

	hWnd=CreateWindow(WND_CLASS_NAME,_T("kbswitch_helper"),WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,0,0,
		GetModuleHandle(0),0);
	if(!hWnd)
		return NULL;

#ifdef _DEBUG
	ShowWindow(hWnd,SW_SHOW);
#endif//_DEBUG

	return hWnd;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static FARPROC FindProc(const char *pProcName)
{
	FARPROC pfn=GetProcAddress(g_hDLL,pProcName);

	if(!pfn)
	{
		char msg[500];

		wsprintfA(msg,"Export missing: \"%s\".",pProcName);

		MessageBoxA(NULL,msg,DLL_NAME,MB_OK|MB_ICONERROR);
	}

	return pfn;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Entry(void)
{
	HHOOK hHook=NULL;
	HOOKPROC pfnHookProc;
	HWND hWnd=NULL;
	int result=1;

	g_quitMsg=RegisterWindowMessage(QUIT_MESSAGE_NAME);

	g_hDLL=LoadLibraryA(DLL_NAME);
	if(!g_hDLL)
	{
		DWORD err=GetLastError();
		char msg[500];

		FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM,NULL,err,0,msg,sizeof msg,NULL);

		MessageBoxA(NULL,msg,DLL_NAME,MB_OK|MB_ICONERROR);
		goto done;
	}

	pfnHookProc=(HOOKPROC)FindProc(HOOKNAME("KBSwitchCBTHookProc"));
	if(!pfnHookProc)
		goto done;

	g_pfnSetKeyboardLayout=(SetKeyboardLayoutFn)FindProc("SetKeyboardLayout");
	if(!g_pfnSetKeyboardLayout)
		goto done;

	hHook=SetWindowsHookEx(WH_CBT,pfnHookProc,g_hDLL,0);
	if(!hHook)
		goto done;

	hWnd=CreateWnd();

	if(hWnd)
	{
		for(;;)
		{
			int r;
			MSG msg;

			r=GetMessage(&msg,hWnd,0,0);
			if(r==0||r==-1)
				break;

			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		result=0;
	}

done:
	if(hWnd)
	{
		DestroyWindow(hWnd);
		hWnd=NULL;
	}

	if(hHook)
	{
		UnhookWindowsHookEx(hHook);
		hHook=NULL;
	}

	if(g_hDLL)
	{
		FreeLibrary(g_hDLL);
		g_hDLL=NULL;
	}

	ExitProcess(result);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
