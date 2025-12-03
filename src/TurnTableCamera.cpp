#include "TurnTableCamera.h"

TurnTableCamera::TurnTableCamera(const TurnTableCameraParams& params) {
    _target = params.target;
    _worldUp = glm::normalize(params.worldUp);

    _radius = params.initialRadius;
    _minRadius = params.minRadius;
    _maxRadius = params.maxRadius;

    _azimuth = params.initialAzimuth;

    _elevation = params.initialElevation;
    _minElevation = params.minElevation;
    _maxElevation = params.maxElevation;

    updateCameraVectors();
}

void TurnTableCamera::rotateHorizontally(float delta) {
    // Turntable behavior: always rotate around world up axis
    _azimuth += delta;
    updateCameraVectors();
}

void TurnTableCamera::rotateVertically(float delta) {
    // Turntable behavior: constrain elevation to prevent flipping
    _elevation = glm::clamp(_elevation + delta, _minElevation, _maxElevation);
    updateCameraVectors();
}

void TurnTableCamera::changeZoom(float delta) {
    _radius = glm::clamp(_radius + delta, _minRadius, _maxRadius);
    updateCameraVectors();
}

void TurnTableCamera::updateCameraVectors() {
    // Compute camera position from spherical coordinates
    // Azimuth rotates around world up (Y), elevation tilts up/down
    float cosElevation = cos(_elevation);
    _forward.x = sin(_azimuth) * cosElevation;
    _forward.y = sin(_elevation);
    _forward.z = cos(_azimuth) * cosElevation;
    _forward = glm::normalize(_forward);

    // Compute left and up vectors using world up as reference
    _left = glm::normalize(glm::cross(_worldUp, _forward));
    _up = glm::normalize(glm::cross(_forward, _left));

    _viewMatrix = glm::lookAt(_target + _radius * -1.f * _forward, _target, _worldUp);
}