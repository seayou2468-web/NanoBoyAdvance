// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cstdio>
#include <cstring>
#include <algorithm>

#include "common.h"
#include "log_manager.h"
#include "console_listener.h"

// iOS-compatible implementation: console logging via os/log.h
#ifdef __APPLE__
#include <os/log.h>
#endif

ConsoleListener::ConsoleListener() : bUseColor(false) {}

ConsoleListener::~ConsoleListener() {
    Close();
}

void ConsoleListener::Open(bool Hidden, int Width, int Height, const char* Title) {
    // No-op for iOS: console output handled by system logging
}

void ConsoleListener::UpdateHandle() {
    // No-op for iOS
}

void ConsoleListener::Close() {
    // No-op for iOS
}

bool ConsoleListener::IsOpen() {
    return true;  // Always "open" for iOS (logs go to system log)
}

void ConsoleListener::LetterSpace(int Width, int Height) {
    // No-op for iOS
}

void ConsoleListener::BufferWidthHeight(int BufferWidth, int BufferHeight, int ScreenWidth, int ScreenHeight, bool BufferFirst) {
    // No-op for iOS
}

void ConsoleListener::PixelSpace(int Left, int Top, int Width, int Height, bool) {
    // No-op for iOS
}

void ConsoleListener::Log(LogTypes::LOG_LEVELS Level, const char* Text) {
    if (!Text || !*Text)
        return;

#ifdef __APPLE__
    os_log_type_t log_type = OS_LOG_TYPE_DEFAULT;
    switch (Level) {
        case LogTypes::LOG_LEVELS::LNOTICE:
        case LogTypes::LOG_LEVELS::LINFO:
            log_type = OS_LOG_TYPE_INFO;
            break;
        case LogTypes::LOG_LEVELS::LWARNING:
            log_type = OS_LOG_TYPE_DEFAULT;
            break;
        case LogTypes::LOG_LEVELS::LERROR:
            log_type = OS_LOG_TYPE_ERROR;
            break;
        case LogTypes::LOG_LEVELS::LCRITICAL:
            log_type = OS_LOG_TYPE_FAULT;
            break;
        default:
            log_type = OS_LOG_TYPE_DEBUG;
            break;
    }
    os_log_with_type(OS_LOG_DEFAULT, log_type, "%{public}s", Text);
#else
    fprintf(stdout, "%s", Text);
    fflush(stdout);
#endif
}

void ConsoleListener::ClearScreen(bool Cursor) {
    // No-op for iOS
}
        // Change screen buffer size
        COORD Co = {BufferWidth, BufferHeight};
        SB = SetConsoleScreenBufferSize(hConsole, Co);
        // Change the screen buffer window size
        SMALL_RECT coo = {0,0,ScreenWidth, ScreenHeight}; // top, left, right, bottom
        SW = SetConsoleWindowInfo(hConsole, TRUE, &coo);
    }
    else
    {
        // Change the screen buffer window size
        SMALL_RECT coo = {0,0, ScreenWidth, ScreenHeight}; // top, left, right, bottom
        SW = SetConsoleWindowInfo(hConsole, TRUE, &coo);
        // Change screen buffer size
        COORD Co = {BufferWidth, BufferHeight};
        SB = SetConsoleScreenBufferSize(hConsole, Co);
    }
#endif
}
void ConsoleListener::LetterSpace(int Width, int Height)
{
#ifdef _WIN32
    // Get console info
    CONSOLE_SCREEN_BUFFER_INFO ConInfo;
    GetConsoleScreenBufferInfo(hConsole, &ConInfo);

    //
    int OldBufferWidth = ConInfo.dwSize.X;
    int OldBufferHeight = ConInfo.dwSize.Y;
    int OldScreenWidth = (ConInfo.srWindow.Right - ConInfo.srWindow.Left);
    int OldScreenHeight = (ConInfo.srWindow.Bottom - ConInfo.srWindow.Top);
    //
    int NewBufferWidth = Width;
    int NewBufferHeight = Height;
    int NewScreenWidth = NewBufferWidth - 1;
    int NewScreenHeight = OldScreenHeight;

    // Width
    BufferWidthHeight(NewBufferWidth, OldBufferHeight, NewScreenWidth, OldScreenHeight, (NewBufferWidth > OldScreenWidth-1));
    // Height
    BufferWidthHeight(NewBufferWidth, NewBufferHeight, NewScreenWidth, NewScreenHeight, (NewBufferHeight > OldScreenHeight-1));

    // Resize the window too
    //MoveWindow(GetConsoleWindow(), 200,200, (Width*8 + 50),(NewScreenHeight*12 + 200), true);
#endif
}
#ifdef _WIN32
COORD ConsoleListener::GetCoordinates(int BytesRead, int BufferWidth)
{
    COORD Ret = {0, 0};
    // Full rows
    int Step = (int)floor((float)BytesRead / (float)BufferWidth);
    Ret.Y += Step;
    // Partial row
    Ret.X = BytesRead - (BufferWidth * Step);
    return Ret;
}
#endif
void ConsoleListener::PixelSpace(int Left, int Top, int Width, int Height, bool Resize)
{
#ifdef _WIN32
    // Check size
    if (Width < 8 || Height < 12) return;

    bool DBef = true;
    bool DAft = true;
    std::string SLog = "";

    const HWND hWnd = GetConsoleWindow();
    const HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

    // Get console info
    CONSOLE_SCREEN_BUFFER_INFO ConInfo;
    GetConsoleScreenBufferInfo(hConsole, &ConInfo);
    DWORD BufferSize = ConInfo.dwSize.X * ConInfo.dwSize.Y;

    // ---------------------------------------------------------------------
    //  Save the current text
    // ------------------------
    DWORD cCharsRead = 0;
    COORD coordScreen = { 0, 0 };

    static const int MAX_BYTES = 1024 * 16;

    std::vector<std::array<TCHAR, MAX_BYTES>> Str;
    std::vector<std::array<WORD, MAX_BYTES>> Attr;

    // ReadConsoleOutputAttribute seems to have a limit at this level
    static const int ReadBufferSize = MAX_BYTES - 32;

    DWORD cAttrRead = ReadBufferSize;
    DWORD BytesRead = 0;
    while (BytesRead < BufferSize)
    {
        Str.resize(Str.size() + 1);
        if (!ReadConsoleOutputCharacter(hConsole, Str.back().data(), ReadBufferSize, coordScreen, &cCharsRead))
            SLog += StringFromFormat("WriteConsoleOutputCharacter error");

        Attr.resize(Attr.size() + 1);
        if (!ReadConsoleOutputAttribute(hConsole, Attr.back().data(), ReadBufferSize, coordScreen, &cAttrRead))
            SLog += StringFromFormat("WriteConsoleOutputAttribute error");

        // Break on error
        if (cAttrRead == 0) break;
        BytesRead += cAttrRead;
        coordScreen = GetCoordinates(BytesRead, ConInfo.dwSize.X);
    }
    // Letter space
    int LWidth = (int)(floor((float)Width / 8.0f) - 1.0f);
    int LHeight = (int)(floor((float)Height / 12.0f) - 1.0f);
    int LBufWidth = LWidth + 1;
    int LBufHeight = (int)floor((float)BufferSize / (float)LBufWidth);
    // Change screen buffer size
    LetterSpace(LBufWidth, LBufHeight);


    ClearScreen(true);
    coordScreen.Y = 0;
    coordScreen.X = 0;
    DWORD cCharsWritten = 0;

    int BytesWritten = 0;
    DWORD cAttrWritten = 0;
    for (size_t i = 0; i < Attr.size(); i++)
    {
        if (!WriteConsoleOutputCharacter(hConsole, Str[i].data(), ReadBufferSize, coordScreen, &cCharsWritten))
            SLog += StringFromFormat("WriteConsoleOutputCharacter error");
        if (!WriteConsoleOutputAttribute(hConsole, Attr[i].data(), ReadBufferSize, coordScreen, &cAttrWritten))
            SLog += StringFromFormat("WriteConsoleOutputAttribute error");

        BytesWritten += cAttrWritten;
        coordScreen = GetCoordinates(BytesWritten, LBufWidth);
    }    

    const int OldCursor = ConInfo.dwCursorPosition.Y * ConInfo.dwSize.X + ConInfo.dwCursorPosition.X;
    COORD Coo = GetCoordinates(OldCursor, LBufWidth);
    SetConsoleCursorPosition(hConsole, Coo);

    if (SLog.length() > 0) Log(LogTypes::LNOTICE, SLog.c_str());

    // Resize the window too
    if (Resize) MoveWindow(GetConsoleWindow(), Left,Top, (Width + 100),Height, true);
#endif
}

void ConsoleListener::Log(LogTypes::LOG_LEVELS Level, const char *Text)
{
#if defined(_WIN32)
    /*
    const int MAX_BYTES = 1024*10;
    char Str[MAX_BYTES];
    va_list ArgPtr;
    int Cnt;
    va_start(ArgPtr, Text);
    Cnt = vsnprintf(Str, MAX_BYTES, Text, ArgPtr);
    va_end(ArgPtr);
    */
    DWORD cCharsWritten;
    WORD Color;

    switch (Level)
    {
    case NOTICE_LEVEL: // light green
        Color = FOREGROUND_GREEN | FOREGROUND_INTENSITY;
        break;
    case ERROR_LEVEL: // light red
        Color = FOREGROUND_RED | FOREGROUND_INTENSITY;
        break;
    case WARNING_LEVEL: // light yellow
        Color = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
        break;
    case INFO_LEVEL: // cyan
        Color = FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
        break;
    case DEBUG_LEVEL: // gray
        Color = FOREGROUND_INTENSITY;
        break;
    default: // off-white
        Color = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
        break;
    }
    if (strlen(Text) > 10)
    {
        // First 10 chars white
        SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
        WriteConsole(hConsole, Text, 10, &cCharsWritten, NULL);
        Text += 10;
    }
    SetConsoleTextAttribute(hConsole, Color);
    WriteConsole(hConsole, Text, (DWORD)strlen(Text), &cCharsWritten, NULL);
#else
    char ColorAttr[16] = "";
    char ResetAttr[16] = "";

    if (bUseColor)
    {
        strcpy(ResetAttr, "\033[0m");
        switch (Level)
        {
        case NOTICE_LEVEL: // light green
            strcpy(ColorAttr, "\033[92m");
            break;
        case ERROR_LEVEL: // light red
            strcpy(ColorAttr, "\033[91m");
            break;
        case WARNING_LEVEL: // light yellow
            strcpy(ColorAttr, "\033[93m");
            break;
        default:
            break;
        }
    }
    fprintf(stderr, "%s%s%s", ColorAttr, Text, ResetAttr);
#endif
}
// Clear console screen
void ConsoleListener::ClearScreen(bool Cursor)
{ 
#if defined(_WIN32)
    COORD coordScreen = { 0, 0 }; 
    DWORD cCharsWritten; 
    CONSOLE_SCREEN_BUFFER_INFO csbi; 
    DWORD dwConSize; 

    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE); 

    GetConsoleScreenBufferInfo(hConsole, &csbi); 
    dwConSize = csbi.dwSize.X * csbi.dwSize.Y;
    // Write space to the entire console
    FillConsoleOutputCharacter(hConsole, TEXT(' '), dwConSize, coordScreen, &cCharsWritten); 
    GetConsoleScreenBufferInfo(hConsole, &csbi); 
    FillConsoleOutputAttribute(hConsole, csbi.wAttributes, dwConSize, coordScreen, &cCharsWritten);
    // Reset cursor
    if (Cursor) SetConsoleCursorPosition(hConsole, coordScreen); 
#endif
}


