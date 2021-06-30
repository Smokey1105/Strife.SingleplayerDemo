#pragma once

#include "Scene/EntityComponent.hpp"
#include "Math/Rectangle.hpp"

DEFINE_COMPONENT(ObstacleComponent)
{
    //ObstacleComponent(RigidBodyComponent* rb);

    void OnAdded() override;
    void OnRemoved() override;

    Rectangle bounds;
};
