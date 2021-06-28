#include "FireballEntity.hpp"
#include "Components/RigidBodyComponent.hpp"
#include "Components/NetComponent.hpp"
#include "HealthBarComponent.hpp"
#include "TeamComponent.hpp"
#include "Renderer/Renderer.hpp"


void FireballEntity::Render(Renderer* renderer)
{
    renderer->RenderCircle(Center(), Radius, Color::Orange(), -1);
}

void FireballEntity::OnAdded()
{
    auto rb = AddComponent<RigidBodyComponent>(b2_dynamicBody);
    rb->CreateCircleCollider(Radius, true);

    light.color = Color::Orange();
    light.maxDistance = Radius;
    light.intensity = 2;

    scene->GetLightManager()->AddLight(&light);

    rb->SetVelocity(velocity);
}

void FireballEntity::OnDestroyed()
{
    scene->GetLightManager()->RemoveLight(&light);
}

void FireballEntity::ReceiveEvent(const IEntityEvent& ev)
{
    if (auto contactBegin = ev.Is<ContactBeginEvent>())
    {
        auto other = contactBegin->other.OwningEntity();
        TeamComponent* otherTeam;

        if (other->TryGetComponent(otherTeam) && otherTeam->teamId == playerId) {
            return;
        }

        if (contactBegin->other.IsTrigger()) return;

        auto healthBar = other->GetComponent<HealthBarComponent>(false);

        if (healthBar != nullptr)
        {
            healthBar->TakeDamage(5, this);
        }

        Destroy();
    }
}

void FireballEntity::Update(float deltaTime)
{
    light.position = Center();
}
