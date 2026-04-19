// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _CONSOLELISTENER_H
#define _CONSOLELISTENER_H

#include "log_manager.h"

// iOS-compatible console listener stub (no native console window on iOS)
class ConsoleListener : public LogListener {
public:
    ConsoleListener();
    ~ConsoleListener();

    void Open(bool Hidden = false, int Width = 100, int Height = 100, const char* Name = "Console");
    void UpdateHandle();
    void Close();
    bool IsOpen();
    void LetterSpace(int Width, int Height);
    void BufferWidthHeight(int BufferWidth, int BufferHeight, int ScreenWidth, int ScreenHeight, bool BufferFirst);
    void PixelSpace(int Left, int Top, int Width, int Height, bool);
    void Log(LogTypes::LOG_LEVELS level, const char* Text) override;
    void ClearScreen(bool Cursor = true);

private:
    bool bUseColor = false;
};

#endif
