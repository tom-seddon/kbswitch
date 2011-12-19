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

BOOL WINAPI DllMain(HANDLE hDLL,DWORD fdwReason,LPVOID lpvReserved)
{
	(void)hDLL,(void)fdwReason,(void)lpvReserved;

	return TRUE;
}


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

__declspec(dllexport)
void SetKeyboardLayout(HKL hKL)
{
	LOG("%s: HKL=%p\n",__FUNCTION__,hKL);

	g_hKL=hKL;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// __declspec(dllexport)
// LRESULT CALLBACK KBSwitchCallWndProcRet(int nCode,WPARAM wParam,LPARAM lParam)
// {
// 	if(nCode==HC_ACTION)
// 	{
// 		BOOL sent_by_current_process=!!wParam;
// 		CWPRETSTRUCT *rs=(CWPRETSTRUCT *)lParam;
// 
// 		if(rs->message==WM_CREATE)
// 			dprintf("%s: HC_ACTION: WM_CREATE: lResult=%ld lParam=0x%X wParam=0x%X message=%u hWnd=0x%X\n",__FUNCTION__,rs->lResult,rs->lParam,rs->wParam,rs->message,rs->hwnd);
// 	}
// 
// 	return CallNextHookEx(NULL,nCode,wParam,lParam);
// }

__declspec(dllexport)
LRESULT CALLBACK KBSwitchCBTHookProc(int nCode,WPARAM wParam,LPARAM lParam)
{
	switch(nCode)
	{
	case HCBT_CREATEWND:
		{
			HWND hWnd=(HWND)wParam;
			CBT_CREATEWND *cw=(CBT_CREATEWND *)lParam;

			LOG("HCBT_CREATEWND: HWND=%p hKL=%p.\n",__FUNCTION__,hWnd,g_hKL);

			if(g_hKL)
			{
				dprintf("    (posting message...)\n");
				PostMessage(hWnd,WM_INPUTLANGCHANGEREQUEST,0,(LPARAM)g_hKL);
			}
		}
		break;

// 	case HCBT_SETFOCUS:
// 		{
// 			HWND hNewWnd=(HWND)wParam;
// 			HWND hOldWnd=(HWND)lParam;
// 			char title[100];
// 
// 			GetWindowTextA(hNewWnd,title,sizeof title);
// 
// 			dprintf("%s: HCBT_SETFOCUS: hNewWnd=%p, title=\"%s\".\n",__FUNCTION__,hNewWnd,title);
// 		}
// 		break;

// 	default:
// 		if(nCode>=0)
// 		{
// 			dprintf("%s: %d: wParam=0x%X lParam=0x%X\n",__FUNCTION__,nCode,wParam,lParam);
// 		}
// 		break;
	}

	return CallNextHookEx(NULL,nCode,wParam,lParam);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
