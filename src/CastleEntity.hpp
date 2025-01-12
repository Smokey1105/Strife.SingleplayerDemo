#pragma once

#include "Components/NetComponent.hpp"
#include "Components/SpriteComponent.hpp"
#include "Components/LightComponent.hpp"
#include "Scene/BaseEntity.hpp"
#include "Scene/IEntityEvent.hpp"
#include "TowerEntity.hpp"
#include "MinionEntity.hpp"
#include "TeamComponent.hpp"

DEFINE_ENTITY(CastleEntity, "castle")
{
    void OnAdded() override;
    void OnDestroyed() override;
    void Update(float deltaTime) override;
    void SpawnPlayer();

    void ReceiveEvent(const IEntityEvent& ev) override;
    void DoSerialize(EntitySerializer& serializer) override;

    int playerId;
    TowerEntity* tower;
    MinionSpawner* minionSpawner;

    TeamComponent* team;

private:
    float _colorChangeTime = 0;
    SpriteComponent* spriteComponent;

    Vector2 _spawnSlots[2];
    int _nextSpawnSlotId = 0;

    LightComponent<PointLight>* _light;
};