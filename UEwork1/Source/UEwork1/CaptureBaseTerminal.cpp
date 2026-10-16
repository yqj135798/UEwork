#include "CaptureBaseTerminal.h"

#include "CaptureLauncher.h"
#include "Components/ChildActorComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

ACaptureBaseTerminal::ACaptureBaseTerminal()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.2f;

    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    FountainVisual = CreateDefaultSubobject<UChildActorComponent>(TEXT("FountainVisual"));
    FountainVisual->SetupAttachment(Root);
    static ConstructorHelpers::FClassFinder<AActor> FountainClass(TEXT("/Game/HebeFountain/Blueprints/BP_Fountain"));
    if (FountainClass.Succeeded())
    {
        FountainVisual->SetChildActorClass(FountainClass.Class);
    }
}

void ACaptureBaseTerminal::BeginPlay()
{
    Super::BeginPlay();

    if (BoundFountain.IsValid())
    {
        SetActorTransform(BoundFountain->GetActorTransform());
        return;
    }

    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BaseGroundTrace), false, this);
    if (FountainVisual && FountainVisual->GetChildActor())
    {
        QueryParams.AddIgnoredActor(FountainVisual->GetChildActor());
    }

    TArray<FHitResult> GroundHits;
    if (GetWorld() && GetWorld()->LineTraceMultiByChannel(
        GroundHits,
        GetActorLocation() + FVector(0.0f, 0.0f, 50000.0f),
        GetActorLocation() - FVector(0.0f, 0.0f, 50000.0f),
        ECC_Visibility,
        QueryParams))
    {
        // Multi-trace lets us pick the lowest valid world surface instead of
        // snapping the terminal to foliage or its own fountain collision.
        SetActorLocation(GroundHits.Last().ImpactPoint);
    }
}

void ACaptureBaseTerminal::BindToExistingFountain(AActor* ExistingFountain)
{
    BoundFountain = ExistingFountain;
    if (!ExistingFountain)
    {
        return;
    }

    SetActorTransform(ExistingFountain->GetActorTransform());
    if (FountainVisual)
    {
        FountainVisual->DestroyChildActor();
        FountainVisual->SetChildActorClass(nullptr);
        FountainVisual->SetVisibility(false, true);
    }
}

FString ACaptureBaseTerminal::GetStorageId() const
{
    const FVector StableLocation = BoundFountain.IsValid()
        ? BoundFountain->GetActorLocation()
        : GetActorLocation();
    return FString::Printf(
        TEXT("Base_%d_%d_%d"),
        FMath::RoundToInt(StableLocation.X),
        FMath::RoundToInt(StableLocation.Y),
        FMath::RoundToInt(StableLocation.Z)
    );
}

FVector ACaptureBaseTerminal::GetPartnerSpawnLocation(int32 SlotIndex) const
{
    const int32 Row = SlotIndex / 3;
    const int32 Column = SlotIndex % 3;
    const float LateralOffset = static_cast<float>(Column - 1) * 185.0f;
    const float ForwardOffset = 260.0f + static_cast<float>(Row) * 150.0f;
    return GetActorLocation()
        + GetActorForwardVector().GetSafeNormal2D() * ForwardOffset
        + GetActorRightVector().GetSafeNormal2D() * LateralOffset
        + FVector(0.0f, 0.0f, 80.0f);
}

void ACaptureBaseTerminal::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
    if (!PlayerPawn)
    {
        return;
    }

    if (BoundFountain.IsValid())
    {
        SetActorTransform(BoundFountain->GetActorTransform());
    }

    const bool bPlayerInRange = FVector::DistSquared2D(PlayerPawn->GetActorLocation(), GetActorLocation())
        <= FMath::Square(InteractionRadius);
    if (bPlayerInRange == bWasPlayerInRange)
    {
        return;
    }

    bWasPlayerInRange = bPlayerInRange;
    for (TActorIterator<ACaptureLauncher> It(GetWorld()); It; ++It)
    {
        It->SetBaseInteractionAvailable(this, bPlayerInRange);
        break;
    }
}
