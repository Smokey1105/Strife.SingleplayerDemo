#include "TowerEntity.hpp"

#include "Engine.hpp"
#include "PlayerEntity.hpp"
#include "FireballEntity.hpp"
#include "CastleEntity.hpp"
#include "ObstacleComponent.hpp"
#include "Components/RigidBodyComponent.hpp"
#include "Components/SpriteComponent.hpp"
#include "Physics/PathFinding.hpp"
#include "Net/ReplicationManager.hpp"

void TowerEntity::DoSerialize(EntitySerializer& serializer)
{

}

void TowerEntity::OnAdded()
{
    spriteComponent = AddComponent<SpriteComponent>("towerSprite");
    spriteComponent->scale = Vector2(5.0f);

    Vector2 size{ 11 * 5, 32 * 5 };
    SetDimensions(size);
    obstacle = AddComponent<ObstacleComponent>();

    auto rigidBody = AddComponent<RigidBodyComponent>(b2_staticBody);
    rigidBody->CreateBoxCollider(size);

    auto health = AddComponent<HealthBarComponent>();
    health->offsetFromCenter = -size.YVector() / 2 - Vector2(0, 5);
    health->maxHealth = 500;
    health->health = 500;

    team = AddComponent<TeamComponent>();

    auto offset = size / 2 + Vector2(40, 40);

    _light = AddComponent<LightComponent<PointLight>>();
    _light->position = Center();
    _light->intensity = 0.5;
    _light->maxDistance = 500;
    _light->maxDistance = 500;

    region = rigidBody->CreateCircleCollider(reach, true);
}

void TowerEntity::Update(float deltaTime)
{
    _light->color = playerId == 0
        ? Color::Green()
        : Color::White();

    if (_currentTarget.IsValid())
    {
        _fireballTimeout -= deltaTime;
    }

    OnUpdateState();
}

void TowerEntity::OnDestroyed()
{
    for (auto player : scene->GetEntitiesOfType<PlayerEntity>())
    {
        if (player->playerId == playerId)
        {
            player->Destroy();
        }
    }

    auto castleHealth = castle->AddComponent<HealthBarComponent>();
    castleHealth->offsetFromCenter = -Vector2(67 * 5, 55 * 5).YVector() / 2 - Vector2(0, 5);
    castleHealth->maxHealth = 1000;
    castleHealth->health = 1000;
}

void TowerEntity::ReceiveEvent(const IEntityEvent& ev)
{
    if (ev.Is<OutOfHealthEvent>())
    {
        Destroy();
    }
    else if (auto damageDealtEvent = ev.Is<DamageDealtEvent>())
    {
        Entity* currentTarget = nullptr;
        if (_currentTarget.TryGetValue(currentTarget))
        {
            if (currentTarget != damageDealtEvent->dealer)
            {
                ChangeState(
                    { TowerEntityAiState::AttackSelectedTarget,
                     EntityReference<Entity>(damageDealtEvent->dealer) });
            }
        }
    }
    else if (auto contactBeginEvent = ev.Is<ContactBeginEvent>())
    {
        if (contactBeginEvent->OtherIs<FireballEntity>())
        {
            return;
        }

        if (contactBeginEvent->self.GetFixture() == region && !contactBeginEvent->other.IsTrigger())
        {
            _targets.PushBackUniqueIfRoom(contactBeginEvent->other.OwningEntity());
        }
    }
    else if (auto contactEndEvent = ev.Is<ContactEndEvent>())
    {
        auto other = contactEndEvent->other;
        if (contactEndEvent->self.GetFixture() == region && !other.IsTrigger())
        {
            _targets.RemoveSingle(other.OwningEntity());
        }
    }
}

void TowerEntity::ChangeState(TowerEntityState state)
{
    OnExitState();
    OnEnterState(state);

    _state = state;
}

void TowerEntity::OnEnterState(TowerEntityState& newState)
{
    switch (newState.state)
    {
    case TowerEntityAiState::DoNothing:
    case TowerEntityAiState::SearchForTarget:
        break;
    case TowerEntityAiState::AttackSelectedTarget:
    {
        _currentTarget = newState.newTarget;
    }
    break;
    }
}

void TowerEntity::OnUpdateState()
{
    switch (_state.state)
    {
    case TowerEntityAiState::DoNothing:
        ChangeState({ TowerEntityAiState::SearchForTarget });
        break;
    case TowerEntityAiState::SearchForTarget:
    {
        float closestTargetDistance = reach * 2;
        Entity* closestTarget = nullptr;

        for (auto& target : _targets)
        {
            if (target == nullptr || target->isDestroyed)
            {
                continue;
            }

            TeamComponent* teamComponent;
            if (target->TryGetComponent(teamComponent)) {
                if (teamComponent->teamId == team->teamId)
                {
                    continue;
                }

                if ((target->Center() - Center()).Length() <= closestTargetDistance)
                {
                    closestTarget = target;
                }
            }
        }

        if (closestTarget != nullptr)
        {
            ChangeState({ TowerEntityAiState::AttackSelectedTarget, EntityReference<Entity>(closestTarget) });
        }
    }
    break;
    case TowerEntityAiState::AttackSelectedTarget:
    {
        Entity* target = _currentTarget.GetValueOrNull();
        if (target == nullptr || (target->Center() - Center()).Length() >= reach)
        {
            _currentTarget.Invalidate();
            ChangeState({ TowerEntityAiState::SearchForTarget });
        }
        else if (_fireballTimeout <= 0)
        {
            _fireballTimeout = FireballTimeoutLength;
            ShootFireball(target);
        }
    }
    break;
    }
}

void TowerEntity::OnExitState()
{
    switch (_state.state)
    {
    case TowerEntityAiState::DoNothing:
    case TowerEntityAiState::SearchForTarget:
        break;
    case TowerEntityAiState::AttackSelectedTarget:
    {
        _currentTarget.Invalidate();
    }
    break;
    }
}

void TowerEntity::ShootFireball(Entity* target)
{
    auto direction = (target->Center() - Center()).Normalize();

    auto fireball = scene->CreateEntity<FireballEntity>(Center(), direction * 400);
    fireball->playerId = playerId;
}