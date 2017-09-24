/*
* Random dungeon generator
*
* Copyright (C) 2017 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#include "Cell.h"
#include <glm/gtc/matrix_transform.hpp>

namespace dungeongenerator {

	Cell::Cell(int posX, int posY)
	{
		x = posX;
		y = posY;
		type = cellTypeEmpty;
		walls = { false, false, false, false };
		doors = { false, false, false, false };
		modelMatrix = glm::mat4();
		modelMatrix = glm::translate(modelMatrix, glm::vec3((float)x, 0, (float)y));
	}

	Cell::~Cell()
	{
	}

}
