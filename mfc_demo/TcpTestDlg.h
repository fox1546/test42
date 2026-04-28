#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <vector>
#include <thread>
#include <mutex>

#pragma comment(lib, "ws2_32.lib")

#define WM_SOCKET_MESSAGE (WM_USER + 1)

enum ConnectionMode
{
    MODE_NONE,
    MODE_SERVER,
    MODE_CLIENT
};

class CTcpTestDlg : public CDialogEx
{
public:
    CTcpTestDlg(CWnd* pParent = nullptr);

#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_TCPTEST_DIALOG };
#endif

protected:
    virtual void DoDataExchange(CDataExchange* pDX);

protected:
    HICON m_hIcon;

    virtual BOOL OnInitDialog();
    afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
    afx_msg void OnPaint();
    afx_msg HCURSOR OnQueryDragIcon();
    DECLARE_MESSAGE_MAP()

public:
    afx_msg void OnBnClickedBtnListen();
    afx_msg void OnBnClickedBtnConnect();
    afx_msg void OnBnClickedBtnDisconnect();
    afx_msg void OnBnClickedBtnSend();
    afx_msg void OnBnClickedBtnClearReceive();
    afx_msg void OnBnClickedBtnClearLog();
    afx_msg LRESULT OnSocketMessage(WPARAM wParam, LPARAM lParam);

private:
    void InitSocket();
    void CleanupSocket();
    void AcceptThreadProc();
    void ReceiveThreadProc();
    void Log(const CString& message);
    CString GetCurrentTime();
    void UpdateUIState();
    void SetControlsEnabled(BOOL bEnabled);
    
    BOOL IsTextMode();
    std::vector<BYTE> HexStringToBytes(const CString& hexStr);
    CString BytesToHexString(const std::vector<BYTE>& data);
    CString FormatReceiveData(const std::vector<BYTE>& data);

private:
    CEdit m_editPort;
    CEdit m_editIP;
    CEdit m_editRemotePort;
    CButton m_btnListen;
    CButton m_btnConnect;
    CButton m_btnDisconnect;
    CEdit m_editSend;
    CButton m_radioText;
    CButton m_radioHex;
    CButton m_btnSend;
    CEdit m_editReceive;
    CEdit m_editLog;
    CButton m_btnClearReceive;
    CButton m_btnClearLog;
    CStatic m_staticStatus;

    SOCKET m_listenSocket;
    SOCKET m_connectedSocket;
    ConnectionMode m_mode;
    BOOL m_bListening;
    BOOL m_bConnected;
    
    std::thread m_acceptThread;
    std::thread m_receiveThread;
    std::mutex m_socketMutex;
    BOOL m_bRunning;
};
