/*
* Random dungeon generator
*
* Copyright (C) 2017 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#include "BspPartition.h"
#include <vector>
#include <stdio.h>

namespace dungeongenerator {

	BspPartition::BspPartition(BspPartition* parent, int left, int top, int right, int bottom, int splitIn, int depth)
	{
		this->parent = parent;
		this->left = left;
		this->right = right;
		this->top = top;
		this->bottom = bottom;
		this->splitIn = splitIn;
		this->depth = depth;

		this->centerX = round(left + (float)(right - left) / 2.0f);
		this->centerY = round(top + (float)(bottom - top) / 2.0f);
		
		// Don't split if any of the dimensions is below certain threshold
		if ((right - left <= 16) || (bottom - top <= 16)) {
			return;
		}

		// Randomly stop splitting
		if (splitIn == BspPartitionSplitVertical) {
			if ((right - left < 4) || (bottom - top < 4)) {
				if ((rand() % 100 < 25 /*this.dungeon.splitStop*/) && (depth > 0)) {
					return;
				}
			}
		}

		// Split
		int splitRangeX = round((float)(right - left) / ((depth == 0) ? 8 : 4));
		int splitRangeY = round((float)(bottom - top) / ((depth == 0) ? 8 : 4));

		int splitX = round(left + ((right - left) / 2) + round(rand() % splitRangeX) - round(rand() % splitRangeX));
		int splitY = round(top + ((bottom - top) / 2) + round(rand() % splitRangeY) - round(rand() % splitRangeY));

		// Add four new child partitions
		children.push_back(new BspPartition(this, left, top, splitX, splitY, rand() % 2, depth + 1));
		children.push_back(new BspPartition(this, splitX, top, right, splitY, rand() % 2, depth + 1));
		children.push_back(new BspPartition(this, left, splitY, splitX, bottom, rand() % 2, depth + 1));
		children.push_back(new BspPartition(this, splitX, splitY, right, bottom, rand() % 2, depth + 1));

	}


	BspPartition::~BspPartition()
	{
	}

	void BspPartition::split(bool origin, int maxDivision, int minDivision) 
	{

		// Don't split if any of the dimensions is below certain threshold
		if ( (right - left <= maxDivision) || (bottom - top <= maxDivision) ) {
			return;
		}

		// Randomly stop splitting
		if (splitIn == BspPartitionSplitVertical) {
			if ((right - left < minDivision) || (bottom - top < minDivision)) {
				if ((rand() % 100 < 25 /*this.dungeon.splitStop*/) && (!origin)) {
					return;
				}
			}
		}

		// Split
		int splitRangeX = round((float)(right - left) / ((origin) ? 8 : 4));
		int splitRangeY = round((float)(bottom - top) / ((origin) ? 8 : 4));

		int splitX = round(left + ((right - left) / 2) + round(rand() % splitRangeX) - round(rand() % splitRangeX));
		int splitY = round(top + ((bottom - top) / 2) + round(rand() % splitRangeY) - round(rand() % splitRangeY));

		// Add four new child partitions

		BspPartition child0 = BspPartition(this, left, top, splitX, splitY, rand() % 2, depth + 1);
		BspPartition child1 = BspPartition(this, splitX, top, right, splitY, rand() % 2, depth + 1);
		BspPartition child2 = BspPartition(this, left, splitY, splitX, bottom, rand() % 2, depth + 1);
		BspPartition child3 = BspPartition(this, splitX, splitY, right, bottom, rand() % 2, depth + 1);

		child0.split(false, 12, 24);
		child1.split(false, 12, 24);
		child2.split(false, 12, 24);
		child3.split(false, 12, 24);

		children.push_back(&child0);
		children.push_back(&child1);
		children.push_back(&child2);
		children.push_back(&child3);

		//for (auto child : children) {
		//	child.split(false, 16, 32);
		//}
	}

	void BspPartition::connect()
	{
		std::vector<BspPartition*> connectionList;

		// Add child nodes with rooms to connection target list
		if (!children.empty()) {
			for (auto child : children) {
				if (child->hasRoom) {
					connectionList.push_back(child);
				}
			}
		}

		// Add this node (and if set it's parent) to the connection list
		connectionList.push_back(this);
		if (parent != NULL) {
			connectionList.push_back(parent);
		}
		fprintf(stdout, "%d\n", connectionList.size());

		if (connectionList.size() == 0) {
			return;
		}

		// Randomize connection list
		// TODO : Not the pretiest code...
		bool skip = false;
		int index = 0;
		std::vector<BspPartition*> randomList;

		//do {
		//	index = rand() %  connectionList.size(); 
		//	fprintf(stdout, "%d - %d / %d\n", index, randomList.size(), connectionList.size());
		//	if (std::find(randomList.begin(), randomList.end(), connectionList[index]) == randomList.end()) {
		//		randomList.push_back(connectionList[index]);
		//	}
		//} 
		//while (randomList.size() != connectionList.size());

		for (int i = 0; i < connectionList.size(); i++) {
			randomList.push_back(connectionList[i]);
		}

		for (auto partition : randomList) {

		}


		//if (randomList.length > 0) {
		//	for (var i = 0; i < randomList.length - 1; i++) {
		//		srcX = connectionList[randomList[i]].centerX;
		//		srcY = connectionList[randomList[i]].centerY;
		//		dstX = connectionList[randomList[i + 1]].centerX;
		//		dstY = connectionList[randomList[i + 1]].centerY;
		//		this.createCorridor(srcX, srcY, dstX, dstY);
		//	}
		//}

	}

}
