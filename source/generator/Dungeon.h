/*
* Random dungeon generator
*
* Copyright (C) 2017 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#pragma once

#include <vector>
#include "BspPartition.h"
#include "Cell.h"

namespace dungeongenerator {

	class Dungeon
	{
	private:
		void getPartitions(BspPartition *bspPartition);
	public:
		int width;
		int height;
		std::vector<BspPartition*> partitionList;
		std::vector< std::vector<Cell*> > cells;
		Dungeon(int w, int h);
		~Dungeon();
		Cell* getCell(int x, int y);
		void generateRooms();
		void generateWalls();
		void generateDoors();
		BspPartition* getRandomRoom();
	};

}

