#pragma once

#include "Scene/EntityComponent.hpp"
#include "Scene/IEntityEvent.hpp"

DEFINE_EVENT(OutOfHealthEvent)
{
    OutOfHealthEvent(Entity* killer)
        : killer(killer)
    {

    }

    Entity* killer;
};

DEFINE_EVENT(DamageDealtEvent)
{
    DamageDealtEvent(Entity * dealer, int damage)
        : dealer(dealer),
        damage(damage)
    {

    }

    Entity* dealer;
    int damage;
};

DEFINE_COMPONENT(HealthBarComponent)
{
    void Render(Renderer* renderer) override;
    void TakeDamage(int amount, Entity* fromEntity = nullptr);

    int health = 100;
    Vector2 offsetFromCenter;
    int maxHealth = 100;
};