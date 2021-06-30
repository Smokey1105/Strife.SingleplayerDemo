#include "ObstacleComponent.hpp"
#include "Physics/PathFinding.hpp"

void ObstacleComponent::OnAdded()
{
    Rectangle adjusted(GetScene()->isometricSettings.WorldToTile(owner->Bounds().TopLeft()),
        GetScene()->isometricSettings.WorldToTile(owner->Bounds().BottomRight()) - GetScene()->isometricSettings.WorldToTile(owner->Bounds().TopLeft()));
    GetScene()->GetService<PathFinderService>()->AddObstacle(adjusted);
    bounds = adjusted;
}

void ObstacleComponent::OnRemoved()
{
    GetScene()->GetService<PathFinderService>()->RemoveObstacle(bounds);
}