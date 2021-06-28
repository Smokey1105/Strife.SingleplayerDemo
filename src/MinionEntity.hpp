#pragma once

#include "Scene/BaseEntity.hpp"
#include "Components/PathFollowerComponent.hpp"
#include "TeamComponent.hpp"

struct CastleEntity;
struct PlayerEntity;
struct HealthBarComponent;

static constexpr int MaxMinions = 12;

DEFINE_ENTITY(MinionSpawner, "minion-spawner")
{
    void DoSerialize(EntitySerializer & serializer) override;

    void OnAdded() override;
    void FixedUpdate(float deltaTime) override;
    void SpawnMinion();

    float spawnTimeout = 0;//30.0f;

    float reach = 50.0f;
    float engagementRadius = 100.0f;

    float fireballTimeout = 0.75f;
    TeamComponent* team;
};

enum class MinionAiState
{
    DoNothing,
    MoveToOpponentBase,
    AttackTarget,
};

DEFINE_ENTITY(MinionEntity, "minion")
{
public:
    void OnAdded() override;

    void Render(Renderer * renderer) override;
    void FixedUpdate(float deltaTime) override;
    void ReceiveEvent(const IEntityEvent & ev) override;
    void ChangeState(MinionAiState newState);

    TeamComponent* team = nullptr;

    // Radius of the circle that encapsulates the minion, if an opponent minion/hero is in said circle,
    // the minion will attack
    float engagementRadius = 100.0f;

    // If the hero runs away from the minion, the minion will follow unless the distance between
    // the two exceeds this reach
    float reach = 50.0f;

    float AttackTimeoutLength = 0.75; // Essentially limits how often a minion can attack

    void Start();
    Entity* FindTargetOrNull();

private:
    void OnEnterState(MinionAiState stateEntered);
    void OnUpdateState();

    void OnExitState(MinionAiState exitedState);

    void MeleeAttack(Entity * attackTarget);
    void ResetTimeouts();
    void FollowTarget(Entity * followTarget);

    float _attackTimeout = 0.75;

    MinionAiState state = MinionAiState::DoNothing;
    PathFollowerComponent* _pathFollower = nullptr;
    RigidBodyComponent* _rb = nullptr;

    HealthBarComponent* _healthBar = nullptr;
    EntityReference<CastleEntity> _opponentBase = EntityReference<CastleEntity>::Invalid();
    FixedSizeVector<EntityReference<Entity>, 16> _targets;
    b2Fixture* _engagementCircle;
};