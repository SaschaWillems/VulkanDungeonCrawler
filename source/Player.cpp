/*
* Player class
*
* Copyright (C) 2017 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#include "Player.h"

Player::Player()
{
	freeLookDelta = glm::vec2(0.0f);
	freeLookRotation = glm::vec3(0.0f);
}


Player::~Player()
{
}

void Player::setPerspective(float fov, float aspect, float znear, float zfar)
{
	this->fov = fov;
	this->znear = znear;
	this->zfar = zfar;
	matrices.projection = glm::perspective(glm::radians(fov), aspect, znear, zfar);
}

void Player::setPosition(glm::vec3 position)
{
	this->position = position;
	this->targetPosition = position;
	updateViewMatrix();
}

void Player::setRotation(glm::vec3 rotation)
{
	this->rotation = rotation;
	this->targetRotation = rotation.y;
	this->freeLookDelta = glm::vec2(0.0f);
	this->freeLookRotation = glm::vec3(0.0f);
	updateViewMatrix();
}

void Player::setDungeon(dungeongenerator::Dungeon *dungeon)
{
	this->dungeon = dungeon;
}

bool Player::move(glm::vec3 dirVec, bool animate) {
	glm::vec3 movementVector = dirVec;
	movementVector = glm::rotate(movementVector, glm::radians(rotation.y), glm::vec3(0.0, 1.0f, 0.0f));
	dungeongenerator::Cell *targetCell = dungeon->getCell(round(position.x + movementVector.x), round(position.z - movementVector.z));
	if (targetCell->type == dungeongenerator::Cell::cellTypeEmpty) {
		// TODO : Blocking animation
		return false;
	}
	if (!animate) {
		position.x += movementVector.x;
		position.z -= movementVector.z;
		targetPosition = position;
		updateViewMatrix();
	}
	else {
		dungeongenerator::Cell *targetCell = dungeon->getCell(round(position.x + movementVector.x), round(position.z - movementVector.z));
		double distance = glm::distance(position, targetPosition);
		if (distance == 0.0f) {
			targetPosition.x = position.x + movementVector.x;
			targetPosition.z = position.z - movementVector.z;
			return true;
		}
	}

	return true;
	// TODO: Return false if blocked
}

void Player::rotate(float dir, bool animate) {
	if (!animate) {
		rotation.y += dir;
		if (rotation.y < 0) {
			rotation.y += 360;
		}
		if (rotation.y >= 360) {
			rotation.y -= 360;
		}
		updateViewMatrix();
	}
	else {
		if (rotationDir == 0.0f) {
			rotationDir = (dir < 0) ? -1.0f : 1.0f;
			targetRotation = rotation.y + dir;
			animRotation = abs(dir);
		}
	}
}

bool Player::updateFreeLook(float timeFactor) {
	float freeLookSpeed = 65.0f;
	float freeLookRebound = 0.25f;
	bool viewChange = false;
	glm::vec3 freeLookLimits = glm::vec3(7.5f, 7.5f, 0.0f);
	if (freeLook) {
		freeLookRotation.y += freeLookDelta.x * timeFactor * freeLookSpeed;
		freeLookRotation.x += freeLookDelta.y * timeFactor * freeLookSpeed;
		freeLookRotation.x = glm::clamp(freeLookRotation.x, -freeLookLimits.x, freeLookLimits.x);
		freeLookRotation.y = glm::clamp(freeLookRotation.y, -freeLookLimits.y, freeLookLimits.y);
		viewChange = true;
	}
	else {
		if (freeLookRotation.x > 0.0f) {
			freeLookRotation.x -= timeFactor * freeLookSpeed * freeLookRebound;
			if (freeLookRotation.x < 0.0f) {
				freeLookRotation.x = 0.0;
			}
			viewChange = true;
		}
		if (freeLookRotation.x < 0.0f) {
			freeLookRotation.x += timeFactor * freeLookSpeed * freeLookRebound;
			if (freeLookRotation.x > 0.0f) {
				freeLookRotation.x = 0.0;
			}
			viewChange = true;
		}
		if (freeLookRotation.y > 0.0f) {
			freeLookRotation.y -= timeFactor * freeLookSpeed * freeLookRebound;
			if (freeLookRotation.y < 0.0f) {
				freeLookRotation.y = 0.0;
			}
			viewChange = true;
		}
		if (freeLookRotation.y < 0.0f) {
			freeLookRotation.y += timeFactor * freeLookSpeed * freeLookRebound;
			if (freeLookRotation.y > 0.0f) {
				freeLookRotation.y = 0.0;
			}
			viewChange = true;
		}
	}
	return viewChange;
}

void Player::updateMovement(float timeFactor) {
	glm::vec3 distVec = glm::normalize(position - targetPosition);
	double distance = glm::distance(position, targetPosition);
	if (distance > 0.0f) {
		position -= glm::normalize(distVec) * timeFactor;
		if (distance <= timeFactor) {
			position = targetPosition;
		};
	};
}

void Player::updateRotation(float timeFactor) {
	if (rotationDir != 0.0f) {
		float rotationFactor = rotationDir * timeFactor * 65.0f;
		rotation.y += rotationFactor;
		animRotation -= abs(rotationFactor);
		if (animRotation <= 0.0f) {
			rotation.y = targetRotation;
			rotationDir = 0.0f;
		}
	}
}

void Player::updateViewMatrix() {
	glm::mat4 rotM = glm::mat4(1.0f);
	glm::mat4 transM;
	rotM = glm::rotate(rotM, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
	rotM = glm::rotate(rotM, glm::radians(freeLookRotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
	rotM = glm::rotate(rotM, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
	rotM = glm::rotate(rotM, glm::radians(freeLookRotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
	rotM = glm::rotate(rotM, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
	transM = glm::translate(glm::mat4(1.0f), position * glm::vec3(-1.0f, 1.0f, -1.0f));
	matrices.view = rotM * transM;
}

bool Player::update(float timeFactor) {
	bool viewChange = false;
	viewChange |= updateFreeLook(timeFactor);
	updateMovement(timeFactor);
	updateRotation(timeFactor);
	if (viewChange) {
		updateViewMatrix();
	}
	return viewChange;
}

void Player::setFreeLookDelta(glm::vec2 delta) {
	freeLookDelta = delta;
}
