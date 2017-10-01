/*
* View frustum culling class
*
* Copyright (C) 2016 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#include <array>
#include <math.h>
#include <glm/glm.hpp>

namespace vks
{
	class Frustum
	{
	public:
		enum side { LEFT = 0, RIGHT = 1, TOP = 2, BOTTOM = 3, BACK = 4, FRONT = 5 };
		std::array<glm::vec4, 6> planes;

		void update(glm::mat4 matrix)
		{
			planes[LEFT].x = matrix[0].w + matrix[0].x;
			planes[LEFT].y = matrix[1].w + matrix[1].x;
			planes[LEFT].z = matrix[2].w + matrix[2].x;
			planes[LEFT].w = matrix[3].w + matrix[3].x;

			planes[RIGHT].x = matrix[0].w - matrix[0].x;
			planes[RIGHT].y = matrix[1].w - matrix[1].x;
			planes[RIGHT].z = matrix[2].w - matrix[2].x;
			planes[RIGHT].w = matrix[3].w - matrix[3].x;

			planes[TOP].x = matrix[0].w - matrix[0].y;
			planes[TOP].y = matrix[1].w - matrix[1].y;
			planes[TOP].z = matrix[2].w - matrix[2].y;
			planes[TOP].w = matrix[3].w - matrix[3].y;

			planes[BOTTOM].x = matrix[0].w + matrix[0].y;
			planes[BOTTOM].y = matrix[1].w + matrix[1].y;
			planes[BOTTOM].z = matrix[2].w + matrix[2].y;
			planes[BOTTOM].w = matrix[3].w + matrix[3].y;

			planes[BACK].x = matrix[0].w + matrix[0].z;
			planes[BACK].y = matrix[1].w + matrix[1].z;
			planes[BACK].z = matrix[2].w + matrix[2].z;
			planes[BACK].w = matrix[3].w + matrix[3].z;

			planes[FRONT].x = matrix[0].w - matrix[0].z;
			planes[FRONT].y = matrix[1].w - matrix[1].z;
			planes[FRONT].z = matrix[2].w - matrix[2].z;
			planes[FRONT].w = matrix[3].w - matrix[3].z;

			for (auto i = 0; i < planes.size(); i++)
			{
				float length = sqrtf(planes[i].x * planes[i].x + planes[i].y * planes[i].y + planes[i].z * planes[i].z);
				planes[i] /= length;
			}
		}
		
		bool checkSphere(glm::vec3 pos, float radius)
		{
			for (auto i = 0; i < planes.size(); i++)
			{
				if ((planes[i].x * pos.x) + (planes[i].y * pos.y) + (planes[i].z * pos.z) + planes[i].w <= -radius)
				{
					return false;
				}
			}
			return true;
		}

		uint32_t halfPlaneTest(const glm::vec3 &p, const glm::vec3 &normal, float offset) const {
			float dist = glm::dot(p, normal) + offset;
			if (dist > 0.02) {
				return 1;
			}
			else if (dist < -0.02) {
				return 0;
			}
			return 2;
		}

		inline int vectorToIndex(const glm::vec3 &v) const {
			int idx = 0;
			if (v.z >= 0) idx |= 1;
			if (v.y >= 0) idx |= 2;
			if (v.x >= 0) idx |= 4;
			return idx;
		}

		uint32_t checkBox(const glm::vec3 &origin, const glm::vec3 &halfDim) const {
			static const glm::vec3 cornerOffsets[] = {
				glm::vec3(-1.f,-1.f,-1.f), glm::vec3(-1.f,-1.f, 1.f), glm::vec3(-1.f, 1.f,-1.f), glm::vec3(-1.f, 1.f, 1.f),
				glm::vec3( 1.f,-1.f,-1.f), glm::vec3( 1.f,-1.f, 1.f), glm::vec3( 1.f, 1.f,-1.f), glm::vec3( 1.f, 1.f, 1.f)
			};
			uint32_t ret = 1;
			for (uint32_t i = 0; i<6; i++) {
				glm::vec3 planeNormal = glm::vec3(planes[i]);
				uint32_t idx = vectorToIndex(planeNormal);
				glm::vec3 testPoint = origin + halfDim * cornerOffsets[idx];
				if (halfPlaneTest(testPoint, planeNormal, planes[i].w) == 0) {
					ret = 0;
					break;
				}
				idx = vectorToIndex(-planeNormal);
				testPoint = origin + halfDim * cornerOffsets[idx];
				if (halfPlaneTest(testPoint, planeNormal, planes[i].w) == 0) {
					ret |= 2;
				}
			}
			return ret;
		}

	};
}
