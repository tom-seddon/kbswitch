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
	LOG("%s: hkl=%p",__FUNCTION__,hKL);

	g_hKL=hKL;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void SetWindowKeyboardLayout(const char *pReason,HWND hWnd)
{
	LOG("%s: hwnd=%p hkl=%p.",pReason,hWnd,g_hKL);//,GetLayoutDisplayName(g_hKL));

 	if(g_hKL)
 		PostMessage(hWnd,WM_INPUTLANGCHANGEREQUEST,0,(LPARAM)g_hKL);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

__declspec(dllexport)
LRESULT CALLBACK KBSwitchCallWndProcRet(int nCode,WPARAM wParam,LPARAM lParam)
{
	if(nCode==HC_ACTION)
	{
		//BOOL sent_by_current_process=!!wParam;
		CWPRETSTRUCT *rs=(CWPRETSTRUCT *)lParam;

		if(rs->message==WM_CREATE)
			SetWindowKeyboardLayout("WM_CREATE ret",rs->hwnd);
	}

	return CallNextHookEx(NULL,nCode,wParam,lParam);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

__declspec(dllexport)
LRESULT CALLBACK KBSwitchCBTHookProc(int nCode,WPARAM wParam,LPARAM lParam)
{
	switch(nCode)
	{
		// HCBT_CREATEWND isn't great, because every UI element is a window, and
        // so the hook gets called a lot.
// 	case HCBT_CREATEWND:
// 		{
// 			SetWindowKeyboardLayout("HCBT_CREATEWND",(HWND)wParam);
// 		}
// 		break;

	case HCBT_SETFOCUS:
		{
			SetWindowKeyboardLayout("HCBT_SETFOCUS",(HWND)wParam);
		}
		break;
	}

	return CallNextHookEx(NULL,nCode,wParam,lParam);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
