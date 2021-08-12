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
    None = 0,
    Moving = 1,
    Attacking = 2
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
    void Attack(Entity* entity);
    void SetMoveDirection(Vector2 direction);
    void MoveTo(Vector2 position);

    void OnAdded() override;
    void ReceiveEvent(const IEntityEvent& ev) override;
    void OnDestroyed() override;

    void Render(Renderer* renderer) override;
    void FixedUpdate(float deltaTime) override;

    template<typename TEntity>
    std::tuple<TEntity*, float> NearestEntityOfType()
    {
        TEntity* nearest = nullptr;
        float minDistance;
        for (auto entity : scene->GetEntitiesOfType<TEntity>())
        {
            float distance = (entity->Center() - Center()).Length();

            if (nearest == nullptr || distance < minDistance)
            {
                TeamComponent* otherTeam;
                if (entity->TryGetComponent<TeamComponent>(otherTeam) && otherTeam->teamId != team->teamId)
                {
                    nearest = entity;
                    minDistance = distance;
                }
            }
        }

        return std::make_tuple(nearest, minDistance);
    }

    RigidBodyComponent* rigidBody;
    PathFollowerComponent* pathFollower;
    HealthBarComponent* health;
    TeamComponent* team;
    //GridSensorComponent<40, 40>* gridSensor;

    EntityReference<Entity> attackTarget;
    PlayerState state = PlayerState::None;
    float attackCoolDown = 0;
    int playerId;
    MoveDirection lastDirection = MoveDirection::None;
    Vector2 lastMoveVector;

    void Die(const OutOfHealthEvent* outOfHealth);
    void GetObservation(Observation& input);
};

Vector2 MoveDirectionToVector2(MoveDirection direction);
MoveDirection GetClosestMoveDirection(Vector2 v);
