#pragma once

#include "Components/NetComponent.hpp"
#include "Components/SpriteComponent.hpp"
#include "Components/LightComponent.hpp"
#include "Scene/BaseEntity.hpp"
#include "Scene/IEntityEvent.hpp"

struct CastleEntity;
struct ObstacleComponent;
struct TeamComponent;

enum class TowerEntityAiState { DoNothing, SearchForTarget, AttackSelectedTarget };
struct TowerEntityState
{
    TowerEntityAiState state;

    EntityReference<Entity> newTarget = EntityReference<Entity>::Invalid();
};

DEFINE_EVENT(TowerDestroyedEvent)
{
    TowerDestroyedEvent()
    {
    }
};

DEFINE_ENTITY(TowerEntity, "tower")
{
    void OnAdded() override;
    void OnDestroyed() override;
    void Update(float deltaTime) override;

    void ReceiveEvent(const IEntityEvent & ev) override;
    void DoSerialize(EntitySerializer & serializer) override;

    void ChangeState(TowerEntityState state);

    int playerId;
    float reach = 250.0f;
    float FireballTimeoutLength = 0.75f;

    TeamComponent* team;
    ObstacleComponent* obstacle;

private:
    float _colorChangeTime = 0;
    SpriteComponent* spriteComponent;

    LightComponent<PointLight>* _light;

    EntityReference<Entity> _currentTarget;
    FixedSizeVector<Entity*, 32> _targets;

    void OnEnterState(TowerEntityState & newState);
    void OnUpdateState();
    void OnExitState();

    TowerEntityState _state{ TowerEntityAiState::DoNothing };

    void ShootFireball(Entity * target);
    float _fireballTimeout = 0.75f;

    b2Fixture* region = nullptr;
};