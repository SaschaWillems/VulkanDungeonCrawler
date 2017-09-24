/*
* Random dungeon generator
*
* Copyright (C) 2017 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#pragma once

template<class T>
struct rect
{
	rect() : left(), top(), right(), bottom() {}
	rect(T left, T top, T right, T bottom) :
		left(left), top(top), right(right), bottom(bottom) {}
	template<class Point>
	rect(Point p, T width, T height) :
		left(p.x), right(p.y), right(p.x + width), bottom(p.y + height) {}

	T left;
	T top;
	T right;
	T bottom;
};

class Room
{
public:
	int x;
	int y;
	rect<int> size;
	Room(int posX, int posY);
	~Room();
	void SetSize(int left, int top, int right, int bottom);
};

