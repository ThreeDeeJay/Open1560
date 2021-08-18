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

define_dummy_symbol(mmcar_carmodel);

#include "carmodel.h"

mmCarModel::mmCarModel()
{
    field_20.Identity();
    Flags |= INST_FLAG_SHADOW | INST_FLAG_MOVER | INST_FLAG_VALID | INST_FLAG_400 | INST_FLAG_2000;
    CarFlags |= CAR_MODEL_FLAG_40;

    Sparks.Init(256, GetSparkLut(const_cast<char*>("tune/spark.tga")));
}

i32 mmCarModel::GetCarFlags(char* /*arg1*/)
{
    return 0;
}