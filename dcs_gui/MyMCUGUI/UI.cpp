/*
*    UI.cpp
* 
*    Original file: WASMClient.cpp
*    Copyright 2021 John L. Dowson
*    Permission is hereby granted, free of charge, to any person obtaining a copy of this software
*    and associated documentation files (the "Software"), to deal in the Software without restriction,
*    including without limitation the rights to use, copy, modify, merge, publish, distribute,
*    sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
*    furnished to do so, subject to the following conditions:
*
*    The above copyright notice and this permission notice shall be included in all copies or
*    substantial portions of the Software.
*
*    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
*    INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
*    PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE
*    FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
*    OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
*    DEALINGS IN THE SOFTWARE.
*/

#define WIN32_LEAN_AND_MEAN

#include "framework.h"
#include <Winsock2.h> // before Windows.h, else Winsock 1 conflict
#include <Ws2tcpip.h> // needed for ip_mreq definition for multicast
#include <Windows.h>
#include <wchar.h>
#include <map>
#include "HIDDevice.h"
#include "Resource.h"
#include "UDPMulticastReceiver.h"
#include "UDPSender.h"
#include <mutex>
#include <deque>

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;                                // current instance
CHAR szTitle[MAX_LOADSTRING];                  // The title bar text
CHAR szWindowClass[MAX_LOADSTRING];            // the main window class name
ULONGLONG systemStartTime;
int     quit = 0;
std::shared_ptr<HIDDevice> hid;

// Sockets
UDPMulticastReceiver udpReceiver;
UDPSender udpSender;

// Threading
volatile bool isRunning = false;
volatile  HANDLE recvThread = NULL;
volatile  HANDLE sendThread = NULL;
volatile  HANDLE hidThread = NULL;

std::mutex hidMutex;
std::deque<std::vector<unsigned char>> readDataDeque;
std::deque<std::vector<unsigned char>> writeDataDeque;


// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: Place code here.
    systemStartTime = GetTickCount64();

    // Initialize global strings
    LoadStringA(hInstance, IDS_APP_TITLE, (LPSTR)&szTitle, MAX_LOADSTRING);
    LoadStringA(hInstance, IDC_WASMCLIENT, (LPSTR)&szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Perform application initialization:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_WASMCLIENT));

    MSG msg;

    // Main message loop:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int) msg.wParam;
}


//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEX wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_WASMCLIENT));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCE(IDC_WASMCLIENT);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassEx(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // Store instance handle in our global variable

   HWND hWnd = CreateWindow(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, CW_USEDEFAULT, 600, 200, 0, nullptr, hInstance, nullptr);

   if (!hWnd)
   {
      return FALSE;
   }

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}

void FlashPanelOnConnect(std::shared_ptr<HIDDevice> hid)
{
    hid->Write(HIDDevice::CMDS_ANNUC_CHANGED, HIDDevice::ANNUC_WARN_ON);
    Sleep(50);
    hid->Write(HIDDevice::CMDS_ANNUC_CHANGED, HIDDevice::ANNUC_WARN_OFF);
    Sleep(50);

    hid->Write(HIDDevice::CMDS_ANNUC_CHANGED, HIDDevice::ANNUC_CAUT_ON);
    Sleep(50);
    hid->Write(HIDDevice::CMDS_ANNUC_CHANGED, HIDDevice::ANNUC_CAUT_OFF);
    Sleep(50);

    hid->Write(HIDDevice::CMDS_ANNUC_CHANGED, HIDDevice::ANNUC_AP_ON);
    Sleep(50);
    hid->Write(HIDDevice::CMDS_ANNUC_CHANGED, HIDDevice::ANNUC_AP_OFF);
    Sleep(50);

    hid->Write(HIDDevice::CMDS_ANNUC_CHANGED, HIDDevice::ANNUC_YD_ON);
    Sleep(50);
    hid->Write(HIDDevice::CMDS_ANNUC_CHANGED, HIDDevice::ANNUC_YD_OFF);
}

DWORD WINAPI RecvThread(void* param)
{
    int ret = udpReceiver.Init();

    while (ret == 0 && isRunning)
    {
        // Socket -> queue
        ret = udpReceiver.Recv([](std::vector<char>& data) -> void
        {
                // Add to queue, but remove leading bytes (headers)
                std::vector<unsigned char> hidWriteData(data.size() - 2);
                hidWriteData.assign(data.begin() + 2, data.end());

                std::lock_guard<std::mutex> guard(hidMutex);
                writeDataDeque.push_back(hidWriteData);
        });

        Sleep(1);
    }

    return 0;
}

DWORD WINAPI SendThread(void* param)
{
    int ret = udpSender.Init();

    while (ret == 0 && isRunning)
    {
        // queue -> Socket
        if (readDataDeque.size() > 0)
        {
            std::vector<unsigned char> d;
            {
                std::lock_guard<std::mutex> guard(hidMutex);
                d = readDataDeque.front();
                readDataDeque.pop_front();
            }

            // Forward on data to socket
            ret = udpSender.Send(d);
        }

        Sleep(1);
    }

    return 0;
}

DWORD WINAPI HidThread(void* param)
{
    while (isRunning)
    {
        // queue -> HID
        if (writeDataDeque.size() > 0)
        {
            std::vector<unsigned char> d;
            {
                std::lock_guard<std::mutex> guard(hidMutex);
                d = writeDataDeque.front();
                writeDataDeque.pop_front();
            }

            hid->Write(d);
        }

        // HID -> queue
        std::vector<unsigned char> hidReadData = hid->Read(4, true);
        if (hidReadData.size() > 0)
        {
            std::lock_guard<std::mutex> guard(hidMutex);
            readDataDeque.push_back(hidReadData);
        }

        Sleep(1);
    }

    return 0;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    static HWND hwndEdit;

    switch (message)
    {
    case WM_CREATE:
        hwndEdit = CreateWindowEx(
            0, "EDIT",   // predefined class 
            NULL,         // no window title 
            WS_CHILD | WS_VISIBLE | WS_VSCROLL |
            ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL,
            0, 0, 0, 0,   // set size in WM_SIZE message 
            hWnd,         // parent window 
            (HMENU)ID_EDITCHILD,   // edit control ID 
            (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE),
            NULL);        // pointer not needed 

        // Add text to the window. 
        SendMessage(hwndEdit, WM_SETTEXT, 0, (LPARAM)"Ready for connection");

        WSADATA wsaData;
        if (WSAStartup(0x0101, &wsaData))
        {
            perror("WSAStartup");
            return 1;
        }

        return 0;

    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            // Parse the menu selections:
            switch (wmId)
            {
            case IDM_ABOUT:
                DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                break;
            case IDM_EXIT:
                DestroyWindow(hWnd);
                break;
            case ID_FILE_START:
            {
                HMENU hMenu = GetMenu(hWnd);
                std::string windowText;

                hid = std::make_shared<HIDDevice>();
                bool hidSuccess = hid->Open();
                if (hidSuccess)
                {
                    windowText += "HID device connected!\r\n";
                }
                else
                {
                    windowText += "HID device failed to connect. Is it plugged in?\r\n";
                }


                if (hidSuccess)
                {
                    FlashPanelOnConnect(hid);
                    EnableMenuItem(hMenu, ID_FILE_STOP, MF_BYCOMMAND | MF_ENABLED);
                    EnableMenuItem(hMenu, ID_FILE_START, MF_BYCOMMAND | MF_DISABLED);

                    isRunning = true;
                    DWORD threadId = 0;
                    if (recvThread == NULL)
                    {
                        recvThread = CreateThread(
                            NULL,							// default security attributes
                            0,								// use default stack size  
                            RecvThread,	                // thread function name
                            NULL,					    // argument to thread function 
                            0,								// use default creation flags 
                            &threadId);				        // returns the thread identifier 

                        if (recvThread == NULL)
                        {
                            isRunning = false;
                        }
                    }
                    if (sendThread == NULL)
                    {
                        sendThread = CreateThread(
                            NULL,							// default security attributes
                            0,								// use default stack size  
                            SendThread,	                // thread function name
                            NULL,					    // argument to thread function 
                            0,								// use default creation flags 
                            &threadId);				        // returns the thread identifier 

                        if (sendThread == NULL)
                        {
                            isRunning = false;
                        }
                    }
                    if (hidThread == NULL)
                    {
                        hidThread = CreateThread(
                            NULL,							// default security attributes
                            0,								// use default stack size  
                            HidThread,	                // thread function name
                            NULL,					    // argument to thread function 
                            0,								// use default creation flags 
                            &threadId);				        // returns the thread identifier 

                        if (hidThread == NULL)
                        {
                            isRunning = false;
                        }
                    }
                }

                SendMessage(hwndEdit, WM_SETTEXT, 0, (LPARAM)windowText.c_str());
                break;
            }
            case ID_FILE_STOP:
            {
                HMENU hMenu = GetMenu(hWnd);

                isRunning = false;
                WaitForSingleObject(recvThread, 5000);
                recvThread = NULL;
                WaitForSingleObject(sendThread, 5000);
                sendThread = NULL;
                WaitForSingleObject(hidThread, 5000);
                hidThread = NULL;

                if (hid)
                    hid->Close();

                hid.reset();

                EnableMenuItem(hMenu, ID_FILE_STOP, MF_BYCOMMAND | MF_DISABLED);
                EnableMenuItem(hMenu, ID_FILE_START, MF_BYCOMMAND | MF_ENABLED);

                SendMessage(hwndEdit, WM_SETTEXT, 0, (LPARAM)"HID device disconnected");
                break;
            }
            case ID_AIRCRAFT_CJ4:
            {
                break;
            }
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            // TODO: Add any drawing code that uses hdc here...
            EndPaint(hWnd, &ps);
        }
        break;
    case WM_SIZE:
        // Make the edit control the size of the window's client area. 

        MoveWindow(hwndEdit,
            0, 0,                  // starting x- and y-coordinates 
            LOWORD(lParam),        // width of client area 
            HIWORD(lParam),        // height of client area 
            TRUE);                 // repaint window 
        return 0;
    case WM_DESTROY:
        WSACleanup();

        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}