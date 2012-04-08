#include "../common.inl"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// http://www.codeproject.com/KB/DLL/data_seg_share.aspx

#pragma bss_seg(".shared")
HKL g_hKL;
#pragma bss_seg()

#pragma comment(linker,"/section:.shared,RWS")

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Each application gets its own list of windows.
static CRITICAL_SECTION g_cs;
static size_t g_maxNumHWNDs;
static size_t g_numHWNDs;
static HWND *g_pHWNDs;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BOOL WINAPI DllMain(HANDLE hDLL,DWORD fdwReason,LPVOID lpvReserved)
{
	(void)hDLL,(void)fdwReason,(void)lpvReserved;

	switch(fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		{
			InitializeCriticalSection(&g_cs);
		}
		break;

	case DLL_PROCESS_DETACH:
		{
			DeleteCriticalSection(&g_cs);
		}
		break;
	}

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static BOOL FindWindowIndex(HWND hWnd,size_t *pIndex)
{
	size_t i;

	// Number of windows likely to be small.
	for(i=0;i<g_numHWNDs;++i)
	{
		if(g_pHWNDs[i]==hWnd)
		{
			if(pIndex)
				*pIndex=i;

			return TRUE;
		}
	}

	return FALSE;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static BOOL IsNewWindow(HWND hWnd)
{
	BOOL isNewWindow=FALSE;

	EnterCriticalSection(&g_cs);

	if(!FindWindowIndex(hWnd,NULL))
	{
		if(g_numHWNDs==g_maxNumHWNDs)
		{
			if(g_maxNumHWNDs==0)
			{
				g_maxNumHWNDs=16;
				g_pHWNDs=LocalAlloc(0,g_maxNumHWNDs*sizeof *g_pHWNDs);
			}
			else
			{
				g_maxNumHWNDs+=g_maxNumHWNDs/2;
				g_pHWNDs=LocalReAlloc(g_pHWNDs,g_maxNumHWNDs*sizeof *g_pHWNDs,0);
			}
		}

		g_pHWNDs[g_numHWNDs++]=hWnd;
	}

	LeaveCriticalSection(&g_cs);

	return isNewWindow;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void ForgetWindow(HWND hWnd)
{
	size_t i;

	EnterCriticalSection(&g_cs);

	if(FindWindowIndex(hWnd,&i))
		g_pHWNDs[i]=g_pHWNDs[--g_numHWNDs];

	LeaveCriticalSection(&g_cs);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

__declspec(dllexport)
void SetKeyboardLayout(HKL hKL)
{
	LOG("%s: hkl=%p",__FUNCTION__,hKL);

	g_hKL=hKL;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void SetWindowKeyboardLayout(const char *pReason,HWND hWnd)
{
 	if(!g_hKL)
	{
		LOG("%s: (HWND %p): no HKL set.",pReason,hWnd);
	}
	else
	{
		if(!IsNewWindow(hWnd))
		{
			LOG("%s: (HWND %p): (HKL)%p, sending WM_INPUTLANGCHANGEREQUEST to new window.",pReason,hWnd,g_hKL);
 			PostMessage(hWnd,WM_INPUTLANGCHANGEREQUEST,0,(LPARAM)g_hKL);
		}
		else
		{
			LOG("%s: (HWND %p): (HKL)%p, not sending WM_INPUTLANGCHANGEREQUEST to existing window.",pReason,hWnd,g_hKL);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

__declspec(dllexport)
LRESULT CALLBACK KBSwitchCBTHookProc(int nCode,WPARAM wParam,LPARAM lParam)
{
	switch(nCode)
	{
	case HCBT_SETFOCUS:
		{
			SetWindowKeyboardLayout("HCBT_SETFOCUS",(HWND)wParam);
		}
		break;

	case HCBT_DESTROYWND:
		{
			ForgetWindow((HWND)wParam);
		}
		break;
	}

	return CallNextHookEx(NULL,nCode,wParam,lParam);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
