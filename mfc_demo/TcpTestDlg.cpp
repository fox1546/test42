#include "framework.h"
#include "TcpTestDlg.h"
#include "Resource.h"
#include "afxdialogex.h"
#include <sstream>
#include <iomanip>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

CTcpTestDlg::CTcpTestDlg(CWnd* pParent /*=nullptr*/)
    : CDialogEx(IDD_TCPTEST_DIALOG, pParent)
    , m_listenSocket(INVALID_SOCKET)
    , m_connectedSocket(INVALID_SOCKET)
    , m_mode(MODE_NONE)
    , m_bListening(FALSE)
    , m_bConnected(FALSE)
    , m_bRunning(FALSE)
{
    m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CTcpTestDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_EDIT_PORT, m_editPort);
    DDX_Control(pDX, IDC_EDIT_IP, m_editIP);
    DDX_Control(pDX, IDC_EDIT_REMOTE_PORT, m_editRemotePort);
    DDX_Control(pDX, IDC_BTN_LISTEN, m_btnListen);
    DDX_Control(pDX, IDC_BTN_CONNECT, m_btnConnect);
    DDX_Control(pDX, IDC_BTN_DISCONNECT, m_btnDisconnect);
    DDX_Control(pDX, IDC_EDIT_SEND, m_editSend);
    DDX_Control(pDX, IDC_RADIO_TEXT, m_radioText);
    DDX_Control(pDX, IDC_RADIO_HEX, m_radioHex);
    DDX_Control(pDX, IDC_BTN_SEND, m_btnSend);
    DDX_Control(pDX, IDC_EDIT_RECEIVE, m_editReceive);
    DDX_Control(pDX, IDC_EDIT_LOG, m_editLog);
    DDX_Control(pDX, IDC_BTN_CLEAR_RECEIVE, m_btnClearReceive);
    DDX_Control(pDX, IDC_BTN_CLEAR_LOG, m_btnClearLog);
    DDX_Control(pDX, IDC_STATIC_STATUS, m_staticStatus);
}

BEGIN_MESSAGE_MAP(CTcpTestDlg, CDialogEx)
    ON_WM_SYSCOMMAND()
    ON_WM_PAINT()
    ON_WM_QUERYDRAGICON()
    ON_BN_CLICKED(IDC_BTN_LISTEN, &CTcpTestDlg::OnBnClickedBtnListen)
    ON_BN_CLICKED(IDC_BTN_CONNECT, &CTcpTestDlg::OnBnClickedBtnConnect)
    ON_BN_CLICKED(IDC_BTN_DISCONNECT, &CTcpTestDlg::OnBnClickedBtnDisconnect)
    ON_BN_CLICKED(IDC_BTN_SEND, &CTcpTestDlg::OnBnClickedBtnSend)
    ON_BN_CLICKED(IDC_BTN_CLEAR_RECEIVE, &CTcpTestDlg::OnBnClickedBtnClearReceive)
    ON_BN_CLICKED(IDC_BTN_CLEAR_LOG, &CTcpTestDlg::OnBnClickedBtnClearLog)
    ON_MESSAGE(WM_SOCKET_MESSAGE, &CTcpTestDlg::OnSocketMessage)
END_MESSAGE_MAP()

BOOL CTcpTestDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
    ASSERT(IDM_ABOUTBOX < 0xF000);

    CMenu* pSysMenu = GetSystemMenu(FALSE);
    if (pSysMenu != nullptr)
    {
        BOOL bNameValid;
        CString strAboutMenu;
        bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
        ASSERT(bNameValid);
        if (!strAboutMenu.IsEmpty())
        {
            pSysMenu->AppendMenu(MF_SEPARATOR);
            pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
        }
    }

    SetIcon(m_hIcon, TRUE);
    SetIcon(m_hIcon, FALSE);

    m_editPort.SetWindowText(_T("8888"));
    m_editIP.SetWindowText(_T("127.0.0.1"));
    m_editRemotePort.SetWindowText(_T("8888"));
    m_radioText.SetCheck(BST_CHECKED);
    
    UpdateUIState();
    InitSocket();

    return TRUE;
}

void CTcpTestDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
    if ((nID & 0xFFF0) == IDM_ABOUTBOX)
    {
        CAboutDlg dlgAbout;
        dlgAbout.DoModal();
    }
    else
    {
        CDialogEx::OnSysCommand(nID, lParam);
    }
}

void CTcpTestDlg::OnPaint()
{
    if (IsIconic())
    {
        CPaintDC dc(this);

        SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

        int cxIcon = GetSystemMetrics(SM_CXICON);
        int cyIcon = GetSystemMetrics(SM_CYICON);
        CRect rect;
        GetClientRect(&rect);
        int x = (rect.Width() - cxIcon + 1) / 2;
        int y = (rect.Height() - cyIcon + 1) / 2;

        dc.DrawIcon(x, y, m_hIcon);
    }
    else
    {
        CDialogEx::OnPaint();
    }
}

HCURSOR CTcpTestDlg::OnQueryDragIcon()
{
    return static_cast<HCURSOR>(m_hIcon);
}

void CTcpTestDlg::InitSocket()
{
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        Log(_T("WSAStartup failed"));
        return;
    }

    if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
    {
        Log(_T("Winsock version not supported"));
        WSACleanup();
        return;
    }

    Log(_T("Winsock initialized successfully"));
}

void CTcpTestDlg::CleanupSocket()
{
    m_bRunning = FALSE;

    if (m_acceptThread.joinable())
    {
        m_acceptThread.join();
    }

    if (m_receiveThread.joinable())
    {
        m_receiveThread.join();
    }

    std::lock_guard<std::mutex> lock(m_socketMutex);

    if (m_connectedSocket != INVALID_SOCKET)
    {
        closesocket(m_connectedSocket);
        m_connectedSocket = INVALID_SOCKET;
    }

    if (m_listenSocket != INVALID_SOCKET)
    {
        closesocket(m_listenSocket);
        m_listenSocket = INVALID_SOCKET;
    }

    WSACleanup();
    Log(_T("Winsock cleanup completed"));
}

void CTcpTestDlg::OnBnClickedBtnListen()
{
    if (m_bListening)
    {
        Log(_T("Stopping listening..."));
        m_bRunning = FALSE;
        
        if (m_listenSocket != INVALID_SOCKET)
        {
            closesocket(m_listenSocket);
            m_listenSocket = INVALID_SOCKET;
        }

        if (m_acceptThread.joinable())
        {
            m_acceptThread.join();
        }

        m_bListening = FALSE;
        m_mode = MODE_NONE;
        Log(_T("Listening stopped"));
        UpdateUIState();
        return;
    }

    CString strPort;
    m_editPort.GetWindowText(strPort);
    int nPort = _ttoi(strPort);

    if (nPort <= 0 || nPort > 65535)
    {
        Log(_T("Please enter a valid port (1-65535)"));
        return;
    }

    m_listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_listenSocket == INVALID_SOCKET)
    {
        Log(_T("Failed to create listen socket"));
        return;
    }

    sockaddr_in service;
    service.sin_family = AF_INET;
    service.sin_addr.s_addr = INADDR_ANY;
    service.sin_port = htons(nPort);

    if (bind(m_listenSocket, (SOCKADDR*)&service, sizeof(service)) == SOCKET_ERROR)
    {
        Log(_T("Failed to bind port"));
        closesocket(m_listenSocket);
        m_listenSocket = INVALID_SOCKET;
        return;
    }

    if (listen(m_listenSocket, SOMAXCONN) == SOCKET_ERROR)
    {
        Log(_T("Listen failed"));
        closesocket(m_listenSocket);
        m_listenSocket = INVALID_SOCKET;
        return;
    }

    m_bListening = TRUE;
    m_mode = MODE_SERVER;
    m_bRunning = TRUE;

    CString logMsg;
    logMsg.Format(_T("Listening on port: %d"), nPort);
    Log(logMsg);

    m_acceptThread = std::thread(&CTcpTestDlg::AcceptThreadProc, this);
    UpdateUIState();
}

void CTcpTestDlg::AcceptThreadProc()
{
    while (m_bRunning && m_listenSocket != INVALID_SOCKET)
    {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(m_listenSocket, &readfds);

        timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        int selectResult = select(0, &readfds, nullptr, nullptr, &timeout);
        if (selectResult == SOCKET_ERROR)
        {
            if (m_bRunning)
            {
                Log(_T("select error"));
            }
            break;
        }

        if (selectResult > 0 && FD_ISSET(m_listenSocket, &readfds))
        {
            sockaddr_in clientAddr;
            int clientAddrLen = sizeof(clientAddr);
            SOCKET clientSocket = accept(m_listenSocket, (SOCKADDR*)&clientAddr, &clientAddrLen);

            if (clientSocket != INVALID_SOCKET)
            {
                std::lock_guard<std::mutex> lock(m_socketMutex);

                if (m_connectedSocket != INVALID_SOCKET)
                {
                    closesocket(m_connectedSocket);
                }

                m_connectedSocket = clientSocket;
                m_bConnected = TRUE;

                CString clientIP = CString(inet_ntoa(clientAddr.sin_addr));
                int clientPort = ntohs(clientAddr.sin_port);
                CString logMsg;
                logMsg.Format(_T("Client connected: %s:%d"), clientIP, clientPort);
                Log(logMsg);

                if (m_receiveThread.joinable())
                {
                    m_receiveThread.join();
                }
                m_receiveThread = std::thread(&CTcpTestDlg::ReceiveThreadProc, this);

                PostMessage(WM_SOCKET_MESSAGE, 0, 0);
            }
        }
    }
}

void CTcpTestDlg::OnBnClickedBtnConnect()
{
    if (m_bConnected)
    {
        OnBnClickedBtnDisconnect();
        return;
    }

    CString strIP, strPort;
    m_editIP.GetWindowText(strIP);
    m_editRemotePort.GetWindowText(strPort);
    int nPort = _ttoi(strPort);

    if (strIP.IsEmpty())
    {
        Log(_T("Please enter IP address"));
        return;
    }

    if (nPort <= 0 || nPort > 65535)
    {
        Log(_T("Please enter a valid port (1-65535)"));
        return;
    }

    m_connectedSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_connectedSocket == INVALID_SOCKET)
    {
        Log(_T("Failed to create socket"));
        return;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(nPort);

    if (inet_pton(AF_INET, CT2A(strIP), &serverAddr.sin_addr) <= 0)
    {
        Log(_T("Invalid IP address format"));
        closesocket(m_connectedSocket);
        m_connectedSocket = INVALID_SOCKET;
        return;
    }

    CString logMsg;
    logMsg.Format(_T("Connecting to %s:%d..."), strIP, nPort);
    Log(logMsg);

    if (connect(m_connectedSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
    {
        Log(_T("Connection failed"));
        closesocket(m_connectedSocket);
        m_connectedSocket = INVALID_SOCKET;
        return;
    }

    m_bConnected = TRUE;
    m_mode = MODE_CLIENT;
    m_bRunning = TRUE;

    logMsg.Format(_T("Connected to %s:%d"), strIP, nPort);
    Log(logMsg);

    m_receiveThread = std::thread(&CTcpTestDlg::ReceiveThreadProc, this);
    UpdateUIState();
}

void CTcpTestDlg::ReceiveThreadProc()
{
    const int BUFFER_SIZE = 4096;
    char buffer[BUFFER_SIZE];

    while (m_bRunning && m_connectedSocket != INVALID_SOCKET)
    {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(m_connectedSocket, &readfds);

        timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        int selectResult = select(0, &readfds, nullptr, nullptr, &timeout);
        if (selectResult == SOCKET_ERROR)
        {
            if (m_bRunning)
            {
                Log(_T("Error receiving data"));
            }
            break;
        }

        if (selectResult > 0 && FD_ISSET(m_connectedSocket, &readfds))
        {
            int bytesReceived = recv(m_connectedSocket, buffer, BUFFER_SIZE, 0);

            if (bytesReceived > 0)
            {
                std::vector<BYTE> data(buffer, buffer + bytesReceived);
                CString displayData = FormatReceiveData(data);

                CString logMsg;
                logMsg.Format(_T("Received data, length: %d bytes"), bytesReceived);
                Log(logMsg);

                m_editReceive.SetSel(-1, -1);
                m_editReceive.ReplaceSel(displayData + _T("\r\n"));
            }
            else if (bytesReceived == 0)
            {
                Log(_T("Connection closed"));
                break;
            }
            else
            {
                if (WSAGetLastError() != WSAEWOULDBLOCK)
                {
                    Log(_T("Failed to receive data"));
                    break;
                }
            }
        }
    }

    std::lock_guard<std::mutex> lock(m_socketMutex);
    if (m_connectedSocket != INVALID_SOCKET)
    {
        closesocket(m_connectedSocket);
        m_connectedSocket = INVALID_SOCKET;
    }
    m_bConnected = FALSE;
    PostMessage(WM_SOCKET_MESSAGE, 0, 0);
}

void CTcpTestDlg::OnBnClickedBtnDisconnect()
{
    Log(_T("Disconnecting..."));
    m_bRunning = FALSE;

    if (m_receiveThread.joinable())
    {
        m_receiveThread.join();
    }

    std::lock_guard<std::mutex> lock(m_socketMutex);
    if (m_connectedSocket != INVALID_SOCKET)
    {
        closesocket(m_connectedSocket);
        m_connectedSocket = INVALID_SOCKET;
    }

    m_bConnected = FALSE;
    m_mode = MODE_NONE;
    Log(_T("Disconnected"));
    UpdateUIState();
}

void CTcpTestDlg::OnBnClickedBtnSend()
{
    if (!m_bConnected)
    {
        Log(_T("No connection"));
        return;
    }

    CString strSend;
    m_editSend.GetWindowText(strSend);

    if (strSend.IsEmpty())
    {
        Log(_T("Please enter data to send"));
        return;
    }

    std::vector<BYTE> sendData;

    if (IsTextMode())
    {
        CT2A asciiText(strSend);
        sendData.assign(asciiText.m_psz, asciiText.m_psz + strlen(asciiText.m_psz));
    }
    else
    {
        sendData = HexStringToBytes(strSend);
        if (sendData.empty() && !strSend.IsEmpty())
        {
            Log(_T("Invalid hex format"));
            return;
        }
    }

    if (sendData.empty())
    {
        Log(_T("No data to send"));
        return;
    }

    int bytesSent = send(m_connectedSocket, (const char*)sendData.data(), sendData.size(), 0);

    if (bytesSent == SOCKET_ERROR)
    {
        Log(_T("Send failed"));
        return;
    }

    CString logMsg;
    logMsg.Format(_T("Sent %d bytes"), bytesSent);
    Log(logMsg);
}

void CTcpTestDlg::OnBnClickedBtnClearReceive()
{
    m_editReceive.SetWindowText(_T(""));
}

void CTcpTestDlg::OnBnClickedBtnClearLog()
{
    m_editLog.SetWindowText(_T(""));
}

LRESULT CTcpTestDlg::OnSocketMessage(WPARAM wParam, LPARAM lParam)
{
    UpdateUIState();
    return 0;
}

void CTcpTestDlg::Log(const CString& message)
{
    CString logLine;
    logLine.Format(_T("[%s] %s\r\n"), GetCurrentTime(), message);
    
    m_editLog.SetSel(-1, -1);
    m_editLog.ReplaceSel(logLine);
}

CString CTcpTestDlg::GetCurrentTime()
{
    CTime time = CTime::GetCurrentTime();
    return time.Format(_T("%H:%M:%S"));
}

void CTcpTestDlg::UpdateUIState()
{
    if (m_bListening)
    {
        m_btnListen.SetWindowText(_T("Stop Listening"));
        m_editPort.EnableWindow(FALSE);
    }
    else
    {
        m_btnListen.SetWindowText(_T("Listen"));
        m_editPort.EnableWindow(TRUE);
    }

    if (m_bConnected)
    {
        m_btnConnect.SetWindowText(_T("Disconnect"));
        m_editIP.EnableWindow(FALSE);
        m_editRemotePort.EnableWindow(FALSE);
        m_btnListen.EnableWindow(FALSE);
        m_btnDisconnect.EnableWindow(TRUE);
        m_btnSend.EnableWindow(TRUE);
    }
    else
    {
        m_btnConnect.SetWindowText(_T("Connect"));
        m_editIP.EnableWindow(TRUE);
        m_editRemotePort.EnableWindow(TRUE);
        m_btnListen.EnableWindow(!m_bListening);
        m_btnDisconnect.EnableWindow(FALSE);
        m_btnSend.EnableWindow(FALSE);
    }

    if (m_bListening || m_bConnected)
    {
        m_staticStatus.SetWindowText(_T("Status: Connected"));
    }
    else
    {
        m_staticStatus.SetWindowText(_T("Status: Disconnected"));
    }
}

BOOL CTcpTestDlg::IsTextMode()
{
    return m_radioText.GetCheck() == BST_CHECKED;
}

std::vector<BYTE> CTcpTestDlg::HexStringToBytes(const CString& hexStr)
{
    std::vector<BYTE> result;
    CString trimmedStr = hexStr;
    trimmedStr.Remove(_T(' '));
    trimmedStr.Remove(_T('\t'));
    trimmedStr.Remove(_T('\n'));
    trimmedStr.Remove(_T('\r'));

    int len = trimmedStr.GetLength();
    if (len % 2 != 0)
    {
        return result;
    }

    for (int i = 0; i < len; i += 2)
    {
        CString byteStr = trimmedStr.Mid(i, 2);
        TCHAR* endptr;
        long byteVal = _tcstol(byteStr, &endptr, 16);
        
        if (*endptr != _T('\0') || byteVal < 0 || byteVal > 255)
        {
            return std::vector<BYTE>();
        }
        
        result.push_back((BYTE)byteVal);
    }

    return result;
}

CString CTcpTestDlg::BytesToHexString(const std::vector<BYTE>& data)
{
    CString result;
    for (size_t i = 0; i < data.size(); i++)
    {
        CString byteStr;
        byteStr.Format(_T("%02X "), data[i]);
        result += byteStr;
    }
    return result.TrimRight();
}

CString CTcpTestDlg::FormatReceiveData(const std::vector<BYTE>& data)
{
    BOOL isText = TRUE;
    for (BYTE b : data)
    {
        if (b < 32 && b != 9 && b != 10 && b != 13)
        {
            isText = FALSE;
            break;
        }
        if (b > 126)
        {
            isText = FALSE;
            break;
        }
    }

    if (isText)
    {
        CString result;
        for (BYTE b : data)
        {
            result += (TCHAR)b;
        }
        return result;
    }
    else
    {
        return BytesToHexString(data);
    }
}
