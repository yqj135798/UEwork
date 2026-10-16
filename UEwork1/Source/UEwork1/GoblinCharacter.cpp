#include "GoblinCharacter.h"

#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/ConstructorHelpers.h"

AGoblinCharacter::AGoblinCharacter()
{
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> GoblinMesh(
        TEXT("/Game/Fab/Stylized_Goblin_Minion_-_Free/SK_Goblin"));
    if (GoblinMesh.Succeeded())
    {
        GetMesh()->SetSkeletalMesh(GoblinMesh.Object);
    }
    // This mesh has no compatible animation clips in the imported Fab asset.
    // Do not fall back to deer or humanoid clips, which repeatedly reset the pose.
    IdleAnimation = nullptr;
    WalkAnimation = nullptr;
    AttackAnimation = nullptr;

    // Fixed, expanded bounds prevent culling/shadow bounds from popping while moving.
    GetMesh()->bComponentUseFixedSkelBounds = true;
    GetMesh()->SetBoundsScale(1.5f);
    GetMesh()->SetReceivesDecals(false);

    // The source mesh is a short creature. Scale it to the existing animal capsule.
    GetCapsuleComponent()->InitCapsuleSize(42.0f, 88.0f);
    GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -88.0f));
    GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
    GetMesh()->SetRelativeScale3D(FVector(1.4f));

    WalkSpeed = 135.0f;
    ChaseSpeed = 285.0f;
    MaxHealth = 40.0f;
    AttackDamage = 4.0f;
    GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
}
