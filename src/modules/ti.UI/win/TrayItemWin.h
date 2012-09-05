/*
 * Copyright (c) 2009-2010 Appcelerator, Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef TrayItemWin_h
#define TrayItemWin_h

#include "../TrayItem.h"

#include <windows.h>
#include <shellapi.h>

namespace ti {

class TrayItemWin : public TrayItem {       
public:
    TrayItemWin(std::string& iconURL, MethodRef cbSingleClick);
    virtual ~TrayItemWin();

    void SetIcon(std::string& iconPath);
    void SetMenu(AutoPtr<Menu> menu);
    void SetHint(std::string& hint);
    void Remove();
    void ShowTrayMenu();
    void HandleRightClick();
    void HandleLeftClick();
    void HandleDoubleLeftClick();
    UINT GetId();
    static bool MessageHandler(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
    
    static LRESULT CALLBACK DoubleClickTimerProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

    private:
    HMENU oldNativeMenu;
    NOTIFYICONDATA* trayIconData;
    static std::vector<AutoPtr<TrayItemWin> > trayItems;
    static UINT trayClickedMessage;
    bool is_double_clicked;
};

} // namespace ti

#endif
