#include "CaptureGameMode.h"

#include "AnimalCharacter.h"
#include "CaptureBaseTerminal.h"
#include "CaptureLauncher.h"
#include "CapturePlayerController.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

ACaptureGameMode::ACaptureGameMode()
{
    static ConstructorHelpers::FClassFinder<APawn> PlayerPawn(
        TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
    if (PlayerPawn.Succeeded())
    {
        DefaultPawnClass = PlayerPawn.Class;
    }
    PlayerControllerClass = ACapturePlayerController::StaticClass();
}

void ACaptureGameMode::BeginPlay()
{
    Super::BeginPlay();
    SpawnPrototypeActors();

    // A generated landscape can be much higher than the original template floor.
    // Wait until the controller has possessed its pawn, then place it above terrain.
    GetWorldTimerManager().SetTimerForNextTick(this, &ACaptureGameMode::PreparePlayerForWasteland);
}

void ACaptureGameMode::PreparePlayerForWasteland()
{
    APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
    if (!PlayerController)
    {
        return;
    }

    if (!PlayerController->GetPawn())
    {
        RestartPlayer(PlayerController);
    }

    APawn* PlayerPawn = PlayerController->GetPawn();
    if (!PlayerPawn || !GetWorld())
    {
        return;
    }

    PlayerController->SetIgnoreMoveInput(false);
    PlayerController->SetIgnoreLookInput(false);
    PlayerController->SetInputMode(FInputModeGameOnly());
    PlayerController->bShowMouseCursor = false;

    AActor* StartActor = UGameplayStatics::GetActorOfClass(this, APlayerStart::StaticClass());
    const FVector TraceCenter = StartActor ? StartActor->GetActorLocation() : PlayerPawn->GetActorLocation();

    FHitResult GroundHit;
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(PlayerGroundTrace), false, PlayerPawn);
    if (GetWorld()->LineTraceSingleByChannel(
        GroundHit,
        FVector(TraceCenter.X, TraceCenter.Y, 50000.0f),
        FVector(TraceCenter.X, TraceCenter.Y, -50000.0f),
        ECC_Visibility,
        QueryParams))
    {
        const float PawnHeight = PlayerPawn->GetSimpleCollisionHalfHeight();
        const FVector SafeLocation(
            TraceCenter.X,
            TraceCenter.Y,
            GroundHit.ImpactPoint.Z + PawnHeight + 10.0f);
        PlayerPawn->TeleportTo(SafeLocation, PlayerPawn->GetActorRotation(), false, true);
        UE_LOG(LogTemp, Log, TEXT("Capture prototype: player placed at terrain height %.1f"), SafeLocation.Z);
    }

    if (ACapturePlayerController* CaptureController = Cast<ACapturePlayerController>(PlayerController))
    {
        CaptureController->OpenMainMenu();
    }
}

void ACaptureGameMode::SpawnPrototypeActors()
{
    if (!GetWorld())
    {
        return;
    }

    bool bHasAnimal = false;
    for (TActorIterator<AAnimalCharacter> It(GetWorld()); It; ++It)
    {
        bHasAnimal = true;
        break;
    }

    bool bHasLauncher = false;
    bool bHasBaseTerminal = false;
    for (TActorIterator<ACaptureLauncher> It(GetWorld()); It; ++It)
    {
        bHasLauncher = true;
        break;
    }

    for (TActorIterator<ACaptureBaseTerminal> It(GetWorld()); It; ++It)
    {
        bHasBaseTerminal = true;
        break;
    }

    AActor* StartActor = UGameplayStatics::GetActorOfClass(this, APlayerStart::StaticClass());
    const FVector StartLocation = StartActor ? StartActor->GetActorLocation() : FVector::ZeroVector;
    const FRotator StartRotation = StartActor ? StartActor->GetActorRotation() : FRotator::ZeroRotator;

    if (!bHasAnimal)
    {
        const FVector AnimalLocation = StartLocation + StartRotation.Vector() * 850.0f + FVector(0.0f, 220.0f, 0.0f);
        GetWorld()->SpawnActor<AAnimalCharacter>(AAnimalCharacter::StaticClass(), AnimalLocation, FRotator::ZeroRotator);
    }

    if (!bHasLauncher)
    {
        GetWorld()->SpawnActor<ACaptureLauncher>(ACaptureLauncher::StaticClass(), StartLocation, FRotator::ZeroRotator);
    }

    TArray<AActor*> ExistingFountains;
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
        AActor* Actor = *It;
        if (Actor && Actor->GetClass()->GetName().Contains(TEXT("BP_Fountain")))
        {
            ExistingFountains.Add(Actor);
        }
    }

    if (!bHasBaseTerminal && !ExistingFountains.IsEmpty())
    {
        for (AActor* Fountain : ExistingFountains)
        {
            ACaptureBaseTerminal* Terminal = GetWorld()->SpawnActor<ACaptureBaseTerminal>(
                ACaptureBaseTerminal::StaticClass(),
                Fountain->GetActorLocation(),
                Fountain->GetActorRotation());
            if (Terminal)
            {
                Terminal->BindToExistingFountain(Fountain);
            }
        }
    }
    else if (!bHasBaseTerminal)
    {
        const FVector BaseLocation = StartLocation + StartRotation.Vector() * 1150.0f + StartRotation.RotateVector(FVector(0.0f, 750.0f, 0.0f));
        GetWorld()->SpawnActor<ACaptureBaseTerminal>(ACaptureBaseTerminal::StaticClass(), BaseLocation, FRotator::ZeroRotator);
    }
}
