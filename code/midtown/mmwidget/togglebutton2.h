/*
    Open1560 - An Open Source Re-Implementation of Midtown Madness 1 Beta
    Copyright (C) 2020 Brick

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

/*
    mmwidget:togglebutton2

    0x4BC730 | public: __thiscall UIToggleButton2::UIToggleButton2(void) | ??0UIToggleButton2@@QAE@XZ
    0x4BC7B0 | public: virtual __thiscall UIToggleButton2::~UIToggleButton2(void) | ??1UIToggleButton2@@UAE@XZ
    0x4BC7C0 | public: void __thiscall UIToggleButton2::Init(struct LocString *,int *,float,float,float,float,int,int,class Callback) | ?Init@UIToggleButton2@@QAEXPAULocString@@PAHMMMMHHVCallback@@@Z
    0x4BC890 | protected: virtual void __thiscall UIToggleButton2::DrawOff(void) | ?DrawOff@UIToggleButton2@@MAEXXZ
    0x4BC8D0 | protected: virtual void __thiscall UIToggleButton2::DrawOn(void) | ?DrawOn@UIToggleButton2@@MAEXXZ
    0x4BC910 | public: void __thiscall UIToggleButton2::Unkill(void) | ?Unkill@UIToggleButton2@@QAEXXZ
    0x4BC940 | public: virtual void __thiscall UIToggleButton2::DoToggle(void) | ?DoToggle@UIToggleButton2@@UAEXXZ
    0x4BC960 | public: virtual void __thiscall UIToggleButton2::Action(union eqEvent) | ?Action@UIToggleButton2@@UAEXTeqEvent@@@Z
    0x4BC9E0 | public: virtual void __thiscall UIToggleButton2::Switch(int) | ?Switch@UIToggleButton2@@UAEXH@Z
    0x4BCA50 | public: virtual void * __thiscall UIToggleButton2::`vector deleting destructor'(unsigned int) | ??_EUIToggleButton2@@UAEPAXI@Z
    0x4BCA50 | public: virtual void * __thiscall UIToggleButton2::`scalar deleting destructor'(unsigned int) | ??_GUIToggleButton2@@UAEPAXI@Z
    0x4BCA80 | public: virtual void __thiscall UIToggleButton2::Update(void) | ?Update@UIToggleButton2@@UAEXXZ
    0x4BCA90 | public: virtual void __thiscall UIToggleButton2::Enable(void) | ?Enable@UIToggleButton2@@UAEXXZ
    0x4BCAA0 | public: virtual void __thiscall UIToggleButton2::Disable(void) | ?Disable@UIToggleButton2@@UAEXXZ
    0x61EF20 | const UIToggleButton2::`vftable' | ??_7UIToggleButton2@@6B@
    0x705C20 | protected: static int UIToggleButton2::ToggleBMLoaded | ?ToggleBMLoaded@UIToggleButton2@@1HA
*/

#include "button.h"

class UIToggleButton2 : public UIButton
{
public:
    // ??0UIToggleButton2@@QAE@XZ
    ARTS_IMPORT UIToggleButton2();

    // ??_GUIToggleButton2@@UAEPAXI@Z
    // ??_EUIToggleButton2@@UAEPAXI@Z
    // ??1UIToggleButton2@@UAE@XZ
    ARTS_IMPORT ~UIToggleButton2() override = default;

    // ?DoToggle@UIToggleButton2@@UAEXXZ
    ARTS_IMPORT virtual void DoToggle();

    // ?Action@UIToggleButton2@@UAEXTeqEvent@@@Z
    ARTS_IMPORT void Action(eqEvent arg1) override;

    // ?Disable@UIToggleButton2@@UAEXXZ | inline
    ARTS_IMPORT void Disable() override;

    // ?Enable@UIToggleButton2@@UAEXXZ | inline
    ARTS_IMPORT void Enable() override;

    // ?Init@UIToggleButton2@@QAEXPAULocString@@PAHMMMMHHVCallback@@@Z
    ARTS_IMPORT void Init(
        LocString* arg1, i32* arg2, f32 arg3, f32 arg4, f32 arg5, f32 arg6, i32 arg7, i32 arg8, Callback arg9);

    // ?Switch@UIToggleButton2@@UAEXH@Z
    ARTS_IMPORT void Switch(i32 arg1) override;

    // ?Unkill@UIToggleButton2@@QAEXXZ | unused
    ARTS_IMPORT void Unkill();

    // ?Update@UIToggleButton2@@UAEXXZ | inline
    ARTS_IMPORT void Update() override;

protected:
    // ?DrawOff@UIToggleButton2@@MAEXXZ
    ARTS_IMPORT virtual void DrawOff();

    // ?DrawOn@UIToggleButton2@@MAEXXZ
    ARTS_IMPORT virtual void DrawOn();

    // ?ToggleBMLoaded@UIToggleButton2@@1HA
    ARTS_IMPORT static i32 ToggleBMLoaded;

    u8 gapB4[0xC];
};

check_size(UIToggleButton2, 0xC0);
