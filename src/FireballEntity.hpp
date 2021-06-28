#pragma once

#include "Scene/BaseEntity.hpp"

DEFINE_ENTITY(FireballEntity, "fireball")
{
    static constexpr float Radius = 10;

    FireballEntity() = default;

    FireballEntity(Vector2 velocity)
        : velocity(velocity)
    {

    }

    void Render(Renderer* renderer) override;
    void OnAdded() override;
    void OnDestroyed() override;
    void ReceiveEvent(const IEntityEvent& ev) override;
    void Update(float deltaTime) override;

    PointLight light;
    int ownerId;
    Vector2 velocity;
    int playerId;
};