#include "framework.h"
#include "mfc_demo.h"
#include "TcpTestDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

BEGIN_MESSAGE_MAP(CTcpTestApp, CWinApp)
END_MESSAGE_MAP()

CTcpTestApp::CTcpTestApp()
{
    m_dwGdiVersion = 0x10;
}

CTcpTestApp theApp;

BOOL CTcpTestApp::InitInstance()
{
    CWinApp::InitInstance();

    SetRegistryKey(_T("Local AppWizard-Generated Applications"));

    CTcpTestDlg dlg;
    m_pMainWnd = &dlg;
    INT_PTR nResponse = dlg.DoModal();
    if (nResponse == IDOK)
    {
    }
    else if (nResponse == IDCANCEL)
    {
    }

    return FALSE;
}
