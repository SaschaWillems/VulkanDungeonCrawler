/*
* Random dungeon generator
*
* Copyright (C) 2017 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#include "Room.h"


Room::Room(int posX, int posY)
{
}


Room::~Room()
{
}

void Room::SetSize(int left, int top, int right, int bottom)
{
	size.left = left;
	size.top = top;
	size.right = right;
	size.bottom = bottom;
}

