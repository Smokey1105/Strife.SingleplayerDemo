#pragma once

#include <Components/PathFollowerComponent.hpp>
#include "GameML.hpp"
#include "Components/NetComponent.hpp"
#include "ML/ML.hpp"
#include "Scene/BaseEntity.hpp"
#include "Scene/IEntityEvent.hpp"
#include "HealthBarComponent.hpp"
#include "TeamComponent.hpp"

enum class PlayerState
{
    None,
    Moving,
    Attacking
};

enum class MoveDirection
{
    None,
    North,
    NorthEast,
    East,
    SouthEast,
    South,
    SouthWest,
    West,
    NorthWest,
    TotalDirections
};

DEFINE_ENTITY(PlayerEntity, "player")
{
    using NeuralNetwork = NeuralNetworkComponent<PlayerNetwork>;

    void Attack(Entity* entity);
    void SetMoveDirection(Vector2 direction);

    void OnAdded() override;
    void ReceiveEvent(const IEntityEvent& ev) override;
    void OnDestroyed() override;

    void Render(Renderer* renderer) override;
    void FixedUpdate(float deltaTime) override;

    RigidBodyComponent* rigidBody;
    PathFollowerComponent* pathFollower;
    HealthBarComponent* health;
    TeamComponent* team;

    EntityReference<Entity> attackTarget;
    PlayerState state = PlayerState::None;
    float attackCoolDown = 0;
    int playerId;
    MoveDirection lastDirection = MoveDirection::None;

    void Die(const OutOfHealthEvent* outOfHealth);
};

Vector2 MoveDirectionToVector2(MoveDirection direction);
MoveDirection GetClosestMoveDirection(Vector2 v);