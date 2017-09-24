/*
* Random dungeon generator
*
* Copyright (C) 2017 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#pragma once

#include <vector>

namespace dungeongenerator {

	class BspPartition
	{
	public:
		BspPartition *parent;
		int left;
		int right;
		int top;
		int bottom;
		int centerX;
		int centerY;
		int splitIn;
		int depth;
		bool hasRoom = false;
		static const int BspPartitionSplitVertical = 0;
		static const int BspPartitionSplitHorizontal = 0;
		std::vector<BspPartition*> children;
		BspPartition(BspPartition *parent, int left, int top, int right, int bottom, int splitIn, int depth);
		~BspPartition();
		void placeRoom();
		void split(bool origin, int maxDivision, int minDivision);
		void createCorridor(int originX, int originY, int destX, int destY);
		void connect();
	};

}

