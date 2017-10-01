/*
* Random dungeon generator
*
* Copyright (C) 2017 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#pragma once

#include <vector>
#include "vulkan/vulkan.h"
#include <glm\glm.hpp>

namespace dungeongenerator {

	class Cell
	{
	public:
		static const int cellTypeEmpty = 0;
		static const int cellTypeCorridor = 1;
		static const int cellTypeRoom = 2;
		static const int dirNorth = 0;
		static const int dirSouth = 1;
		static const int dirWest= 2;
		static const int dirEast = 3;
		int x;
		int y;
		int type;
		bool hasDoor = false;
		VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
		glm::mat4 modelMatrix;
		std::vector<bool> walls;
		std::vector<bool> doors;
		Cell(int posX, int posY);
		~Cell();
	};

}

