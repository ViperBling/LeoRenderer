#include "Camera.hpp"

void Camera::SetPerspective(float fov, float aspect, float zNear, float zFar) {

    mFov = fov;
    mZNear = zNear;
    mZFar = zFar;
    mMatrices.mPerspective = glm::perspective(glm::radians(fov), aspect, zNear, zFar);

    if (mbFlipY) mMatrices.mPerspective[1][1] *= -1.0f;
}

void Camera::UpdateAspectRatio(float aspect) {

    mMatrices.mPerspective = glm::perspective(glm::radians(mFov), aspect, mZNear, mZFar);
    if (mbFlipY) mMatrices.mPerspective[1][1] *= -1.0f;
}

void Camera::Update(float deltaTime) {

    mbUpdated = false;
    if (mType == FirstPerson)
    {
        if (Moving())
        {
            glm::vec3 camFront;
            camFront.x = -cos(glm::radians(mRotation.x)) * sin(glm::radians(mRotation.y));
            camFront.y = sin(glm::radians(mRotation.x));
            camFront.z = cos(glm::radians(mRotation.x)) * cos(glm::radians(mRotation.y));
            camFront = glm::normalize(camFront);
            float moveSpeed = deltaTime * mMovementSpeed;

            if (mKeys.mUp) mPosition += camFront * moveSpeed;
            if (mKeys.mDown) mPosition -= camFront * moveSpeed;
            if (mKeys.mLeft) mPosition -= glm::normalize(glm::cross(camFront, glm::vec3(0.0f, 1.0f, 0.0f))) * moveSpeed;
            if (mKeys.mRight) mPosition += glm::normalize(glm::cross(camFront, glm::vec3(0.0f, 1.0f, 0.0f))) * moveSpeed;
        }
    }
    updateViewMatrix();
}

void Camera::updateViewMatrix() {

    glm::mat4 rotM = glm::mat4(1.0f);
    glm::mat4 transM;

    rotM = glm::rotate(rotM, glm::radians(mRotation.x * (mbFlipY ? -1.0f : 1.0f)), glm::vec3(1.0f, 0.0f, 0.0f));
    rotM = glm::rotate(rotM, glm::radians(mRotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
    rotM = glm::rotate(rotM, glm::radians(mRotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

    glm::vec3 translation = mPosition;
    if (mbFlipY) translation.y *= -1.0f;
    transM = glm::translate(glm::mat4(1.0f), translation);

    if (mType == FirstPerson) mMatrices.mView = rotM * transM;
    else mMatrices.mView = transM * rotM;
    mViewPos = glm::vec4(mPosition, 0.0f) * glm::vec4(-1.0f, 1.0f, -1.0f, 1.0f);

    mbUpdated = true;
}
