#include "../common.inl"

#include <shlwapi.h>
#include <shellapi.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const char MUTEX_NAME[]="kbswitch2_mutex";

static const TCHAR WND_CLASS_NAME[]=_T("kbswitch2_wnd");

// Got bored of dynamically allocating everything...
enum {LOCALIZED_STR_SIZE=100};

enum {NOTIFY_MSG=WM_APP+1};
enum {NOTIFY_ID=1};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// http://blogs.msdn.com/b/michkap/archive/2006/05/06/591174.aspx -- :(
static const char g_aRegKeyNameRoot[]="SYSTEM\\CurrentControlSet\\Control\\Keyboard Layouts\\";
enum {REG_KEY_NAME_ROOT_LEN=sizeof g_aRegKeyNameRoot/sizeof g_aRegKeyNameRoot[0]-1};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct Layout
{
	struct Layout *pNextLayout;

	HKL hkl;
	TCHAR *pDisplayName;
};
typedef struct Layout Layout;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static TCHAR *GetRegString(HKEY hRootKey,const char *pKeyName,const wchar_t *pValueName)
{
	HKEY hKey;
	DWORD valueType,valueSizeBytes;
	int good=0;
	TCHAR *pValueValue=0;

	if(RegOpenKeyExA(hRootKey,pKeyName,0,KEY_READ,&hKey)!=ERROR_SUCCESS)
		goto bye;

	if(RegQueryValueEx(hKey,pValueName,0,&valueType,0,&valueSizeBytes)!=ERROR_SUCCESS)
		goto bye;

	if(valueType!=REG_SZ)
		goto bye;

	// Add one for the '\x0', if it wasn't there already, and make it
	// at least LOCALIZED_STR_SIZE too.
	valueSizeBytes+=sizeof(TCHAR);
	valueSizeBytes=min(valueSizeBytes,LOCALIZED_STR_SIZE*sizeof(TCHAR));

	pValueValue=LocalAlloc(0,valueSizeBytes);
	if(!pValueValue)
		goto bye;

	if(RegQueryValueEx(hKey,pValueName,0,&valueType,(BYTE *)pValueValue,
		&valueSizeBytes)!=ERROR_SUCCESS)
	{
		goto bye;
	}

	good=1;

bye:
	if(hKey)
		RegCloseKey(hKey);

	if(!good)
	{
		LocalFree(pValueValue);
		pValueValue=0;
	}

	return pValueValue;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static TCHAR *GetLayoutDisplayName(HKL hkl)
{
	TCHAR *pDisplayName;
	char aRegKeyName[REG_KEY_NAME_ROOT_LEN+KL_NAMELENGTH];

	// Activate keyboard layout to get its name, forming the registry key name.
	{
		HKL hklOld=GetKeyboardLayout(0);

		if(!ActivateKeyboardLayout(hkl,0))
			return 0;

		lstrcpyA(aRegKeyName,g_aRegKeyNameRoot);

		if(!GetKeyboardLayoutNameA(aRegKeyName+REG_KEY_NAME_ROOT_LEN))
			return 0;

		if(!ActivateKeyboardLayout(hklOld,0))
			return 0;
	}

	pDisplayName=GetRegString(HKEY_LOCAL_MACHINE,aRegKeyName,_T("Layout Display Name"));
	if(!pDisplayName)
		return 0;

	if(pDisplayName[0]==_T('@'))
	{
		// With any luck, SHLoadIndirectString will just leave the input alone
		// if it fails...
		SHLoadIndirectString(pDisplayName,pDisplayName,LOCALIZED_STR_SIZE,0);
	}

	return pDisplayName;
}

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

static void NotifyCtl(DWORD action,const TCHAR *pTip)
{
	NOTIFYICONDATA nid;

	nid.cbSize=sizeof nid;
	nid.hWnd=g_hWnd;
	nid.uID=NOTIFY_ID;
	nid.uFlags=NIF_ICON|NIF_MESSAGE;
	nid.uCallbackMessage=NOTIFY_MSG;
	nid.hIcon=LoadIcon(NULL,IDI_APPLICATION);//GetModuleHandle(0),MAKEINTRESOURCE(IDI_ICON1));

	if(pTip)
	{
		nid.uFlags|=NIF_TIP;
		lstrcpyn(nid.szTip,pTip,sizeof nid.szTip/sizeof nid.szTip[0]);
	}

	Shell_NotifyIcon(action,&nid);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void UpdateToolTip(void)
{
	Layout *pFirstLayout=CreateLayoutsList();
	Layout *pActiveLayout=FindActiveLayout(pFirstLayout);
	TCHAR *pDisplayName;

	pActiveLayout=FindActiveLayout(pFirstLayout);

	if(!pActiveLayout)
		pDisplayName=_T("?");
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

				AppendMenu(hMenu,flags,id++,pLayout->pDisplayName);
			}

			AppendMenu(hMenu,MF_SEPARATOR,0,0);
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

		SystemParametersInfo(SPI_SETDEFAULTINPUTLANG,0,&pLayout->hkl,0);
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
	WNDCLASSEX w;

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
		return 0;

	g_hWnd=CreateWindow(WND_CLASS_NAME,_T("kbswitch"),WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,0,0,
		GetModuleHandle(0),0);
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

	NotifyCtl(NIM_ADD,_T(""));

	UpdateToolTip();

	for(;;)
	{
		int r;
		MSG msg;

		r=GetMessage(&msg,g_hWnd,0,0);
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

static BOOL RunHelper(const TCHAR *pEXEName)
{
	TCHAR *pFileName;
	BOOL good;
	STARTUPINFO si;
	PROCESS_INFORMATION pi;

	{
		DWORD moduleFileNameSize=32768;
		TCHAR *pModuleFileName=LocalAlloc(0,moduleFileNameSize*sizeof *pModuleFileName);
		TCHAR *p;
		TCHAR *pLastSep=pModuleFileName;
		DWORD fileNameSize;

		GetModuleFileName(NULL,pModuleFileName,moduleFileNameSize);
		pModuleFileName[moduleFileNameSize-1]=0;

		for(p=pModuleFileName;*p!=0;p=CharNext(p))
		{
			if(*p==_T('\\')||*p==_T('/'))
				pLastSep=p;
		}

		*pLastSep=0;

		fileNameSize=lstrlen(pModuleFileName)+1+lstrlen(pEXEName)+1;
		pFileName=LocalAlloc(0,fileNameSize*sizeof pFileName);

		lstrcpy(pFileName,pModuleFileName);
		lstrcat(pFileName,_T("\\"));
		lstrcat(pFileName,pEXEName);

		LocalFree(pModuleFileName);
		pModuleFileName=NULL;
	}

	ResetMem(&si,sizeof si);
	si.cb=sizeof si;

	good=CreateProcess(pFileName,NULL,NULL,NULL,TRUE,0,NULL,NULL,&si,&pi);
	if(!good)
	{
		DWORD err=GetLastError();
		TCHAR msg[500];

		FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,NULL,err,0,msg,sizeof msg/sizeof msg[0],NULL);

		MessageBox(NULL,msg,pFileName,MB_OK|MB_ICONERROR);
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
		if(!RunHelper(_T("kbswitch2_helper_x64") _T(BUILD_SUFFIX) _T(".exe")))
			return FALSE;
	}

	if(!RunHelper(_T("kbswitch2_helper_x86") _T(BUILD_SUFFIX) _T(".exe")))
		return FALSE;

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Entry(void)
{
	UINT quitMsg=RegisterWindowMessage(QUIT_MESSAGE_NAME);
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
