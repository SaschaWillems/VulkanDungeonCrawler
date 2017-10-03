/*
* Player class
*
* Copyright (C) 2017 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#pragma once

#include <algorithm>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include "generator\Dungeon.h"

class Player
{
private:
	glm::vec2 freeLookDelta;
	dungeongenerator::Dungeon *dungeon;
	float rotationDir;
	float animRotation;
	float targetRotation;
	bool updateFreeLook(float timeFactor);
	void updateMovement(float timeFactor);
	void updateRotation(float timeFactor);
	void updateViewMatrix();
public:
	struct {
		glm::mat4 projection;
		glm::mat4 view;
	} matrices;

	float fov, znear, zfar;

	glm::vec3 position;
	glm::vec3 rotation;
	glm::vec3 movementVector;
	glm::vec3 targetPosition;
	glm::vec3 freeLookRotation;
	bool freeLook;
	Player();
	~Player();

	void setPerspective(float fov, float aspect, float znear, float zfar);
	void setPosition(glm::vec3 position);
	void setRotation(glm::vec3 rotation);

	void setDungeon(dungeongenerator::Dungeon *dungeon);
	bool move(glm::vec3 dirVec, bool animate);
	void rotate(float dir, bool animate);
	bool update(float timeFactor);
	void setFreeLookDelta(glm::vec2 delta);
};

