#pragma once

#include <iostream>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

enum CameraType
{
    LookAt,
    FirstPerson
};

struct CameraMat
{
    glm::mat4 mPerspective;
    glm::mat4 mView;
};

struct Key
{
    bool mLeft = false;
    bool mRight = false;
    bool mUp = false;
    bool mDown = false;
};

class Camera
{
public:
    void SetPerspective(float fov, float aspect, float zNear, float zFar);
    void UpdateAspectRatio(float aspect);

    bool Moving() const { return mKeys.mLeft || mKeys.mRight || mKeys.mUp || mKeys.mDown; }
    float GetNearClip() const { return mZNear; }
    float GetFarClip() const { return mZFar; }
    void SetNearClip(float nearClip) { mZNear = nearClip; }
    void SetFarClip(float farClip) { mZFar = farClip; }
    void SetPosition(glm::vec3 position) { mPosition = position; updateViewMatrix(); }
    void SetRotation(glm::vec3 rotation) { mRotation = rotation; updateViewMatrix(); }
    void Rotate(glm::vec3 delta) { mRotation += delta; updateViewMatrix(); }
    void SetTranslation(glm::vec3 translation) { mPosition = translation; updateViewMatrix(); }
    void Translate(glm::vec3 delta) { mPosition += delta; updateViewMatrix(); }
    void SetRotationSpeed(float rotationSpeed) { mRotationSpeed = rotationSpeed; }
    void SetMovementSpeed(float movementSpeed) { mMovementSpeed = movementSpeed; }

    void Update(float deltaTime);

private:
    void updateViewMatrix();

public:
    CameraType mType = CameraType::LookAt;

    glm::vec3 mRotation = glm::vec3();
    glm::vec3 mPosition = glm::vec3();
    glm::vec4 mViewPos = glm::vec4();

    float mRotationSpeed = 1.0f;
    float mMovementSpeed = 1.0f;

    bool mbUpdated = false;
    bool mbFlipY = false;

    CameraMat mMatrices;
    Key mKeys;

private:
    float mFov;
    float mZNear;
    float mZFar;
};