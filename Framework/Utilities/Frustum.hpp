#pragma once

#include <array>
#include <cmath>
#include <glm/glm.hpp>

class Frustum
{
public:
    enum side
    {
        LEFT = 0,
        RIGHT = 1,
        TOP = 2,
        BOTTOM = 3,
        BACK = 4,
        FRONT = 5
    };
    std::array<glm::vec4, 6> mPlanes;

public:
    void Update(glm::mat4 matrix)
    {
        mPlanes[LEFT].x = matrix[0].w + matrix[0].x;
        mPlanes[LEFT].y = matrix[1].w + matrix[1].x;
        mPlanes[LEFT].z = matrix[2].w + matrix[2].x;
        mPlanes[LEFT].w = matrix[3].w + matrix[3].x;

        mPlanes[RIGHT].x = matrix[0].w - matrix[0].x;
        mPlanes[RIGHT].y = matrix[1].w - matrix[1].x;
        mPlanes[RIGHT].z = matrix[2].w - matrix[2].x;
        mPlanes[RIGHT].w = matrix[3].w - matrix[3].x;

        mPlanes[TOP].x = matrix[0].w - matrix[0].y;
        mPlanes[TOP].y = matrix[1].w - matrix[1].y;
        mPlanes[TOP].z = matrix[2].w - matrix[2].y;
        mPlanes[TOP].w = matrix[3].w - matrix[3].y;

        mPlanes[BOTTOM].x = matrix[0].w + matrix[0].y;
        mPlanes[BOTTOM].y = matrix[1].w + matrix[1].y;
        mPlanes[BOTTOM].z = matrix[2].w + matrix[2].y;
        mPlanes[BOTTOM].w = matrix[3].w + matrix[3].y;

        mPlanes[BACK].x = matrix[0].w + matrix[0].z;
        mPlanes[BACK].y = matrix[1].w + matrix[1].z;
        mPlanes[BACK].z = matrix[2].w + matrix[2].z;
        mPlanes[BACK].w = matrix[3].w + matrix[3].z;

        mPlanes[FRONT].x = matrix[0].w - matrix[0].z;
        mPlanes[FRONT].y = matrix[1].w - matrix[1].z;
        mPlanes[FRONT].z = matrix[2].w - matrix[2].z;
        mPlanes[FRONT].w = matrix[3].w - matrix[3].z;

        for (auto i = 0; i < mPlanes.size(); i++)
        {
            float length = sqrtf(mPlanes[i].x * mPlanes[i].x + mPlanes[i].y * mPlanes[i].y + mPlanes[i].z * mPlanes[i].z);
            mPlanes[i] /= length;
        }
    }

    bool CheckSphere(glm::vec3 pos, float radius)
    {
        for (auto i = 0; i < mPlanes.size(); i++)
        {
            if ((mPlanes[i].x * pos.x) + (mPlanes[i].y * pos.y) + (mPlanes[i].z * pos.z) + mPlanes[i].w <= -radius)
            {
                return false;
            }
        }
        return true;
    }
};
