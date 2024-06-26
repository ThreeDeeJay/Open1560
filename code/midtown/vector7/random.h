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
    vector7:random

    0x56D9F0 | public: void __thiscall Random::Seed(int) | ?Seed@Random@@QAEXH@Z
    0x56DAB0 | public: float __thiscall Random::Number(void) | ?Number@Random@@QAEMXZ
    0x56DB10 | public: float __thiscall Random::Normal(float,float) | ?Normal@Random@@QAEMMM@Z
*/

struct Random
{
public:
    // ?Normal@Random@@QAEMMM@Z | unused
    ARTS_IMPORT f32 Normal(f32 arg1, f32 arg2);

    // ?Number@Random@@QAEMXZ
    ARTS_IMPORT f32 Number();

    // ?Seed@Random@@QAEXH@Z
    ARTS_IMPORT void Seed(i32 arg1);

    u8 gap0[0xE8];
};

check_size(Random, 0xE8);
