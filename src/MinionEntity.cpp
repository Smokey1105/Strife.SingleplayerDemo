#include "Renderer/Renderer.hpp"

#include "Components/SpriteComponent.hpp"
#include "Components/RigidBodyComponent.hpp"
#include "Components/PathFollowerComponent.hpp"

#include "HealthBarComponent.hpp"
#include "CastleEntity.hpp"
#include "PlayerEntity.hpp"
#include "FireballEntity.hpp"

#include "MinionEntity.hpp"
#include "TowerEntity.hpp"

static int MinionsSpawned = 0;

void MinionSpawner::DoSerialize(EntitySerializer& serializer)
{

}

void MinionSpawner::OnAdded()
{
    team = AddComponent<TeamComponent>();
}

void MinionSpawner::FixedUpdate(float deltaTime)
{
    spawnTimeout -= deltaTime;

    if (spawnTimeout <= 0 && MinionsSpawned <= MaxMinions)
    {
        spawnTimeout = 30.0f;
        SpawnMinion();
    }
}

void MinionSpawner::SpawnMinion()
{
    auto minion = scene->CreateEntity<MinionEntity>(Center());

    minion->team->teamId = team->teamId;
    minion->reach = reach;
    minion->AttackTimeoutLength = fireballTimeout;
    minion->engagementRadius = engagementRadius;

    ++MinionsSpawned;
    minion->Start();
}

void MinionEntity::OnAdded()
{
    Vector2 size{ 6 * 5, 6 * 5 };
    SetDimensions(size);

    _rb = AddComponent<RigidBodyComponent>(b2_dynamicBody);
    _rb->CreateBoxCollider(size);

    _pathFollower = AddComponent<PathFollowerComponent>(_rb);
    _healthBar = AddComponent<HealthBarComponent>();

    team = AddComponent<TeamComponent>();

    _healthBar->offsetFromCenter = -Dimensions().YVector() / 2 - Vector2(0, 5);
    _pathFollower->speed = 50;

    _engagementCircle = _rb->CreateCircleCollider(engagementRadius, true);
}

void MinionEntity::Start()
{
    auto bases = scene->GetEntitiesOfType<CastleEntity>();
    for (auto base : bases) if (base->team->teamId != team->teamId) _opponentBase = base;
    ChangeState(MinionAiState::MoveToOpponentBase);
}

void MinionEntity::Render(Renderer* renderer)
{
    Color color = Color::Red();

    if (team->teamId == 0)
    {
        color = Color::Purple();
    }

    renderer->RenderCircleOutline(Center(), engagementRadius, color, -1);
   
    // Debug: shows aggro and melee ranges
    // renderer->RenderCircleOutline(Center(), reach, color, -1);
    // renderer->RenderRectangle(Rectangle(Center() - Dimensions() / 2, Dimensions()), color, -0.99);
}

void MinionEntity::FixedUpdate(float deltaTime)
{
    _attackTimeout -= deltaTime;
    OnUpdateState();
}

static bool EntityIsPotentialTarget(TeamComponent* selfTeam, Entity* entity)
{
    TeamComponent* entityTeam;
    if (!entity->TryGetComponent(entityTeam))
    {
        return false;
    }

    return selfTeam->teamId != entityTeam->teamId && (entity->Is<MinionEntity>()
        || entity->Is<PlayerEntity>()
        || entity->Is<TowerEntity>()
        || entity->Is<CastleEntity>());
}

void MinionEntity::ReceiveEvent(const IEntityEvent& ev)
{
    if (auto event = ev.Is<OutOfHealthEvent>())
    {
        // TODO: Remove this and replace with a more robust system
        --MinionsSpawned;

        // TODO: Award EXP and "Gold" (currency equivalent) here
        Destroy();
    }

    if (auto contactBegin = ev.Is<ContactBeginEvent>())
    {
        auto other = contactBegin->other.OwningEntity();
        if (contactBegin->self == _engagementCircle && !contactBegin->other.IsTrigger() && EntityIsPotentialTarget(team, other))
        {
            _targets.PushBackUniqueIfRoom(EntityReference<Entity>(other));
        }
    }
    else if (auto contactEnd = ev.Is<ContactEndEvent>())
    {
        auto other = contactEnd->other.OwningEntity();
        if (contactEnd->self == _engagementCircle && !contactEnd->other.IsTrigger() && EntityIsPotentialTarget(team, other))
        {
            _targets.RemoveSingle(EntityReference<Entity>(other));
        }
    }
}

void MinionEntity::ChangeState(MinionAiState newState)
{
    OnExitState(state);
    OnEnterState(newState);

    state = newState;
}

void MinionEntity::OnEnterState(MinionAiState stateEntered)
{
    switch (stateEntered)
    {
    case MinionAiState::MoveToOpponentBase:
    {
        CastleEntity* castleEntity;
        if (_opponentBase.TryGetValue(castleEntity))
        {
            _pathFollower->SetTarget(castleEntity->Center());
        }

        break;
    }
    case MinionAiState::DoNothing:
        //_pathFollower->Stop(true);
        //break;
    case MinionAiState::AttackTarget:
        break;
    }
}

static bool IsCloserThan(Entity* lhs, Entity* rhs, Entity* reference, float& outDistance)
{
    if (rhs == nullptr) return true;
    if (lhs == nullptr) return false;

    outDistance = (lhs->Center() - reference->Center()).Length();

    return outDistance < (rhs->Center() - reference->Center()).Length();
}

struct EntityDistanceByType
{
    EntityDistanceByType(StringId type)
        : type(type)
    {

    }

    StringId type;
    Entity* closest = nullptr;
    float distance = INFINITY;
};

void MinionEntity::OnUpdateState()
{
    switch (state)
    {
    case MinionAiState::DoNothing:
    {
        if (_opponentBase.IsValid())
        {
            ChangeState(MinionAiState::MoveToOpponentBase);
        }

        break;
    }
    case MinionAiState::MoveToOpponentBase:
    {
        auto attackTarget = FindTargetOrNull();

        if (attackTarget != nullptr)
        {
            ChangeState(MinionAiState::AttackTarget);
        }

        break;
    }
    case MinionAiState::AttackTarget:
    {
        auto attackTarget = FindTargetOrNull();
        if (attackTarget != nullptr)
        {
            FollowTarget(attackTarget);
            MeleeAttack(attackTarget);
        }
        else
        {
            ChangeState(MinionAiState::MoveToOpponentBase);
        }

        break;
    }
    default:
        break;
    }
}

void MinionEntity::OnExitState(MinionAiState exitedState)
{
    switch (exitedState)
    {
    case MinionAiState::MoveToOpponentBase:
        //Log("Stop moving to opponent base\n");
        //_pathFollower->Stop(true);
        break;
    default:
        break;
    }
}

void MinionEntity::ResetTimeouts()
{
    _attackTimeout = AttackTimeoutLength;
}

void MinionEntity::MeleeAttack(Entity* attackTarget)
{
    if (_attackTimeout > 0)
    {
        return;
    }
    else
    {
        ResetTimeouts();
    }

    auto distance = (attackTarget->Center() - Center()).Length();

    if (distance <= reach)
    {
        auto targetHealthComponent = attackTarget->GetComponent<HealthBarComponent>();
        targetHealthComponent->TakeDamage(5.0, this);
    }
}

void MinionEntity::FollowTarget(Entity* followTarget)
{
    _pathFollower->FollowEntity(followTarget, 128.0f);
}

Entity* MinionEntity::FindTargetOrNull()
{
    // These are kept in order by priority
    EntityDistanceByType closestTargetByEntityId[] = { CastleEntity::Type, MinionEntity::Type, PlayerEntity::Type, TowerEntity::Type };

    int aliveIndex = 0;

    for (int i = 0; i < _targets.Size(); ++i)
    {
        Entity* target;
        if (_targets[i].TryGetValue(target))
        {
            for (auto& closestTarget : closestTargetByEntityId)
            {
                if (target->type == closestTarget.type)
                {
                    float distance;
                    if (IsCloserThan(target, closestTarget.closest, this, distance))
                    {
                        closestTarget.closest = target;
                        closestTarget.distance = distance;
                    }
                }
            }

            _targets[aliveIndex++] = _targets[i];
        }
    }

    _targets.Resize(aliveIndex);

    for (auto& closestTarget : closestTargetByEntityId)
    {
        if (closestTarget.distance < engagementRadius)
        {
            return closestTarget.closest;
        }
    }

    return nullptr;
}