#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlwapi.h>
#include <shellapi.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#pragma warning(error:4024)//different types for formal and actual parameter 

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#ifdef _DEBUG
#define BUILD_SUFFIX "d"
#else
#define BUILD_SUFFIX ""
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#ifdef _M_X64
#define PLATFORM_SUFFIX "x64"
#else
#define PLATFORM_SUFFIX "x86"
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define W2(X) L##X
#define W(X) W2(X)

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const char CONTROL_MESSAGE_NAME[]="KBSWITCH_CONTROL_MESSAGE";

// Values for wParam.
enum {
	CONTROL_QUIT=0,//lParam ignored.
	CONTROL_NEXT_LAYOUT=1,//lParam ignored.
	CONTROL_PREV_LAYOUT=2,//lParam ignored.
	CONTROL_SET_LAYOUT=3,//lParam is HKL.
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void Log(const char *pSrcFileName,int line,const char *pFmt,...)
{
	static char msg[16384];

	char *p=msg;
	HMODULE hModule;
	char moduleFileName[500],*pModuleName;
	DWORD pid;
	va_list v;
	int i;

#pragma warning(push)
#pragma warning(disable:4054)//'conversion' : from function pointer 'type1' to data pointer 'type2'
	GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,(LPCSTR)(void *)&Log,&hModule);
#pragma warning(pop)

	GetModuleFileNameA(hModule,moduleFileName,sizeof moduleFileName);
	moduleFileName[sizeof moduleFileName-1]=0;

	pModuleName=moduleFileName;

	for(i=0;moduleFileName[i]!=0;++i)
	{
		if(moduleFileName[i]=='\\'||moduleFileName[i]=='/')
			pModuleName=&moduleFileName[i]+1;
	}
	
	pid=GetCurrentProcessId();

	p+=wsprintfA(p,"%s(%d): \"%s\" (%lu): ",pSrcFileName,line,pModuleName,pid);

	va_start(v,pFmt);
	p+=wvsprintfA(p,pFmt,v);
	va_end(v);

	OutputDebugStringA(msg);
}

#define LOG(...) (Log(__FILE__,__LINE__,__VA_ARGS__))

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// http://blogs.msdn.com/b/michkap/archive/2006/05/06/591174.aspx -- :(
static const char g_aRegKeyNameRoot[]="SYSTEM\\CurrentControlSet\\Control\\Keyboard Layouts\\";
enum {REG_KEY_NAME_ROOT_LEN=sizeof g_aRegKeyNameRoot-1};

enum {
	LOCALIZED_STR_SIZE=100,
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static wchar_t *GetRegString(HKEY hRootKey,const char *pKeyName,const wchar_t *pValueName)
{
	HKEY hKey;
	DWORD valueType,valueSizeBytes;
	int good=0;
	wchar_t *pValueValue=0;

	if(RegOpenKeyExA(hRootKey,pKeyName,0,KEY_READ,&hKey)!=ERROR_SUCCESS)
		goto bye;

	if(RegQueryValueExW(hKey,pValueName,0,&valueType,0,&valueSizeBytes)!=ERROR_SUCCESS)
		goto bye;

	if(valueType!=REG_SZ)
		goto bye;

	// Add one for the '\x0', if it wasn't there already, and make it
	// at least LOCALIZED_STR_SIZE too.
	valueSizeBytes+=sizeof(wchar_t);
	valueSizeBytes=min(valueSizeBytes,LOCALIZED_STR_SIZE*sizeof(wchar_t));

	pValueValue=LocalAlloc(0,valueSizeBytes);
	if(!pValueValue)
		goto bye;

	if(RegQueryValueExW(hKey,pValueName,0,&valueType,(BYTE *)pValueValue,&valueSizeBytes)!=ERROR_SUCCESS)
		goto bye;

	good=1;

bye:
	if(hKey)
		RegCloseKey(hKey);

	if(!good)
	{
		LocalFree(pValueValue);
		pValueValue=NULL;
	}

	return pValueValue;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static wchar_t *GetLayoutDisplayName(HKL hkl)
{
	wchar_t *pDisplayName;
	char aRegKeyName[REG_KEY_NAME_ROOT_LEN+KL_NAMELENGTH];

	// Activate keyboard layout to get its name, forming the registry key name.
	{
		HKL hklOld=GetKeyboardLayout(0);

		if(!ActivateKeyboardLayout(hkl,0))
			return NULL;

		lstrcpyA(aRegKeyName,g_aRegKeyNameRoot);

		if(!GetKeyboardLayoutNameA(aRegKeyName+REG_KEY_NAME_ROOT_LEN))
			return NULL;

		if(!ActivateKeyboardLayout(hklOld,0))
			return NULL;
	}

	pDisplayName=GetRegString(HKEY_LOCAL_MACHINE,aRegKeyName,L"Layout Display Name");
	if(!pDisplayName)
		return NULL;

	if(pDisplayName[0]==L'@')
	{
		// With any luck, SHLoadIndirectString will just leave the input alone
		// if it fails...
		SHLoadIndirectString(pDisplayName,pDisplayName,LOCALIZED_STR_SIZE,0);
	}

	return pDisplayName;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
