#include "../common.inl"

#include "resource.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const char MUTEX_NAME[]="kbswitch2_mutex";

static const char WND_CLASS_NAME[]="kbswitch2_wnd";

enum {NOTIFY_MSG=WM_APP+1};
enum {NOTIFY_ID=1};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct Layout
{
	struct Layout *pNextLayout;

	HKL hkl;
	wchar_t *pDisplayName;
};
typedef struct Layout Layout;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static Layout *FindActiveLayout(Layout *pFirstLayout)
{
	Layout *pLayout;
	HKL hkl=GetKeyboardLayout(0);

	for(pLayout=pFirstLayout;pLayout;pLayout=pLayout->pNextLayout)
	{
		if(pLayout->hkl==hkl)
			return pLayout;
	}

	return NULL;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void DeleteLayoutList(Layout *pFirstLayout)
{
	Layout *pLayout=pFirstLayout;

	while(pLayout)
	{
		Layout *pNextLayout=pLayout->pNextLayout;

		LocalFree(pLayout->pDisplayName);
		LocalFree(pLayout);

		pLayout=pNextLayout;
	}
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static Layout *CreateLayoutsList(void)
{
	BOOL good=FALSE;
	int i;
	Layout *pFirstLayout=NULL,**ppNextLayout=&pFirstLayout;
	int numHKLs;
	HKL *pHKLs=NULL;

	numHKLs=GetKeyboardLayoutList(0,NULL);
	if(numHKLs==0)
		goto bye;

	pHKLs=LocalAlloc(0,numHKLs*sizeof *pHKLs);
	if(!pHKLs)
		goto bye;

	if(GetKeyboardLayoutList(numHKLs,pHKLs)!=numHKLs)
		goto bye;

	for(i=0;i<numHKLs;++i)
	{
		Layout entry,*pEntry;

		entry.hkl=pHKLs[i];

		entry.pDisplayName=GetLayoutDisplayName(entry.hkl);
		if(!entry.pDisplayName)
			continue;

		pEntry=LocalAlloc(0,sizeof *pEntry);
		if(!pEntry)
			goto bye;

		*pEntry=entry;

		*ppNextLayout=pEntry;
		ppNextLayout=&pEntry->pNextLayout;
		*ppNextLayout=NULL;
	}

	good=TRUE;

bye:
	if(!good)
	{
		DeleteLayoutList(pFirstLayout);
		pFirstLayout=NULL;
	}

	return pFirstLayout;
}

// 	dprintf(_T("%d layouts:\n"),pList->numHKLs);
// 	for(i=0;i<pList->numHKLs;++i)
// 	{
// 		// 		TCHAR aLayoutName[LOCALIZED_STR_SIZE];
// 		// 		TCHAR aCountryName[LOCALIZED_STR_SIZE];
// 		// 
// 		// 		if(!GetCountryName(g_pHKLs[i],aCountryName))
// 		// 			return 0;
// 
// 		pList->ppDisplayNames[i]=GetLayoutDisplayName(pList->pHKLs[i]);
// 		if(!pList->ppDisplayNames[i])
// 			goto bye;
// 
// 		dprintf(_T("    %u. HKL=0x%08X: \"%s\".\n"),i,pList->pHKLs[i],pList->ppDisplayNames[i]);
// 	}
// 
// 	good=1;
// 
// bye:
// 	if(!good)
// 	{
// 		DeleteLayoutsList(pList);
// 		pList=0;
// 	}
// 
// 	return pList;
// }

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static HWND g_hWnd;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void NotifyCtl(DWORD action,const wchar_t *pTip)
{
	NOTIFYICONDATAW nid;

	nid.cbSize=sizeof nid;
	nid.hWnd=g_hWnd;
	nid.uID=NOTIFY_ID;
	nid.uFlags=NIF_ICON|NIF_MESSAGE;
	nid.uCallbackMessage=NOTIFY_MSG;
	nid.hIcon=LoadIcon(GetModuleHandle(NULL),MAKEINTRESOURCE(IDI_ICON1));

	if(pTip)
	{
		enum {
			MAX_TIP_SIZE=sizeof nid.szTip/sizeof nid.szTip[0],
		};

		nid.uFlags|=NIF_TIP;

		lstrcpynW(nid.szTip,pTip,MAX_TIP_SIZE);
		nid.szTip[MAX_TIP_SIZE-1]=0;
	}

	Shell_NotifyIconW(action,&nid);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void UpdateToolTip(void)
{
	Layout *pFirstLayout=CreateLayoutsList();
	Layout *pActiveLayout=FindActiveLayout(pFirstLayout);
	wchar_t *pDisplayName;

	pActiveLayout=FindActiveLayout(pFirstLayout);

	if(!pActiveLayout)
		pDisplayName=L"?";
	else
		pDisplayName=pActiveLayout->pDisplayName;

	NotifyCtl(NIM_MODIFY,pDisplayName);

	DeleteLayoutList(pFirstLayout);
	pFirstLayout=NULL;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void DoPopupMenu(void)
{
	int idLayoutsBegin,idLayoutsEnd,idExit,cmd;
	POINT mpt;
	HMENU hMenu=CreatePopupMenu();
	Layout *pFirstLayout=CreateLayoutsList();

	SetForegroundWindow(g_hWnd);

	// Fill menu
	{
		int id=1;

		idLayoutsBegin=id;
		if(pFirstLayout)
		{
			Layout *pActiveLayout=FindActiveLayout(pFirstLayout),*pLayout;

			for(pLayout=pFirstLayout;pLayout;pLayout=pLayout->pNextLayout)
			{
				UINT flags=0;

				if(pLayout==pActiveLayout)
					flags|=MF_CHECKED;

				AppendMenuW(hMenu,flags,id++,pLayout->pDisplayName);
			}

			AppendMenuW(hMenu,MF_SEPARATOR,0,0);
		}
		idLayoutsEnd=id;

		idExit=id++;
		AppendMenuA(hMenu,0,idExit,"E&xit");
	}

	// Popup menu
	GetCursorPos(&mpt);
	cmd=TrackPopupMenuEx(hMenu,TPM_RETURNCMD,mpt.x,mpt.y,g_hWnd,0);

	// Do what?
	if(cmd==idExit)
	{
		// Bye...
		PostQuitMessage(0);
	}
	else if(cmd>=idLayoutsBegin&&cmd<idLayoutsEnd)
	{
		// Set new layout
		int idx=cmd-idLayoutsBegin;
		Layout *pLayout=pFirstLayout;

		while(idx-->0)
			pLayout=pLayout->pNextLayout;

		//SystemParametersInfo(SPI_SETDEFAULTINPUTLANG,0,&pLayout->hkl,0);
		PostMessage(HWND_BROADCAST,WM_INPUTLANGCHANGEREQUEST,0,(LPARAM)pLayout->hkl);
	}

	DeleteLayoutList(pFirstLayout);
	pFirstLayout=NULL;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static LRESULT CALLBACK WndProc(HWND hWnd,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
	switch(uMsg)
	{
	case WM_INPUTLANGCHANGEREQUEST:
		{
			UpdateToolTip();
		}
		break;

	case WM_RBUTTONUP:
		// avoid injecting the DLL into explorer...
		DoPopupMenu();
		return 0;

	case NOTIFY_MSG:
		{
			switch(lParam)
			{
			case WM_RBUTTONDOWN:
				DoPopupMenu();
				return 0;
			}
		}
		break;

	case WM_CLOSE:
		PostQuitMessage(0);
		return 0;
	}

	return DefWindowProc(hWnd,uMsg,wParam,lParam);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static BOOL CreateWnd(void)
{
	WNDCLASSEXA w;

	w.cbClsExtra=0;
	w.cbSize=sizeof w;
	w.cbWndExtra=0;
	w.hbrBackground=0;//GetStockObject(NULL_BRUSH);
	w.hCursor=LoadCursor(0,IDC_ARROW);
	w.hIcon=LoadIcon(GetModuleHandle(NULL),MAKEINTRESOURCE(IDI_ICON1));
	w.hIconSm=LoadIcon(GetModuleHandle(NULL),MAKEINTRESOURCE(IDI_ICON1));
	w.hInstance=GetModuleHandle(0);
	w.lpfnWndProc=&WndProc;
	w.lpszClassName=WND_CLASS_NAME;
	w.lpszMenuName=0;
	w.style=CS_HREDRAW|CS_VREDRAW;

	if(!RegisterClassExA(&w))
		return 0;

	g_hWnd=CreateWindowA(WND_CLASS_NAME,"kbswitch",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,0,0,GetModuleHandle(0),0);
	if(!g_hWnd)
		return FALSE;

#ifdef _DEBUG
	ShowWindow(g_hWnd,SW_SHOW);
#endif//_DEBUG

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static int Main(void)
{
	if(!CreateWnd())
		return 1;

	NotifyCtl(NIM_ADD,L"");

	UpdateToolTip();

	for(;;)
	{
		int r;
		MSG msg;

		r=GetMessage(&msg,NULL,0,0);
		if(r==0||r==-1)
			break;

		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	NotifyCtl(NIM_DELETE,NULL);

	return 0;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const STARTUPINFOW SI_ZERO={0,};

static BOOL RunHelper(const wchar_t *pEXEName)
{
	wchar_t *pFileName;
	BOOL good;
	STARTUPINFOW si;
	PROCESS_INFORMATION pi;

	{
		DWORD moduleFileNameSize=32768;
		wchar_t *pModuleFileName=LocalAlloc(0,moduleFileNameSize*sizeof *pModuleFileName);
		wchar_t *p;
		wchar_t *pLastSep=pModuleFileName;
		DWORD fileNameSize;

		GetModuleFileNameW(NULL,pModuleFileName,moduleFileNameSize);
		pModuleFileName[moduleFileNameSize-1]=0;

		for(p=pModuleFileName;*p!=0;p=CharNextW(p))
		{
			if(*p==L'\\'||*p==L'/')
				pLastSep=p;
		}

		*pLastSep=0;

		fileNameSize=lstrlenW(pModuleFileName)+1+lstrlenW(pEXEName)+1;
		pFileName=LocalAlloc(0,fileNameSize*sizeof pFileName);

		lstrcpyW(pFileName,pModuleFileName);
		lstrcatW(pFileName,L"\\");
		lstrcatW(pFileName,pEXEName);

		LocalFree(pModuleFileName);
		pModuleFileName=NULL;
	}

	si=SI_ZERO;
	si.cb=sizeof si;

	good=CreateProcessW(pFileName,NULL,NULL,NULL,TRUE,0,NULL,NULL,&si,&pi);
	if(!good)
	{
		DWORD err=GetLastError();
		wchar_t msg[500];

		FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM,NULL,err,0,msg,sizeof msg/sizeof msg[0],NULL);

		MessageBoxW(NULL,msg,pFileName,MB_OK|MB_ICONERROR);
	}

	LocalFree(pFileName);
	pFileName=NULL;

	return good;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static BOOL RunHelpers(void)
{
	BOOL isWOW64;

	if(IsWow64Process(GetCurrentProcess(),&isWOW64)&&isWOW64)
	{
		if(!RunHelper(L"kbswitch2_helper_x64" W(BUILD_SUFFIX) L".exe"))
			return FALSE;
	}

	if(!RunHelper(L"kbswitch2_helper_x86" W(BUILD_SUFFIX) L".exe"))
		return FALSE;

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Entry(void)
{
	UINT quitMsg=RegisterWindowMessageA(QUIT_MESSAGE_NAME);
	HANDLE hMutex;
	int result=1;

	hMutex=OpenMutexA(MUTEX_ALL_ACCESS,FALSE,MUTEX_NAME);
	if(!hMutex)
	{
		hMutex=CreateMutexA(0,FALSE,MUTEX_NAME);

		if(!RunHelpers())
			result=1;
		else
		{
			result=Main();

			ReleaseMutex(hMutex);
			hMutex=NULL;
		}
	}

	PostMessage(HWND_BROADCAST,quitMsg,0,0);

	ExitProcess(result);
}
