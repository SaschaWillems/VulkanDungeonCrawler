/*
* Random dungeon generator
*
* Copyright (C) 2017 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#include "Dungeon.h"
#include "BspPartition.h"
#include <stdio.h>

namespace dungeongenerator {

	Dungeon::Dungeon(int w, int h)
	{
		width = w;
		height = h;

		cells.resize(width);
		for (int x = 0; x < width; ++x) {
			cells[x].resize(height);
			for (int y = 0; y < height; ++y) {
				cells[x][y] = new Cell(x, y);
			}
		}

	}


	Dungeon::~Dungeon()
	{
		for (int x = 0; x < width; ++x) {
			cells[x].resize(height);
			for (int y = 0; y < height; ++y) {
				delete(cells[x][y]);
			}
		}
	}

	Cell* Dungeon::getCell(int x, int y) {
		return cells[x][y];
	}

	void generateCells(Dungeon* dungeon, BspPartition* bspPartition) {
		// TODO: Constant for max. room size (to avoid huge rooms)
		if ((bspPartition->children.size() == 0) && (rand() % 100) < 75 /*dungeonRoomFrequency*/) {
			bspPartition->hasRoom = true;
			for (int x = bspPartition->left + 2; x <= bspPartition->right - 2; x++) {
				for (int y = bspPartition->top + 2; y <= bspPartition->bottom - 2; y++) {
					if ((x >= dungeon->width - 1) || (y >= dungeon->height - 1)) {
						break;
					}
					dungeon->cells[x][y]->type = Cell::cellTypeRoom;
					//this.roomDimLeft = this.left + 2;
					//this.roomDimTop = this.top + 2;
					//this.roomDimRight = this.right - 2;
					//this.roomDimBottom = this.bottom - 2;
				}
			}


		}
		else {
			for (auto& child : bspPartition->children) {
				generateCells(dungeon, child);
			}
		}

	}

	void Dungeon::getPartitions(BspPartition *bspPartition)
	{
		if (bspPartition->children.empty()) {
			partitionList.push_back(bspPartition);
		} else {
			for (auto child : bspPartition->children) {
				getPartitions(child);
			}
		}
	}

	void generateCorridor(Dungeon* dungeon, int startX, int startY, int destX, int destY) {

		int curPosX = startX;
		int curPosY = startY;

		do {

			if (curPosX < destX) {
				curPosX++;
			}
			else {
				if (curPosX > destX) {
					curPosX--;
				}
				else {
					if (curPosY < destY) {
						curPosY++;
					}
					else {
						if (curPosY > destY) {
							curPosY--;
						}
					}
				}
			}

			//if (!dungeon->pointInRoom(curPosX, curPosY)) {
			//	dungeon->cell[curPosX][curPosY].isCorridor = true;
			//}

			if (dungeon->cells[curPosX][curPosY]->type != Cell::cellTypeRoom) {
				dungeon->cells[curPosX][curPosY]->type = Cell::cellTypeCorridor;
			}

		} while ((curPosX != destX) || (curPosY != destY));

	}
	
	void connectPartition(Dungeon* dungeon, BspPartition* partition) {
		std::vector<BspPartition*> connectionList;

		// Add child nodes with rooms to connection target list
		if (!partition->children.empty()) {
			for (auto child : partition->children) {
				if (child->hasRoom) {
					connectionList.push_back(child);
				}
			}
		}

		// Add this node (and if set it's parent) to the connection list
		connectionList.push_back(partition);
		if (partition->parent != NULL) {
			connectionList.push_back(partition->parent);
		}

		if (connectionList.size() == 0) {
			return;
		}

		for (int i = 0; i < connectionList.size() - 1; i++) {
			int startX = connectionList[i]->centerX;
			int startY = connectionList[i]->centerY;
			int destX = connectionList[i + 1]->centerX;
			int destY = connectionList[i + 1]->centerY;
			generateCorridor(dungeon, startX, startY, destX, destY);
		}

		if (partition->parent != NULL) {
			connectPartition(dungeon, partition->parent);
		}
	}

	void generateCorridors(Dungeon* dungeon) {
		for (auto partition : dungeon->partitionList) {
			if ((partition->hasRoom) && (partition->children.empty())) {
				connectPartition(dungeon, partition);
			}
		}
	}
	
	void Dungeon::generateRooms() {
		dungeongenerator::BspPartition rootPartition(NULL, 0, 0, width, height, BspPartition::BspPartitionSplitHorizontal, 0);
		//rootPartition.split(true, 15, 32);

		generateCells(this, &rootPartition);

		partitionList.clear();
		getPartitions(&rootPartition);

		generateCorridors(this);
				
	}

	BspPartition* Dungeon::getRandomRoom() {
		BspPartition* room = NULL;
		do {
			int roomIndex = rand() % partitionList.size();
			if ((partitionList[roomIndex]->hasRoom) && (partitionList[roomIndex]->children.empty())) {
				room = partitionList[roomIndex];
			}
		} while (room == NULL);

		return room;
	}

	void Dungeon::generateWalls() {
		for (int x = 0; x < width; ++x) {
			for (int y = 0; y < height; ++y) {
				if (cells[x][y]->type != Cell::cellTypeEmpty) {
					Cell *curCell = cells[x][y];
					// To the west
					if (x == 0) {
						curCell->walls[Cell::dirWest] = true;
					}
					else {
						if (cells[x - 1][y]->type == Cell::cellTypeEmpty) {
							curCell->walls[Cell::dirWest] = true;
						}
					}

					// To the east
					if (x == width - 1) {
						curCell->walls[Cell::dirEast] = true;
					}
					else {
						if (cells[x + 1][y]->type == Cell::cellTypeEmpty) {
							curCell->walls[Cell::dirEast] = true;
						}
					}

					// To the north
					if (y == 0) {
						curCell->walls[Cell::dirNorth] = true;
					}
					else {
						if (cells[x][y - 1]->type == Cell::cellTypeEmpty) {
							curCell->walls[Cell::dirNorth] = true;
						}
					}

					// To the south
					if (y == height - 1) {
						curCell->walls[Cell::dirSouth] = true;
					}
					else {
						if (cells[x][y + 1]->type == Cell::cellTypeEmpty) {
							curCell->walls[Cell::dirSouth] = true;
						}
					}
				}
			}
		}
	}

	void Dungeon::generateDoors() {
		for (int x = 1; x < width-1; ++x) {
			for (int y = 1; y < height-1; ++y) {
				Cell *curCell = cells[x][y];
				// TODO : Code doesnt' take corridors alongside 
				if (curCell->type == Cell::cellTypeCorridor) {

					// Check if cell has at least two opposite walls to each other (east and west or south and north)
					// Then check against neighbors. If neighbor cell has less than two walls, a corridor usually ends into a room and a door needs to be placed

					if ((curCell->walls[Cell::dirWest]) && (curCell->walls[Cell::dirEast])) {
						if ((cells[x][y - 1]->type != Cell::cellTypeEmpty) && (cells[x][y - 1]->type != Cell::cellTypeCorridor))
							curCell->doors[Cell::dirNorth] = true;
						if ((cells[x][y + 1]->type != Cell::cellTypeEmpty) && (cells[x][y + 1]->type != Cell::cellTypeCorridor))
							curCell->doors[Cell::dirSouth] = true;
					}

					if ((curCell->walls[Cell::dirNorth]) && (curCell->walls[Cell::dirSouth])) {
						if ((cells[x - 1][y]->type != Cell::cellTypeEmpty) && (cells[x - 1][y]->type != Cell::cellTypeCorridor))
							curCell->doors[Cell::dirWest] = true;
						if ((cells[x + 1][y]->type != Cell::cellTypeEmpty) && (cells[x + 1][y]->type != Cell::cellTypeCorridor))
							curCell->doors[Cell::dirEast] = true;
					}

				}
				for (auto d : curCell->doors) {
					if (d) {
						curCell->hasDoor = true;
					}
				}
			}
		}
	}

}
