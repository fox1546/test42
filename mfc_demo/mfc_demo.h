#pragma once

#include "resource.h"

class CTcpTestApp : public CWinApp
{
public:
    CTcpTestApp();

public:
    virtual BOOL InitInstance();

    DECLARE_MESSAGE_MAP()
};
