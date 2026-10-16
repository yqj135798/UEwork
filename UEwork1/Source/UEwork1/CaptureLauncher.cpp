#include "CaptureLauncher.h"

#include "CapturePlayerController.h"

#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/Skeleton.h"
#include "AnimalCharacter.h"
#include "CaptureBall.h"
#include "CaptureBaseTerminal.h"
#include "CaptureHUD.h"
#include "CaptureHotbarWidget.h"
#include "CapturePartnerSaveGame.h"
#include "CaptureStorageInputWidget.h"
#include "PartnerStorageWidget.h"
#include "Blueprint/UserWidget.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Guid.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UnrealType.h"
#include "TimerManager.h"

const FString ACaptureLauncher::PartnerSaveSlot(TEXT("CapturePartnerStorage"));

ACaptureLauncher::ACaptureLauncher()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.04f;
    SelectedSlot = 0;
    PlayerHealth = 30.0f;
    NextHandAttackTime = 0.0f;
    bCaptureAimActive = false;
    bCaptureThrowPending = false;
    bHandAttackActive = false;
    bEating = false;
    bPlayerDefeated = false;
    bComputerOpen = false;
    bInventoryOpen = false;
    Hunger = 100.0f;
    bBaseStorageOpen = false;
    bBaseInteractionAvailable = false;
    bComputerSelectingStorage = true;
    bComputerDragActive = false;
    bComputerDragSourceStorage = true;
    bReleaseConfirmationPending = false;
    bReleaseConfirmationStorage = true;
    bPartnerCommandMenuOpen = false;
    bPartnerCommandHoldActive = false;
    HandAttackDirection = FVector::ForwardVector;
    CurrentMeleeDamage = 3.0f;
    CurrentMeleeRange = 360.0f;
    bSwordWasFalling = false;
    SwordLocomotionLockUntil = 0.0f;
    LastSwordControlYaw = 0.0f;
    CurrentSwordLocomotionAnimation = nullptr;
    SelectedPartnerIndex = -1;
    ComputerSelectionIndex = -1;
    ComputerDragSourceIndex = -1;
    ReleaseConfirmationIndex = -1;
}

FString ACaptureLauncher::GetSaveSlotName() const
{
    if (const ACapturePlayerController* Controller = Cast<ACapturePlayerController>(
        UGameplayStatics::GetPlayerController(this, 0)))
    {
        return Controller->GetActiveSaveSlot();
    }
    return PartnerSaveSlot;
}

bool ACaptureLauncher::IsCreativeMode() const
{
    if (const ACapturePlayerController* Controller = Cast<ACapturePlayerController>(
        UGameplayStatics::GetPlayerController(this, 0)))
    {
        return Controller->IsCreativeMode();
    }
    return false;
}

void ACaptureLauncher::LoadSelectedSaveProfile()
{
    for (TPair<FString, TWeakObjectPtr<AAnimalCharacter>>& Entry : DeployedPartners)
    {
        if (Entry.Value.IsValid())
        {
            Entry.Value->Destroy();
        }
    }
    for (TPair<FString, TMap<FString, TWeakObjectPtr<AAnimalCharacter>>>& BaseEntry : DeployedBasePartners)
    {
        for (TPair<FString, TWeakObjectPtr<AAnimalCharacter>>& Entry : BaseEntry.Value)
        {
            if (Entry.Value.IsValid())
            {
                Entry.Value->Destroy();
            }
        }
    }
    DeployedPartners.Empty();
    DeployedBasePartners.Empty();
    PartyPartners.Empty();
    ComputerPartners.Empty();
    BasePartners.Empty();
    BasePartnersByTerminal.Empty();
    HotbarItems.Empty();
    BackpackItems.Empty();
    Hunger = 100.0f;
    SelectedSlot = 0;
    SelectedPartnerIndex = -1;

    LoadPartnerData();
    InitializeInventory();
    if (GetSaveSlotName() == PartnerSaveSlot)
    {
        const bool bHasCaptureBall = HotbarItems.ContainsByPredicate([](const FInventoryItemStack& Stack)
        {
            return Stack.ItemType == EInventoryItemType::CaptureBall && Stack.Count > 0;
        });
        if (!bHasCaptureBall)
        {
            FInventoryItemStack* EmptySlot = HotbarItems.FindByPredicate([](const FInventoryItemStack& Stack)
            {
                return Stack.IsEmpty();
            });
            if (EmptySlot)
            {
                *EmptySlot = { EInventoryItemType::CaptureBall, 1 };
            }
        }
    }
    SpawnStoredBasePartners();
    UpdateHotbar();
}

void ACaptureLauncher::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    UpdateSwordLocomotion();
    TickHunger(DeltaSeconds);
}

void ACaptureLauncher::BeginPlay()
{
    Super::BeginPlay();

    if (APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0))
    {
        EnableInput(PlayerController);
        if (InputComponent)
        {
            InputComponent->BindKey(
                EKeys::LeftMouseButton,
                IE_Pressed,
                this,
                &ACaptureLauncher::UsePrimaryAction
            );
            InputComponent->BindKey(
                EKeys::LeftMouseButton,
                IE_Released,
                this,
                &ACaptureLauncher::ReleasePrimaryAction
            );
            InputComponent->BindKey(
                EKeys::RightMouseButton,
                IE_Pressed,
                this,
                &ACaptureLauncher::UseSecondaryAction
            );
            InputComponent->BindKey(
                EKeys::Q,
                IE_Pressed,
                this,
                &ACaptureLauncher::DropSelectedItem
            );
            InputComponent->BindKey(EKeys::One, IE_Pressed, this, &ACaptureLauncher::SelectHand);
            InputComponent->BindKey(EKeys::Two, IE_Pressed, this, &ACaptureLauncher::SelectCaptureBall);
            InputComponent->BindKey(EKeys::Three, IE_Pressed, this, &ACaptureLauncher::SelectSword);
            InputComponent->BindKey(EKeys::Four, IE_Pressed, this, &ACaptureLauncher::SelectSlotFour);
            InputComponent->BindKey(EKeys::Five, IE_Pressed, this, &ACaptureLauncher::SelectSlotFive);
            InputComponent->BindKey(EKeys::Six, IE_Pressed, this, &ACaptureLauncher::SelectSlotSix);
            InputComponent->BindKey(EKeys::Seven, IE_Pressed, this, &ACaptureLauncher::SelectSlotSeven);
            InputComponent->BindKey(EKeys::Eight, IE_Pressed, this, &ACaptureLauncher::SelectSlotEight);
            InputComponent->BindKey(EKeys::Nine, IE_Pressed, this, &ACaptureLauncher::SelectSlotNine);
            InputComponent->BindKey(EKeys::MouseScrollUp, IE_Pressed, this, &ACaptureLauncher::SelectPreviousHotbarItem);
            InputComponent->BindKey(EKeys::MouseScrollDown, IE_Pressed, this, &ACaptureLauncher::SelectNextHotbarItem);
            InputComponent->BindKey(EKeys::P, IE_Pressed, this, &ACaptureLauncher::ToggleComputer);
            InputComponent->BindKey(EKeys::Y, IE_Pressed, this, &ACaptureLauncher::HandleYAction);
            InputComponent->BindKey(EKeys::N, IE_Pressed, this, &ACaptureLauncher::CancelReleaseConfirmation);
            InputComponent->BindKey(EKeys::Up, IE_Pressed, this, &ACaptureLauncher::SelectPreviousPartner);
            InputComponent->BindKey(EKeys::Down, IE_Pressed, this, &ACaptureLauncher::SelectNextPartner);
            InputComponent->BindKey(EKeys::Left, IE_Pressed, this, &ACaptureLauncher::ChangeComputerList);
            InputComponent->BindKey(EKeys::Right, IE_Pressed, this, &ACaptureLauncher::ChangeComputerList);
            InputComponent->BindKey(EKeys::R, IE_Pressed, this, &ACaptureLauncher::ToggleSelectedPartner);
            InputComponent->BindKey(EKeys::X, IE_Pressed, this, &ACaptureLauncher::ReleaseComputerPartner);
            InputComponent->BindKey(EKeys::E, IE_Pressed, this, &ACaptureLauncher::BeginPartnerCommandHold);
            InputComponent->BindKey(EKeys::E, IE_Released, this, &ACaptureLauncher::EndPartnerCommandHold);
            InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &ACaptureLauncher::CloseInventory);
        }

        PlayerController->ClientSetHUD(ACaptureHUD::StaticClass());
        ConfigurePlayerAnimations();
        LoadPartnerData();
        InitializeInventory();
        GetWorldTimerManager().SetTimerForNextTick(this, &ACaptureLauncher::SpawnStoredBasePartners);
        CreateHeldItemVisual();
        UpdateHotbar();
    }

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(
            -1,
            6.0f,
            FColor::Yellow,
            TEXT("Hotbar: 1-9 select, mouse wheel switches. Press E away from a partner to open inventory.")
        );
    }
}

void ACaptureLauncher::UsePrimaryAction()
{
    if (bPlayerDefeated || bEating)
    {
        return;
    }
    if (bPartnerCommandMenuOpen)
    {
        SelectPartnerCommandAtCursor();
        return;
    }

    if (bComputerOpen || bBaseStorageOpen || bInventoryOpen)
    {
        if (IsUsingUMGStorage())
        {
            return;
        }
        BeginComputerPointerDrag();
        return;
    }

    switch (GetSelectedItemType())
    {
    case EInventoryItemType::CaptureBall:
        BeginCaptureAim();
        break;
    case EInventoryItemType::GreatSword:
        AttackWithSword(false);
        break;
    default:
        AttackWithHand();
        break;
    }
}

void ACaptureLauncher::UseSecondaryAction()
{
    if (bPlayerDefeated || bEating || bPartnerCommandMenuOpen || bComputerOpen || bBaseStorageOpen || bInventoryOpen)
    {
        return;
    }

    if (GetSelectedItemType() == EInventoryItemType::GreatSword)
    {
        AttackWithSword(true);
    }
    else if (GetSelectedItemType() == EInventoryItemType::ChickenMeat
        || GetSelectedItemType() == EInventoryItemType::RawMeat
        || GetSelectedItemType() == EInventoryItemType::FishMeat)
    {
        ConsumeSelectedFood();
    }
}

void ACaptureLauncher::ReleasePrimaryAction()
{
    if (bComputerOpen || bBaseStorageOpen || bInventoryOpen)
    {
        if (IsUsingUMGStorage())
        {
            return;
        }
        EndComputerPointerDrag();
        return;
    }

    if (GetSelectedItemType() == EInventoryItemType::CaptureBall)
    {
        ReleaseCaptureAim();
    }
}

void ACaptureLauncher::HandleComputerPointerInput(bool bPressed)
{
    if (!bComputerOpen && !bBaseStorageOpen)
    {
        return;
    }

    if (bPressed)
    {
        APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
        float MouseX = 0.0f;
        float MouseY = 0.0f;
        if (PlayerController && PlayerController->GetMousePosition(MouseX, MouseY))
        {
            BeginStoragePointerDragAt(FVector2D(MouseX, MouseY));
        }
    }
    else
    {
        APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
        float MouseX = 0.0f;
        float MouseY = 0.0f;
        if (PlayerController && PlayerController->GetMousePosition(MouseX, MouseY))
        {
            EndStoragePointerDragAt(FVector2D(MouseX, MouseY));
        }
    }
}

void ACaptureLauncher::HandlePartnerCommandPointerInput(bool bPressed)
{
    if (bPartnerCommandMenuOpen && bPressed)
    {
        SelectPartnerCommandAtCursor();
    }
}

void ACaptureLauncher::BeginCaptureAim()
{
    if (bCaptureAimActive || bCaptureThrowPending || bHandAttackActive || bComputerOpen)
    {
        return;
    }

    if (!IsCreativeMode() && GetSaveSlotName() != PartnerSaveSlot && (!HotbarItems.IsValidIndex(SelectedSlot)
        || HotbarItems[SelectedSlot].ItemType != EInventoryItemType::CaptureBall
        || HotbarItems[SelectedSlot].Count <= 0))
    {
        return;
    }

    bCaptureAimActive = true;
    PlayPlayerAnimation(CaptureAimAnimation, true);

    if (APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0))
    {
        if (ACaptureHUD* CaptureHUD = Cast<ACaptureHUD>(PlayerController->GetHUD()))
        {
            CaptureHUD->SetCaptureAimState(true);
        }
    }
}

void ACaptureLauncher::ReleaseCaptureAim()
{
    if (!bCaptureAimActive || bCaptureThrowPending)
    {
        return;
    }

    bCaptureAimActive = false;
    bCaptureThrowPending = true;
    PlayPlayerAnimation(CaptureThrowAnimation, false);

    if (HeldBallMesh)
    {
        HeldBallMesh->SetVisibility(false, true);
    }

    if (APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0))
    {
        if (ACaptureHUD* CaptureHUD = Cast<ACaptureHUD>(PlayerController->GetHUD()))
        {
            CaptureHUD->SetCaptureAimState(false);
        }
    }

    // The retargeted throw reaches its release pose near the middle of the animation.
    GetWorldTimerManager().SetTimer(ThrowTimer, this, &ACaptureLauncher::SpawnCaptureBall, 1.05f, false);
}

void ACaptureLauncher::SpawnCaptureBall()
{
    APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
    if (!PlayerController)
    {
        bCaptureThrowPending = false;
        UpdateHeldItemVisual();
        return;
    }

    FVector ViewLocation;
    FRotator ViewRotation;
    PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);
    const FVector Direction = ViewRotation.Vector();
    const FVector SpawnLocation = ViewLocation + Direction * 150.0f;

    FActorSpawnParameters SpawnParameters;
    SpawnParameters.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    ACaptureBall* Ball = GetWorld()->SpawnActor<ACaptureBall>(
        ACaptureBall::StaticClass(),
        SpawnLocation,
        ViewRotation,
        SpawnParameters
    );
    if (Ball)
    {
        Ball->SetOwner(PlayerController->GetPawn());
        Ball->SetInstigator(PlayerController->GetPawn());
        Ball->InitVelocity(Direction);
        if (!IsCreativeMode() && GetSaveSlotName() != PartnerSaveSlot && HotbarItems.IsValidIndex(SelectedSlot))
        {
            FInventoryItemStack& CaptureBall = HotbarItems[SelectedSlot];
            if (CaptureBall.ItemType == EInventoryItemType::CaptureBall && --CaptureBall.Count <= 0)
            {
                CaptureBall.Clear();
            }
            UpdateHotbar();
            SavePartnerData();
        }
    }

    bCaptureThrowPending = false;
}

void ACaptureLauncher::AttackWithHand()
{
    APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
    APawn* PlayerPawn = PlayerController ? PlayerController->GetPawn() : nullptr;
    if (!PlayerController || !PlayerPawn || !GetWorld() || bHandAttackActive || bComputerOpen)
    {
        return;
    }

    if (GetWorld()->GetTimeSeconds() < NextHandAttackTime)
    {
        return;
    }
    const float AttackDuration = HandAttackAnimation
        ? HandAttackAnimation->GetPlayLength()
        : 0.45f;
    NextHandAttackTime = GetWorld()->GetTimeSeconds() + AttackDuration;
    bHandAttackActive = true;
    CurrentMeleeDamage = 3.0f;
    CurrentMeleeRange = 360.0f;

    FVector ViewLocation;
    FRotator ViewRotation;
    PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);
    HandAttackDirection = ViewRotation.Vector();
    HandAttackDirection.Z = 0.0f;
    HandAttackDirection.Normalize();

    // An attack is committed until its recovery frame: the camera can still aim,
    // but locomotion is disabled so its motion and its damage land together.
    PlayerController->SetIgnoreMoveInput(true);
    PlayPlayerAnimation(HandAttackAnimation, false);

    const float ImpactTime = FMath::Clamp(AttackDuration * 0.56f, 0.16f, 0.34f);
    GetWorldTimerManager().SetTimer(
        HandAttackHitTimer,
        this,
        &ACaptureLauncher::ResolveHandAttack,
        ImpactTime,
        false
    );
    GetWorldTimerManager().SetTimer(
        HandAttackFinishTimer,
        this,
        &ACaptureLauncher::FinishHandAttack,
        FMath::Max(0.1f, AttackDuration),
        false
    );
}

void ACaptureLauncher::AttackWithSword(bool bUseSlash)
{
    APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
    APawn* PlayerPawn = PlayerController ? PlayerController->GetPawn() : nullptr;
    if (!PlayerController || !PlayerPawn || !GetWorld() || bHandAttackActive || bComputerOpen)
    {
        return;
    }

    if (GetWorld()->GetTimeSeconds() < NextHandAttackTime)
    {
        return;
    }

    UAnimSequence* AttackAnimation = bUseSlash && SwordSlashAnimation
        ? SwordSlashAnimation.Get()
        : SwordAttackAnimation.Get();
    const float AttackDuration = AttackAnimation ? AttackAnimation->GetPlayLength() : 0.9f;
    NextHandAttackTime = GetWorld()->GetTimeSeconds() + AttackDuration;
    bHandAttackActive = true;
    CurrentMeleeDamage = 6.0f;
    CurrentMeleeRange = 470.0f;

    FVector ViewLocation;
    FRotator ViewRotation;
    PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);
    HandAttackDirection = ViewRotation.Vector();
    HandAttackDirection.Z = 0.0f;
    HandAttackDirection.Normalize();

    PlayerController->SetIgnoreMoveInput(true);
    CurrentSwordLocomotionAnimation = nullptr;
    PlayPlayerAnimation(AttackAnimation, false);

    const float ImpactTime = FMath::Clamp(AttackDuration * 0.52f, 0.22f, 0.72f);
    GetWorldTimerManager().SetTimer(
        HandAttackHitTimer,
        this,
        &ACaptureLauncher::ResolveHandAttack,
        ImpactTime,
        false
    );
    GetWorldTimerManager().SetTimer(
        HandAttackFinishTimer,
        this,
        &ACaptureLauncher::FinishHandAttack,
        FMath::Max(0.1f, AttackDuration),
        false
    );
}

void ACaptureLauncher::ResolveHandAttack()
{
    APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
    APawn* PlayerPawn = PlayerController ? PlayerController->GetPawn() : nullptr;
    if (!bHandAttackActive || !PlayerPawn || !GetWorld())
    {
        return;
    }

    AAnimalCharacter* BestTarget = nullptr;
    float BestScore = TNumericLimits<float>::Max();
    for (TActorIterator<AAnimalCharacter> It(GetWorld()); It; ++It)
    {
        AAnimalCharacter* Animal = *It;
        if (!Animal || Animal->IsInactive())
        {
            continue;
        }

        FVector ToAnimal = Animal->GetActorLocation() - PlayerPawn->GetActorLocation();
        const float VerticalDistance = FMath::Abs(ToAnimal.Z);
        ToAnimal.Z = 0.0f;
        const float HorizontalDistance = ToAnimal.Size();
        if (HorizontalDistance > CurrentMeleeRange || VerticalDistance > 180.0f)
        {
            continue;
        }

        const float FacingDot = FVector::DotProduct(HandAttackDirection, ToAnimal.GetSafeNormal());
        if (FacingDot < -0.30f)
        {
            continue;
        }

        const float Score = HorizontalDistance - FacingDot * 110.0f;
        if (Score < BestScore)
        {
            BestTarget = Animal;
            BestScore = Score;
        }
    }

    if (BestTarget)
    {
        BestTarget->ReceiveDamage(CurrentMeleeDamage, PlayerPawn);
        CommandPartnersToAttackTarget(BestTarget);
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(
                -1,
                0.8f,
                FColor::White,
                CurrentMeleeDamage > 3.0f ? TEXT("Great sword hit: 6 damage") : TEXT("Punch hit: 3 damage")
            );
        }
        return;
    }

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 0.8f, FColor::White, TEXT("Punch missed"));
    }
}

void ACaptureLauncher::FinishHandAttack()
{
    bHandAttackActive = false;

    if (APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0))
    {
        PlayerController->SetIgnoreMoveInput(false);
    }

    RestorePlayerAnimation();
}

void ACaptureLauncher::DropSelectedItem()
{
    if (bComputerOpen || bInventoryOpen || bEating || !HotbarItems.IsValidIndex(SelectedSlot))
    {
        return;
    }

    FInventoryItemStack& SelectedItem = HotbarItems[SelectedSlot];
    if (SelectedItem.IsEmpty())
    {
        return;
    }

    if (SelectedItem.ItemType == EInventoryItemType::GreatSword)
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(
                -1,
                1.0f,
                FColor::White,
                TEXT("The great sword cannot be dropped")
            );
        }
        return;
    }

    APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
    if (!PlayerController || !GetWorld())
    {
        return;
    }

    APawn* PlayerPawn = PlayerController->GetPawn();
    if (!PlayerPawn)
    {
        return;
    }

    FVector ThrowDirection = PlayerPawn->GetActorForwardVector();
    ThrowDirection.Z = 0.0f;
    if (!ThrowDirection.Normalize())
    {
        ThrowDirection = PlayerController->GetControlRotation().Vector();
        ThrowDirection.Z = 0.0f;
        ThrowDirection.Normalize();
    }
    ThrowDirection = (ThrowDirection + FVector(0.0f, 0.0f, 0.28f)).GetSafeNormal();

    const FVector SpawnLocation = PlayerPawn->GetActorLocation()
        + PlayerPawn->GetActorForwardVector().GetSafeNormal2D() * 85.0f
        + FVector(0.0f, 0.0f, 80.0f);
    FActorSpawnParameters SpawnParameters;
    SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    if (SelectedItem.ItemType == EInventoryItemType::CaptureBall)
    {
        if (ACaptureBall* DroppedBall = GetWorld()->SpawnActor<ACaptureBall>(
            ACaptureBall::StaticClass(),
            SpawnLocation,
            ThrowDirection.Rotation(),
            SpawnParameters))
        {
            DroppedBall->SetCaptureEnabled(false);
            DroppedBall->InitVelocity(ThrowDirection);
        }
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Yellow, TEXT("Capture ball dropped (test mode: unlimited supply)"));
        }
        return;
    }

    if (SelectedItem.ItemType != EInventoryItemType::ChickenMeat
        && SelectedItem.ItemType != EInventoryItemType::RawMeat
        && SelectedItem.ItemType != EInventoryItemType::FishMeat)
    {
        return;
    }

    --SelectedItem.Count;
    if (SelectedItem.Count <= 0)
    {
        SelectedItem.Clear();
    }

    if (UStaticMesh* FoodMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Fab/Food__fish__chicken__meat__3d_models_low_poly_/food_fish_chicken_meat3d_models_low_poly/StaticMeshes/food_fish_chicken_meat3d_models_low_poly")))
    {
        UStaticMeshComponent* DroppedFoodVisual = NewObject<UStaticMeshComponent>(PlayerPawn, NAME_None, RF_Transient);
        PlayerPawn->AddInstanceComponent(DroppedFoodVisual);
        DroppedFoodVisual->SetStaticMesh(FoodMesh);
        DroppedFoodVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        DroppedFoodVisual->SetCastShadow(true);
        DroppedFoodVisual->SetWorldLocation(SpawnLocation - FVector(0.0f, 0.0f, 55.0f));
        DroppedFoodVisual->SetWorldRotation(ThrowDirection.Rotation());
        DroppedFoodVisual->SetWorldScale3D(FVector(0.42f));
        DroppedFoodVisual->RegisterComponent();
        FTimerHandle FoodVisualTimer;
        GetWorldTimerManager().SetTimer(FoodVisualTimer, FTimerDelegate::CreateWeakLambda(DroppedFoodVisual, [DroppedFoodVisual]()
        {
            DroppedFoodVisual->DestroyComponent();
        }), 10.0f, false);
    }

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 1.2f, FColor::Yellow, TEXT("Food dropped"));
    }
    UpdateHotbar();
    SavePartnerData();
}

void ACaptureLauncher::RegisterCapturedAnimal(AAnimalCharacter* Animal)
{
    if (!Animal || !Animal->GetClass())
    {
        return;
    }

    FPartnerRecord NewPartner;
    NewPartner.AnimalClass = Animal->GetClass();
    NewPartner.DisplayName = MakePartnerDisplayName(Animal);
    NewPartner.PartnerId = FGuid::NewGuid().ToString(EGuidFormats::Digits);

    if (PartyPartners.Num() < 6)
    {
        PartyPartners.Add(NewPartner);
        SelectedPartnerIndex = PartyPartners.Num() - 1;
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(
                -1,
                2.5f,
                FColor::Green,
                FString::Printf(TEXT("%s joined your party (%d/6)"), *NewPartner.DisplayName, PartyPartners.Num())
            );
        }
    }
    else
    {
        ComputerPartners.Add(NewPartner);
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(
                -1,
                2.5f,
                FColor::Cyan,
                FString::Printf(TEXT("Party is full. %s was transferred to the computer"), *NewPartner.DisplayName)
            );
        }
    }

    RefreshPartnerHUD();
    SavePartnerData();
}

FString ACaptureLauncher::MakePartnerDisplayName(const AAnimalCharacter* Animal) const
{
    FString DisplayName = Animal ? Animal->GetClass()->GetName() : TEXT("Animal");
    DisplayName.RemoveFromEnd(TEXT("_C"));
    DisplayName.ReplaceInline(TEXT("BP_Animal_"), TEXT(""));
    DisplayName.ReplaceInline(TEXT("BP_"), TEXT(""));
    return DisplayName.IsEmpty() ? TEXT("Animal") : DisplayName;
}

void ACaptureLauncher::ToggleComputer()
{
    if (bHandAttackActive || bCaptureThrowPending)
    {
        return;
    }

    if (bComputerOpen)
    {
        CloseStorage();
        return;
    }

    bBaseStorageOpen = false;
    bComputerOpen = true;
    bComputerSelectingStorage = true;
    ComputerSelectionIndex = ComputerPartners.IsEmpty() ? -1 : 0;
    ClearReleaseConfirmation();
    if (bCaptureAimActive)
    {
        bCaptureAimActive = false;
        RestorePlayerAnimation();
    }
    ShowStorageInputWidget();
    RefreshPartnerHUD();
}

void ACaptureLauncher::ToggleInventory()
{
    if (bInventoryOpen)
    {
        CloseInventory();
    }
    else
    {
        OpenInventory();
    }
}

void ACaptureLauncher::OpenInventory()
{
    if (bInventoryOpen || bComputerOpen || bBaseStorageOpen || bHandAttackActive || bCaptureThrowPending)
    {
        return;
    }

    APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
    if (!PlayerController)
    {
        return;
    }

    bInventoryOpen = true;
    if (!PlayerInventoryWidget)
    {
        PlayerInventoryWidget = CreateWidget<UPlayerInventoryWidget>(PlayerController);
    }
    if (!PlayerInventoryWidget)
    {
        bInventoryOpen = false;
        UE_LOG(LogTemp, Error, TEXT("Could not create player inventory widget."));
        return;
    }

    PlayerInventoryWidget->RefreshFromLauncher(this);
    if (!PlayerInventoryWidget->IsInViewport())
    {
        PlayerInventoryWidget->AddToViewport(120);
    }
    PlayerInventoryWidget->SetVisibility(ESlateVisibility::Visible);
    PlayerController->SetIgnoreMoveInput(true);
    PlayerController->SetIgnoreLookInput(true);
    PlayerController->bShowMouseCursor = true;
    PlayerController->bEnableClickEvents = true;
    PlayerController->bEnableMouseOverEvents = true;
    FInputModeGameAndUI InputMode;
    InputMode.SetHideCursorDuringCapture(false);
    InputMode.SetWidgetToFocus(PlayerInventoryWidget->TakeWidget());
    PlayerController->SetInputMode(InputMode);
}

void ACaptureLauncher::CloseInventory()
{
    if (!bInventoryOpen)
    {
        return;
    }

    bInventoryOpen = false;
    if (PlayerInventoryWidget)
    {
        PlayerInventoryWidget->RemoveFromParent();
    }
    if (APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0))
    {
        PlayerController->SetIgnoreMoveInput(false);
        PlayerController->SetIgnoreLookInput(false);
        PlayerController->bShowMouseCursor = false;
        PlayerController->bEnableClickEvents = false;
        PlayerController->bEnableMouseOverEvents = false;
        PlayerController->SetInputMode(FInputModeGameOnly());
    }
    SavePartnerData();
}

bool ACaptureLauncher::HandleInventoryShortcut(const FKey& Key)
{
    if (!bInventoryOpen)
    {
        return false;
    }
    if (Key == EKeys::E || Key == EKeys::Escape)
    {
        CloseInventory();
        return true;
    }
    return false;
}

void ACaptureLauncher::InitializeInventory()
{
    const bool bMissingInventory = HotbarItems.Num() != 9 || BackpackItems.Num() != 27;
    if (HotbarItems.Num() != 9)
    {
        HotbarItems.SetNum(9);
    }
    if (BackpackItems.Num() != 27)
    {
        BackpackItems.SetNum(27);
    }
    for (FInventoryItemStack& Stack : HotbarItems)
    {
        if (Stack.Count <= 0)
        {
            Stack.Clear();
        }
    }
    for (FInventoryItemStack& Stack : BackpackItems)
    {
        if (Stack.Count <= 0)
        {
            Stack.Clear();
        }
    }
    if (bMissingInventory)
    {
        HotbarItems[0] = { EInventoryItemType::CaptureBall, 1 };
        HotbarItems[1] = { EInventoryItemType::GreatSword, 1 };
    }
    SelectedSlot = FMath::Clamp(SelectedSlot, 0, HotbarItems.Num() - 1);
    Hunger = FMath::Clamp(Hunger, 0.0f, 100.0f);
}

EInventoryItemType ACaptureLauncher::GetSelectedItemType() const
{
    return HotbarItems.IsValidIndex(SelectedSlot) && !HotbarItems[SelectedSlot].IsEmpty()
        ? HotbarItems[SelectedSlot].ItemType
        : EInventoryItemType::None;
}

void ACaptureLauncher::MoveInventoryItem(bool bSourceBackpack, int32 SourceIndex, bool bTargetBackpack, int32 TargetIndex)
{
    TArray<FInventoryItemStack>& SourceList = bSourceBackpack ? BackpackItems : HotbarItems;
    TArray<FInventoryItemStack>& TargetList = bTargetBackpack ? BackpackItems : HotbarItems;
    if (!SourceList.IsValidIndex(SourceIndex) || !TargetList.IsValidIndex(TargetIndex)
        || (bSourceBackpack == bTargetBackpack && SourceIndex == TargetIndex))
    {
        return;
    }

    FInventoryItemStack& Source = SourceList[SourceIndex];
    FInventoryItemStack& Target = TargetList[TargetIndex];
    if (Source.IsEmpty())
    {
        return;
    }
    const int32 MaxStack = (Source.ItemType == EInventoryItemType::ChickenMeat || Source.ItemType == EInventoryItemType::RawMeat || Source.ItemType == EInventoryItemType::FishMeat) ? 100 : 1;
    if (!Target.IsEmpty() && Target.ItemType == Source.ItemType && Target.Count < MaxStack)
    {
        const int32 MovedCount = FMath::Min(MaxStack - Target.Count, Source.Count);
        Target.Count += MovedCount;
        Source.Count -= MovedCount;
        if (Source.Count <= 0)
        {
            Source.Clear();
        }
    }
    else
    {
        Swap(Source, Target);
    }
    if (PlayerInventoryWidget)
    {
        PlayerInventoryWidget->RefreshFromLauncher(this);
    }
    UpdateHotbar();
    SavePartnerData();
}

void ACaptureLauncher::AddInventoryItem(EInventoryItemType ItemType, int32 Count)
{
    if (ItemType == EInventoryItemType::None || Count <= 0)
    {
        return;
    }
    const int32 MaxStack = (ItemType == EInventoryItemType::ChickenMeat || ItemType == EInventoryItemType::RawMeat || ItemType == EInventoryItemType::FishMeat) ? 100 : 1;
    auto FillExisting = [&](TArray<FInventoryItemStack>& List)
    {
        for (FInventoryItemStack& Stack : List)
        {
            if (Count <= 0 || Stack.ItemType != ItemType || Stack.Count >= MaxStack)
            {
                continue;
            }
            const int32 Added = FMath::Min(MaxStack - Stack.Count, Count);
            Stack.Count += Added;
            Count -= Added;
        }
    };
    auto FillEmpty = [&](TArray<FInventoryItemStack>& List)
    {
        for (FInventoryItemStack& Stack : List)
        {
            if (Count <= 0 || !Stack.IsEmpty())
            {
                continue;
            }
            const int32 Added = FMath::Min(MaxStack, Count);
            Stack.ItemType = ItemType;
            Stack.Count = Added;
            Count -= Added;
        }
    };
    FillExisting(HotbarItems);
    FillExisting(BackpackItems);
    FillEmpty(HotbarItems);
    FillEmpty(BackpackItems);
    if (Count > 0 && GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("Inventory full: some items could not be picked up"));
    }
    if (PlayerInventoryWidget)
    {
        PlayerInventoryWidget->RefreshFromLauncher(this);
    }
    UpdateHotbar();
    SavePartnerData();
}

void ACaptureLauncher::AddAnimalDrops(const AAnimalCharacter* Animal)
{
    if (!Animal || Animal->IsCompanion())
    {
        return;
    }
    const FString Name = Animal->GetClass() ? Animal->GetClass()->GetName() : FString();
    const EInventoryItemType Drop = Name.Contains(TEXT("Chicken"))
        ? EInventoryItemType::ChickenMeat
        : (Name.Contains(TEXT("Fish")) ? EInventoryItemType::FishMeat : EInventoryItemType::RawMeat);

    // The pickup is automatic, but briefly show the imported food mesh at the
    // defeated creature so the inventory gain has a visible world-space cause.
    if (AAnimalCharacter* MutableAnimal = const_cast<AAnimalCharacter*>(Animal))
    {
        if (UStaticMesh* FoodMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Fab/Food__fish__chicken__meat__3d_models_low_poly_/food_fish_chicken_meat3d_models_low_poly/StaticMeshes/food_fish_chicken_meat3d_models_low_poly")))
        {
            UStaticMeshComponent* PickupVisual = NewObject<UStaticMeshComponent>(MutableAnimal, NAME_None, RF_Transient);
            MutableAnimal->AddInstanceComponent(PickupVisual);
            PickupVisual->SetStaticMesh(FoodMesh);
            PickupVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            PickupVisual->SetCastShadow(false);
            PickupVisual->SetWorldLocation(Animal->GetActorLocation() + FVector(0.0f, 0.0f, 38.0f));
            PickupVisual->SetWorldScale3D(FVector(0.45f));
            PickupVisual->RegisterComponent();
            FTimerHandle PickupVisualTimer;
            GetWorldTimerManager().SetTimer(PickupVisualTimer, FTimerDelegate::CreateWeakLambda(PickupVisual, [PickupVisual]()
            {
                PickupVisual->DestroyComponent();
            }), 0.85f, false);
        }
    }
    AddInventoryItem(Drop, 1);
    if (GEngine)
    {
        const TCHAR* Label = Drop == EInventoryItemType::ChickenMeat ? TEXT("chicken meat") : (Drop == EInventoryItemType::FishMeat ? TEXT("fish meat") : TEXT("raw meat"));
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, FString::Printf(TEXT("Picked up %s"), Label));
    }
}

void ACaptureLauncher::ConsumeSelectedFood()
{
    if (!HotbarItems.IsValidIndex(SelectedSlot) || bHandAttackActive || bCaptureThrowPending || bEating)
    {
        return;
    }
    FInventoryItemStack& Food = HotbarItems[SelectedSlot];
    float RestoredHunger = 0.0f;
    switch (Food.ItemType)
    {
    case EInventoryItemType::ChickenMeat: RestoredHunger = 20.0f; break;
    case EInventoryItemType::RawMeat: RestoredHunger = 15.0f; break;
    case EInventoryItemType::FishMeat: RestoredHunger = 18.0f; break;
    default: return;
    }
    Hunger = FMath::Min(100.0f, Hunger + RestoredHunger);
    if (!IsCreativeMode())
    {
        --Food.Count;
        if (Food.Count <= 0)
        {
            Food.Clear();
        }
    }
    bEating = true;
    PlayPlayerAnimation(EatAnimation, false);
    // AS_Eats has a long recovery tail. The food is consumed at the bite pose,
    // then normal locomotion resumes instead of holding the final pose.
    const float EatDuration = EatAnimation
        ? FMath::Clamp(EatAnimation->GetPlayLength() * 0.42f, 0.45f, 0.85f)
        : 0.65f;
    GetWorldTimerManager().SetTimer(AnimationRestoreTimer, this, &ACaptureLauncher::RestorePlayerAnimation, EatDuration, false);
    UpdateHotbar();
    SavePartnerData();
}

void ACaptureLauncher::TickHunger(float DeltaSeconds)
{
    if (IsCreativeMode())
    {
        const bool bChanged = !FMath::IsNearlyEqual(Hunger, 100.0f) || !FMath::IsNearlyEqual(PlayerHealth, 30.0f);
        Hunger = 100.0f;
        PlayerHealth = 30.0f;
        if (bChanged)
        {
            UpdateHotbar();
        }
        return;
    }

    if (bPlayerDefeated || bComputerOpen || bInventoryOpen || !GetWorld())
    {
        return;
    }
    ACharacter* Character = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
    const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
    const float Speed = Character ? Character->GetVelocity().Size2D() : 0.0f;
    const float DrainPerSecond = Speed < 5.0f ? 0.025f : (Movement && Movement->MaxWalkSpeed >= 650.0f ? 0.14f : 0.08f);
    const float PreviousHunger = Hunger;
    const float PreviousHealth = PlayerHealth;
    const bool bCanRegenerate = PlayerHealth < 30.0f && Hunger >= 18.0f;
    const float RegenerationHungerCost = bCanRegenerate ? 0.35f : 0.0f;
    Hunger = FMath::Max(0.0f, Hunger - (DrainPerSecond + RegenerationHungerCost) * DeltaSeconds);
    if (bCanRegenerate)
    {
        // Minecraft-style passive healing: sufficient food restores health and
        // burns hunger faster. It stops automatically at the low-food threshold.
        PlayerHealth = FMath::Min(30.0f, PlayerHealth + 0.50f * DeltaSeconds);
    }
    if (FMath::Abs(Hunger - PreviousHunger) > 0.01f || FMath::Abs(PlayerHealth - PreviousHealth) > 0.01f)
    {
        UpdateHotbar();
    }
}

void ACaptureLauncher::InteractWithBase()
{
    if (!bComputerOpen && bBaseInteractionAvailable)
    {
        OpenBaseStorage();
    }
}

void ACaptureLauncher::HandleYAction()
{
    if (bReleaseConfirmationPending)
    {
        ConfirmReleaseComputerPartner();
        return;
    }

    InteractWithBase();
}

void ACaptureLauncher::CancelReleaseConfirmation()
{
    if (bReleaseConfirmationPending)
    {
        ClearReleaseConfirmation();
    }
}

void ACaptureLauncher::OpenBaseStorage()
{
    if (bHandAttackActive || bCaptureThrowPending || !bBaseInteractionAvailable)
    {
        return;
    }

    bBaseStorageOpen = true;
    bComputerOpen = true;
    ActiveBaseStorageId = AvailableBaseTerminal->GetStorageId();
    MigrateLegacyBasePartners();
    bComputerSelectingStorage = true;
    ComputerSelectionIndex = GetActiveStoragePartners().IsEmpty() ? -1 : 0;
    ClearReleaseConfirmation();
    SyncActiveBasePartners();
    ShowStorageInputWidget();
    RefreshPartnerHUD();
}

void ACaptureLauncher::CloseStorage()
{
    ClearReleaseConfirmation();
    bComputerOpen = false;
    bBaseStorageOpen = false;
    ActiveBaseStorageId.Empty();
    bComputerDragActive = false;
    ComputerDragSourceIndex = -1;
    HideStorageInputWidget();

    if (APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0))
    {
        PlayerController->SetIgnoreMoveInput(false);
        PlayerController->SetIgnoreLookInput(false);
        PlayerController->bShowMouseCursor = false;
        PlayerController->bEnableClickEvents = false;
        PlayerController->bEnableMouseOverEvents = false;
        PlayerController->SetInputMode(FInputModeGameOnly());
        if (ACaptureHUD* CaptureHUD = Cast<ACaptureHUD>(PlayerController->GetHUD()))
        {
            CaptureHUD->SetComputerDragState(false, FString());
        }
    }

    RefreshPartnerHUD();
}

void ACaptureLauncher::ShowStorageInputWidget()
{
    APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
    if (!PlayerController)
    {
        return;
    }

    // WBP_PartnerStorage is the actual runtime surface. Its C++ parent owns the
    // data bridge, while the Blueprint remains the project's visible UI asset.
    if (StorageInputWidget)
    {
        StorageInputWidget->RemoveFromParent();
    }

    if (!PartnerStorageWidget)
    {
        UClass* StorageWidgetClass = LoadClass<UPartnerStorageWidget>(
            nullptr,
            TEXT("/Game/UI/WBP_PartnerStorage.WBP_PartnerStorage_C")
        );
        PartnerStorageWidget = StorageWidgetClass
            ? CreateWidget<UPartnerStorageWidget>(PlayerController, StorageWidgetClass)
            : CreateWidget<UPartnerStorageWidget>(PlayerController);
    }

    if (!PartnerStorageWidget)
    {
        UE_LOG(LogTemp, Error, TEXT("Could not create WBP_PartnerStorage."));
        return;
    }

    PartnerStorageWidget->RefreshFromLauncher(this);
    if (!PartnerStorageWidget->IsInViewport())
    {
        PartnerStorageWidget->AddToViewport(100);
    }
    PartnerStorageWidget->SetVisibility(ESlateVisibility::Visible);

    PlayerController->SetIgnoreMoveInput(true);
    PlayerController->SetIgnoreLookInput(true);
    PlayerController->bShowMouseCursor = true;
    PlayerController->bEnableClickEvents = true;
    PlayerController->bEnableMouseOverEvents = true;
    FInputModeGameAndUI InputMode;
    InputMode.SetHideCursorDuringCapture(false);
    InputMode.SetWidgetToFocus(PartnerStorageWidget->TakeWidget());
    PlayerController->SetInputMode(InputMode);
}

void ACaptureLauncher::HideStorageInputWidget()
{
    if (StorageInputWidget)
    {
        StorageInputWidget->RemoveFromParent();
    }

    if (PartnerStorageWidget)
    {
        PartnerStorageWidget->RemoveFromParent();
    }
}

bool ACaptureLauncher::IsUsingUMGStorage() const
{
    return PartnerStorageWidget && PartnerStorageWidget->IsInViewport();
}

bool ACaptureLauncher::HandleStorageShortcut(const FKey& Key)
{
    if (!bComputerOpen)
    {
        return false;
    }

    if (Key == EKeys::Up)
    {
        SelectPreviousPartner();
    }
    else if (Key == EKeys::Down)
    {
        SelectNextPartner();
    }
    else if (Key == EKeys::Left || Key == EKeys::Right)
    {
        ChangeComputerList();
    }
    else if (Key == EKeys::X)
    {
        ReleaseComputerPartner();
    }
    else if (Key == EKeys::P)
    {
        CloseStorage();
    }
    else
    {
        return false;
    }

    return true;
}

void ACaptureLauncher::PopulatePartnerStorageWidget()
{
    if (!PartnerStorageWidget)
    {
        return;
    }

    PartnerStorageWidget->RefreshFromLauncher(this);
}

void ACaptureLauncher::SetBaseInteractionAvailable(ACaptureBaseTerminal* BaseTerminal, bool bAvailable)
{
    if (bAvailable)
    {
        bBaseInteractionAvailable = true;
        AvailableBaseTerminal = BaseTerminal;
    }
    else if (AvailableBaseTerminal.Get() == BaseTerminal)
    {
        // Other, distant fountains must not clear the prompt for the one the
        // player is currently standing next to.
        bBaseInteractionAvailable = false;
        AvailableBaseTerminal = nullptr;
    }
    else
    {
        return;
    }

    if (APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0))
    {
        if (ACaptureHUD* CaptureHUD = Cast<ACaptureHUD>(PlayerController->GetHUD()))
        {
            CaptureHUD->SetBaseInteractionPrompt(bBaseInteractionAvailable);
        }
    }
}

void ACaptureLauncher::SelectPreviousPartner()
{
    ChangeSelectedPartner(-1);
}

void ACaptureLauncher::SelectNextPartner()
{
    ChangeSelectedPartner(1);
}

void ACaptureLauncher::ChangeSelectedPartner(int32 Direction)
{
    ClearReleaseConfirmation();
    if (bComputerOpen)
    {
        const int32 ItemCount = bComputerSelectingStorage ? GetActiveStoragePartners().Num() : PartyPartners.Num();
        if (ItemCount > 0)
        {
            ComputerSelectionIndex = ComputerSelectionIndex >= 0
                ? (ComputerSelectionIndex + Direction + ItemCount) % ItemCount
                : 0;
        }
    }
    else if (!PartyPartners.IsEmpty())
    {
        SelectedPartnerIndex = SelectedPartnerIndex >= 0
            ? (SelectedPartnerIndex + Direction + PartyPartners.Num()) % PartyPartners.Num()
            : 0;
    }

    RefreshPartnerHUD();
}

void ACaptureLauncher::ChangeComputerList()
{
    if (!bComputerOpen)
    {
        return;
    }

    ClearReleaseConfirmation();
    bComputerSelectingStorage = !bComputerSelectingStorage;
    const int32 ItemCount = bComputerSelectingStorage ? GetActiveStoragePartners().Num() : PartyPartners.Num();
    ComputerSelectionIndex = ItemCount > 0 ? 0 : -1;
    RefreshPartnerHUD();
}

void ACaptureLauncher::ReleaseComputerPartner()
{
    if (!bComputerOpen)
    {
        return;
    }

    TArray<FPartnerRecord>& ActiveList = bComputerSelectingStorage ? GetActiveStoragePartners() : PartyPartners;
    if (!ActiveList.IsValidIndex(ComputerSelectionIndex))
    {
        return;
    }

    const FPartnerRecord& Candidate = ActiveList[ComputerSelectionIndex];
    if (!bReleaseConfirmationPending)
    {
        bReleaseConfirmationPending = true;
        bReleaseConfirmationStorage = bComputerSelectingStorage;
        ReleaseConfirmationIndex = ComputerSelectionIndex;
        ReleaseConfirmationPartnerId = Candidate.PartnerId;
        GetWorldTimerManager().SetTimer(
            ReleaseConfirmationTimer,
            this,
            &ACaptureLauncher::ClearReleaseConfirmation,
            4.0f,
            false);
        if (APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0))
        {
            if (ACaptureHUD* CaptureHUD = Cast<ACaptureHUD>(PlayerController->GetHUD()))
            {
                CaptureHUD->SetReleaseConfirmation(true, Candidate.DisplayName);
            }
        }
        return;
    }

    // X deliberately only opens the modal. Confirmation is a separate Y action
    // so a repeated accidental key press cannot delete a partner.
}

void ACaptureLauncher::ConfirmReleaseComputerPartner()
{
    if (!bComputerOpen || !bReleaseConfirmationPending)
    {
        return;
    }

    TArray<FPartnerRecord>& ActiveList = bReleaseConfirmationStorage ? GetActiveStoragePartners() : PartyPartners;
    if (!ActiveList.IsValidIndex(ReleaseConfirmationIndex)
        || bReleaseConfirmationStorage != bComputerSelectingStorage
        || ActiveList[ReleaseConfirmationIndex].PartnerId != ReleaseConfirmationPartnerId)
    {
        ClearReleaseConfirmation();
        return;
    }

    const FString ReleasedName = ActiveList[ReleaseConfirmationIndex].DisplayName;
    if (!bReleaseConfirmationStorage)
    {
        RecallPartner(ActiveList[ReleaseConfirmationIndex].PartnerId, false);
        if (SelectedPartnerIndex >= ReleaseConfirmationIndex)
        {
            SelectedPartnerIndex = FMath::Max(0, SelectedPartnerIndex - 1);
        }
    }

    ActiveList.RemoveAt(ReleaseConfirmationIndex);
    ClearReleaseConfirmation();
    ComputerSelectionIndex = ActiveList.IsEmpty()
        ? -1
        : FMath::Min(ReleaseConfirmationIndex, ActiveList.Num() - 1);
    if (!bReleaseConfirmationStorage && PartyPartners.IsEmpty())
    {
        SelectedPartnerIndex = -1;
    }

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(
            -1,
            2.0f,
            FColor(255, 120, 120),
            FString::Printf(TEXT("%s was released"), *ReleasedName)
        );
    }
    RefreshPartnerHUD();
    SavePartnerData();
}

void ACaptureLauncher::ToggleSelectedPartner()
{
    if (bPlayerDefeated || bComputerOpen || bHandAttackActive || bCaptureAimActive || bCaptureThrowPending)
    {
        return;
    }

    if (!PartyPartners.IsValidIndex(SelectedPartnerIndex))
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Yellow, TEXT("Your party is empty"));
        }
        return;
    }

    APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
    APawn* PlayerPawn = PlayerController ? PlayerController->GetPawn() : nullptr;
    if (!PlayerPawn || !GetWorld())
    {
        return;
    }

    const FPartnerRecord& Partner = PartyPartners[SelectedPartnerIndex];
    if (IsPartnerDeployed(Partner.PartnerId))
    {
        RecallPartner(Partner.PartnerId, true);
        RefreshPartnerHUD();
        return;
    }
    if (!Partner.AnimalClass)
    {
        return;
    }

    FVector Forward = PlayerPawn->GetActorForwardVector().GetSafeNormal2D();
    if (Forward.IsNearlyZero())
    {
        Forward = FVector::ForwardVector;
    }
    FActorSpawnParameters SpawnParameters;
    SpawnParameters.Owner = PlayerPawn;
    SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    AAnimalCharacter* SpawnedPartner = GetWorld()->SpawnActor<AAnimalCharacter>(
        Partner.AnimalClass,
        PlayerPawn->GetActorLocation() + Forward * 220.0f + FVector(0.0f, 0.0f, 35.0f),
        Forward.Rotation(),
        SpawnParameters
    );
    if (!SpawnedPartner)
    {
        return;
    }

    SpawnedPartner->SetCompanionState(PlayerPawn);
    SpawnedPartner->SetCompanionBehavior(Partner.BehaviorMode);
    DeployedPartners.Add(Partner.PartnerId, SpawnedPartner);
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Cyan, FString::Printf(TEXT("Go, %s!"), *Partner.DisplayName));
    }
    RefreshPartnerHUD();
}

void ACaptureLauncher::BeginComputerPointerDrag()
{
    APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
    float MouseX = 0.0f;
    float MouseY = 0.0f;
    if (PlayerController && PlayerController->GetMousePosition(MouseX, MouseY))
    {
        BeginStoragePointerDragAt(FVector2D(MouseX, MouseY));
    }
}

void ACaptureLauncher::BeginStoragePointerDragAt(const FVector2D& ScreenPosition)
{
    ClearReleaseConfirmation();
    APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
    ACaptureHUD* CaptureHUD = PlayerController ? Cast<ACaptureHUD>(PlayerController->GetHUD()) : nullptr;
    if (!CaptureHUD)
    {
        return;
    }

    bool bSourceStorage = true;
    int32 SourceIndex = -1;
    if (!CaptureHUD->GetComputerSlotAtScreenPosition(ScreenPosition, bSourceStorage, SourceIndex))
    {
        UE_LOG(LogTemp, Verbose, TEXT("Storage drag did not start at raw pointer %.1f, %.1f"), ScreenPosition.X, ScreenPosition.Y);
        return;
    }

    const TArray<FPartnerRecord>& SourceList = bSourceStorage ? GetActiveStoragePartners() : PartyPartners;
    if (!SourceList.IsValidIndex(SourceIndex))
    {
        return;
    }

    bComputerDragActive = true;
    bComputerDragSourceStorage = bSourceStorage;
    ComputerDragSourceIndex = SourceIndex;
    bComputerSelectingStorage = bSourceStorage;
    ComputerSelectionIndex = SourceIndex;
    CaptureHUD->SetComputerDragState(true, SourceList[SourceIndex].DisplayName);
    UE_LOG(LogTemp, Log, TEXT("Storage drag began for %s"), *SourceList[SourceIndex].DisplayName);
    RefreshPartnerHUD();
}

void ACaptureLauncher::EndComputerPointerDrag()
{
    APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
    float MouseX = 0.0f;
    float MouseY = 0.0f;
    if (PlayerController && PlayerController->GetMousePosition(MouseX, MouseY))
    {
        EndStoragePointerDragAt(FVector2D(MouseX, MouseY));
    }
}

void ACaptureLauncher::EndStoragePointerDragAt(const FVector2D& ScreenPosition)
{
    if (!bComputerDragActive)
    {
        return;
    }

    APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
    ACaptureHUD* CaptureHUD = PlayerController ? Cast<ACaptureHUD>(PlayerController->GetHUD()) : nullptr;
    bool bTargetStorage = true;
    int32 TargetIndex = -1;
    if (CaptureHUD && CaptureHUD->GetComputerSlotAtScreenPosition(ScreenPosition, bTargetStorage, TargetIndex))
    {
        MoveComputerPartner(bComputerDragSourceStorage, ComputerDragSourceIndex, bTargetStorage, TargetIndex);
        UE_LOG(LogTemp, Log, TEXT("Storage drag released on slot %d"), TargetIndex);
    }

    bComputerDragActive = false;
    ComputerDragSourceIndex = -1;
    if (CaptureHUD)
    {
        CaptureHUD->SetComputerDragState(false, FString());
    }
    RefreshPartnerHUD();
}

void ACaptureLauncher::ClearReleaseConfirmation()
{
    bReleaseConfirmationPending = false;
    ReleaseConfirmationIndex = -1;
    ReleaseConfirmationPartnerId.Empty();
    GetWorldTimerManager().ClearTimer(ReleaseConfirmationTimer);
    if (APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0))
    {
        if (ACaptureHUD* CaptureHUD = Cast<ACaptureHUD>(PlayerController->GetHUD()))
        {
            CaptureHUD->SetReleaseConfirmation(false, FString());
        }
    }
}

void ACaptureLauncher::MoveComputerPartner(bool bSourceStorage, int32 SourceIndex, bool bTargetStorage, int32 TargetIndex)
{
    TArray<FPartnerRecord>& StoragePartners = GetActiveStoragePartners();
    TArray<FPartnerRecord>& SourceList = bSourceStorage ? StoragePartners : PartyPartners;
    TArray<FPartnerRecord>& TargetList = bTargetStorage ? StoragePartners : PartyPartners;
    if (!SourceList.IsValidIndex(SourceIndex) || TargetIndex < 0)
    {
        return;
    }

    if (bBaseStorageOpen && !bSourceStorage && bTargetStorage
        && !StoragePartners.IsValidIndex(TargetIndex) && StoragePartners.Num() >= 6)
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Yellow, TEXT("This base can only house 6 partners"));
        }
        return;
    }

    if (bSourceStorage == bTargetStorage)
    {
        if (TargetList.IsValidIndex(TargetIndex) && TargetIndex != SourceIndex)
        {
            TargetList.Swap(SourceIndex, TargetIndex);
            ComputerSelectionIndex = TargetIndex;
            SavePartnerData();
        }
        return;
    }

    if (!bSourceStorage && bTargetStorage)
    {
        // A creature cannot remain deployed after it is moved to storage.
        RecallPartner(PartyPartners[SourceIndex].PartnerId, false);
        if (StoragePartners.IsValidIndex(TargetIndex))
        {
            Swap(PartyPartners[SourceIndex], StoragePartners[TargetIndex]);
            ComputerSelectionIndex = TargetIndex;
        }
        else
        {
            StoragePartners.Add(PartyPartners[SourceIndex]);
            PartyPartners.RemoveAt(SourceIndex);
            ComputerSelectionIndex = StoragePartners.Num() - 1;
        }
    }
    else if (bSourceStorage && !bTargetStorage)
    {
        if (PartyPartners.IsValidIndex(TargetIndex))
        {
            // The party creature travelling to storage must first return.
            RecallPartner(PartyPartners[TargetIndex].PartnerId, false);
            Swap(StoragePartners[SourceIndex], PartyPartners[TargetIndex]);
            ComputerSelectionIndex = TargetIndex;
        }
        else if (PartyPartners.Num() < 6)
        {
            PartyPartners.Add(StoragePartners[SourceIndex]);
            StoragePartners.RemoveAt(SourceIndex);
            ComputerSelectionIndex = PartyPartners.Num() - 1;
        }
        else
        {
            return;
        }
    }

    SelectedPartnerIndex = PartyPartners.IsValidIndex(SelectedPartnerIndex)
        ? SelectedPartnerIndex
        : (PartyPartners.IsEmpty() ? -1 : PartyPartners.Num() - 1);
    SavePartnerData();
    SyncActiveBasePartners();
}

void ACaptureLauncher::MovePartnerFromUMG(bool bSourceStorage, int32 SourceIndex, bool bTargetStorage, int32 TargetIndex)
{
    if (!bComputerOpen)
    {
        return;
    }

    MoveComputerPartner(bSourceStorage, SourceIndex, bTargetStorage, TargetIndex);
    PopulatePartnerStorageWidget();
    RefreshPartnerHUD();
}

TArray<FPartnerRecord>& ACaptureLauncher::GetActiveStoragePartners()
{
    if (bBaseStorageOpen)
    {
        return ActiveBaseStorageId.IsEmpty()
            ? BasePartners
            : BasePartnersByTerminal.FindOrAdd(ActiveBaseStorageId);
    }
    return ComputerPartners;
}

const TArray<FPartnerRecord>& ACaptureLauncher::GetActiveStoragePartners() const
{
    if (bBaseStorageOpen && !ActiveBaseStorageId.IsEmpty())
    {
        if (const TArray<FPartnerRecord>* BaseStorage = BasePartnersByTerminal.Find(ActiveBaseStorageId))
        {
            return *BaseStorage;
        }
    }
    return bBaseStorageOpen ? BasePartners : ComputerPartners;
}

void ACaptureLauncher::MigrateLegacyBasePartners()
{
    if (BasePartners.IsEmpty() || ActiveBaseStorageId.IsEmpty())
    {
        return;
    }

    TArray<FPartnerRecord>& FirstBaseStorage = BasePartnersByTerminal.FindOrAdd(ActiveBaseStorageId);
    if (FirstBaseStorage.IsEmpty())
    {
        FirstBaseStorage = MoveTemp(BasePartners);
        FirstBaseStorage.SetNum(FMath::Min(FirstBaseStorage.Num(), 6));
        SavePartnerData();
    }
}

void ACaptureLauncher::SpawnStoredBasePartners()
{
    if (!GetWorld())
    {
        return;
    }

    for (TActorIterator<ACaptureBaseTerminal> It(GetWorld()); It; ++It)
    {
        SyncBasePartnerActors(*It);
    }
}

void ACaptureLauncher::SyncActiveBasePartners()
{
    if (bBaseStorageOpen && AvailableBaseTerminal.IsValid())
    {
        SyncBasePartnerActors(AvailableBaseTerminal.Get());
    }
}

void ACaptureLauncher::SyncBasePartnerActors(ACaptureBaseTerminal* BaseTerminal)
{
    if (!BaseTerminal || !GetWorld())
    {
        return;
    }

    const FString StorageId = BaseTerminal->GetStorageId();
    const TArray<FPartnerRecord>* StoredPartners = BasePartnersByTerminal.Find(StorageId);
    TSet<FString> StoredIds;
    if (StoredPartners)
    {
        for (const FPartnerRecord& Partner : *StoredPartners)
        {
            StoredIds.Add(Partner.PartnerId);
        }
    }

    TMap<FString, TWeakObjectPtr<AAnimalCharacter>>& SpawnedPartners = DeployedBasePartners.FindOrAdd(StorageId);
    for (auto It = SpawnedPartners.CreateIterator(); It; ++It)
    {
        if (!StoredIds.Contains(It.Key()) || !It.Value().IsValid())
        {
            if (It.Value().IsValid())
            {
                It.Value()->Destroy();
            }
            It.RemoveCurrent();
        }
    }

    if (!StoredPartners)
    {
        return;
    }

    for (int32 Index = 0; Index < StoredPartners->Num(); ++Index)
    {
        const FPartnerRecord& Partner = (*StoredPartners)[Index];
        if (!Partner.AnimalClass || (SpawnedPartners.Contains(Partner.PartnerId) && SpawnedPartners[Partner.PartnerId].IsValid()))
        {
            continue;
        }

        FActorSpawnParameters SpawnParameters;
        SpawnParameters.Owner = BaseTerminal;
        SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
        AAnimalCharacter* SpawnedPartner = GetWorld()->SpawnActor<AAnimalCharacter>(
            Partner.AnimalClass,
            BaseTerminal->GetPartnerSpawnLocation(Index),
            BaseTerminal->GetActorRotation(),
            SpawnParameters
        );
        if (SpawnedPartner)
        {
            SpawnedPartner->SetBaseResidentState(BaseTerminal, 3000.0f);
            SpawnedPartners.Add(Partner.PartnerId, SpawnedPartner);
        }
    }
}

void ACaptureLauncher::RecallPartner(const FString& PartnerId, bool bAnnounce)
{
    if (TWeakObjectPtr<AAnimalCharacter>* PartnerActor = DeployedPartners.Find(PartnerId))
    {
        if (PartnerActor->IsValid())
        {
            PartnerActor->Get()->Destroy();
        }
        DeployedPartners.Remove(PartnerId);
        if (bAnnounce && GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Cyan, TEXT("Partner returned to its ball"));
        }
    }
}

void ACaptureLauncher::RecallDefeatedPartner(AAnimalCharacter* PartnerActor)
{
    if (!PartnerActor)
    {
        return;
    }

    for (auto It = DeployedPartners.CreateIterator(); It; ++It)
    {
        if (It.Value().Get() != PartnerActor)
        {
            continue;
        }

        const FString PartnerId = It.Key();
        It.RemoveCurrent();
        PartnerActor->Destroy();
        SavePartnerData();
        RefreshPartnerHUD();
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Cyan, TEXT("A defeated partner returned to its ball"));
        }
        return;
    }

    PartnerActor->Destroy();
}

bool ACaptureLauncher::IsPartnerDeployed(const FString& PartnerId) const
{
    const TWeakObjectPtr<AAnimalCharacter>* PartnerActor = DeployedPartners.Find(PartnerId);
    return PartnerActor && PartnerActor->IsValid();
}

void ACaptureLauncher::CommandPartnersToDefend(AAnimalCharacter* Attacker)
{
    if (!Attacker)
    {
        return;
    }

    for (auto It = DeployedPartners.CreateIterator(); It; ++It)
    {
        if (It.Value().IsValid())
        {
            It.Value()->DefendOwnerAgainst(Attacker);
        }
        else
        {
            It.RemoveCurrent();
        }
    }
}

void ACaptureLauncher::CommandPartnersToAttackTarget(AAnimalCharacter* Target)
{
    if (!Target)
    {
        return;
    }

    for (auto It = DeployedPartners.CreateIterator(); It; ++It)
    {
        if (It.Value().IsValid())
        {
            It.Value()->CommandAttackTarget(Target);
        }
        else
        {
            It.RemoveCurrent();
        }
    }
}

void ACaptureLauncher::BeginPartnerCommandHold()
{
    if (bPartnerCommandMenuOpen)
    {
        ClosePartnerCommandMenu();
        return;
    }

    if (bInventoryOpen)
    {
        CloseInventory();
        return;
    }

    if (bComputerOpen || bHandAttackActive || bCaptureAimActive || bCaptureThrowPending)
    {
        return;
    }

    if (!FindCommandedPartner())
    {
        OpenInventory();
        return;
    }

    bPartnerCommandHoldActive = true;
    GetWorldTimerManager().SetTimer(PartnerCommandHoldTimer, this, &ACaptureLauncher::OpenPartnerCommandMenu, 0.35f, false);
}

void ACaptureLauncher::EndPartnerCommandHold()
{
    if (bPartnerCommandMenuOpen)
    {
        return;
    }

    bPartnerCommandHoldActive = false;
    GetWorldTimerManager().ClearTimer(PartnerCommandHoldTimer);
}

AAnimalCharacter* ACaptureLauncher::FindCommandedPartner() const
{
    APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
    APawn* PlayerPawn = PlayerController ? PlayerController->GetPawn() : nullptr;
    if (!PlayerController || !PlayerPawn || !GetWorld())
    {
        return nullptr;
    }

    FVector ViewLocation;
    FRotator ViewRotation;
    PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);
    const FVector ViewDirection = ViewRotation.Vector();
    AAnimalCharacter* BestPartner = nullptr;
    float BestScore = TNumericLimits<float>::Max();
    for (TActorIterator<AAnimalCharacter> It(GetWorld()); It; ++It)
    {
        AAnimalCharacter* Candidate = *It;
        if (!Candidate || !Candidate->IsCompanionFor(PlayerPawn))
        {
            continue;
        }

        const FVector ToPartner = Candidate->GetActorLocation() - ViewLocation;
        const float Distance = ToPartner.Size();
        if (Distance > 700.0f || Distance < KINDA_SMALL_NUMBER)
        {
            continue;
        }

        const float FacingDot = FVector::DotProduct(ViewDirection, ToPartner / Distance);
        if (FacingDot < 0.78f)
        {
            continue;
        }

        const float Score = Distance - FacingDot * 220.0f;
        if (Score < BestScore)
        {
            BestScore = Score;
            BestPartner = Candidate;
        }
    }
    return BestPartner;
}

void ACaptureLauncher::OpenPartnerCommandMenu()
{
    if (!bPartnerCommandHoldActive || bPartnerCommandMenuOpen)
    {
        return;
    }

    AAnimalCharacter* Partner = FindCommandedPartner();
    APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
    if (!Partner || !PlayerController)
    {
        return;
    }

    bPartnerCommandMenuOpen = true;
    CommandedPartner = Partner;
    PlayerController->SetIgnoreMoveInput(true);
    PlayerController->SetIgnoreLookInput(true);
    PlayerController->bShowMouseCursor = true;
    PlayerController->bEnableClickEvents = true;
    FInputModeGameAndUI InputMode;
    InputMode.SetHideCursorDuringCapture(false);
    PlayerController->SetInputMode(InputMode);
    int32 ViewportX = 0;
    int32 ViewportY = 0;
    PlayerController->GetViewportSize(ViewportX, ViewportY);
    PlayerController->SetMouseLocation(ViewportX / 2, ViewportY / 2);

    UClass* WheelClass = StaticLoadClass(
        UUserWidget::StaticClass(),
        nullptr,
        TEXT("/Game/UI/WBP_PartnerCommandWheel.WBP_PartnerCommandWheel_C")
    );
    if (WheelClass)
    {
        PartnerCommandWheelWidget = CreateWidget<UUserWidget>(PlayerController, WheelClass);
        if (PartnerCommandWheelWidget)
        {
            if (FObjectProperty* PartnerProperty = FindFProperty<FObjectProperty>(WheelClass, TEXT("CommandedPartner")))
            {
                PartnerProperty->SetObjectPropertyValue_InContainer(PartnerCommandWheelWidget, Partner);
            }
            if (FObjectProperty* SourceProperty = FindFProperty<FObjectProperty>(WheelClass, TEXT("CommandSource")))
            {
                SourceProperty->SetObjectPropertyValue_InContainer(PartnerCommandWheelWidget, this);
            }
            PartnerCommandWheelWidget->AddToViewport(100);
        }
    }
}

void ACaptureLauncher::ClosePartnerCommandMenu()
{
    if (PartnerCommandWheelWidget)
    {
        PartnerCommandWheelWidget->RemoveFromParent();
        PartnerCommandWheelWidget = nullptr;
    }
    bPartnerCommandMenuOpen = false;
    bPartnerCommandHoldActive = false;
    CommandedPartner = nullptr;
    GetWorldTimerManager().ClearTimer(PartnerCommandHoldTimer);
    if (APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0))
    {
        PlayerController->SetIgnoreMoveInput(false);
        PlayerController->SetIgnoreLookInput(false);
        PlayerController->bShowMouseCursor = false;
        PlayerController->bEnableClickEvents = false;
        PlayerController->SetInputMode(FInputModeGameOnly());
        if (ACaptureHUD* CaptureHUD = Cast<ACaptureHUD>(PlayerController->GetHUD()))
        {
            CaptureHUD->SetPartnerCommandMenuState(false, FString());
        }
    }
}

void ACaptureLauncher::SelectPartnerCommandAtCursor()
{
    if (PartnerCommandWheelWidget)
    {
        return;
    }

    APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
    if (!PlayerController)
    {
        ClosePartnerCommandMenu();
        return;
    }

    float MouseX = 0.0f;
    float MouseY = 0.0f;
    if (!PlayerController->GetMousePosition(MouseX, MouseY))
    {
        return;
    }

    EPartnerBehaviorMode SelectedBehavior = EPartnerBehaviorMode::Neutral;
    if (ACaptureHUD* CaptureHUD = Cast<ACaptureHUD>(PlayerController->GetHUD());
        CaptureHUD && CaptureHUD->GetPartnerCommandAtScreenPosition(FVector2D(MouseX, MouseY), SelectedBehavior))
    {
        SetPartnerBehavior(CommandedPartner.Get(), SelectedBehavior);
        ClosePartnerCommandMenu();
    }
}

void ACaptureLauncher::ApplyPartnerBehaviorFromCommandWheel(EPartnerBehaviorMode BehaviorMode)
{
    if (!bPartnerCommandMenuOpen || !CommandedPartner.IsValid())
    {
        return;
    }

    SetPartnerBehavior(CommandedPartner.Get(), BehaviorMode);
    ClosePartnerCommandMenu();
}

void ACaptureLauncher::SetPartnerBehavior(AAnimalCharacter* PartnerActor, EPartnerBehaviorMode BehaviorMode)
{
    if (!PartnerActor)
    {
        return;
    }

    PartnerActor->SetCompanionBehavior(BehaviorMode);
    for (const TPair<FString, TWeakObjectPtr<AAnimalCharacter>>& Entry : DeployedPartners)
    {
        if (Entry.Value.Get() != PartnerActor)
        {
            continue;
        }
        for (FPartnerRecord& Partner : PartyPartners)
        {
            if (Partner.PartnerId == Entry.Key)
            {
                Partner.BehaviorMode = BehaviorMode;
                SavePartnerData();
                return;
            }
        }
    }
}

void ACaptureLauncher::RefreshPartnerHUD()
{
    APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
    ACaptureHUD* CaptureHUD = PlayerController ? Cast<ACaptureHUD>(PlayerController->GetHUD()) : nullptr;
    if (!CaptureHUD)
    {
        return;
    }

    TArray<FString> PartyNames;
    PartyNames.Reserve(PartyPartners.Num());
    for (const FPartnerRecord& Partner : PartyPartners)
    {
        PartyNames.Add(Partner.DisplayName);
    }

    const TArray<FPartnerRecord>& ActiveStorage = GetActiveStoragePartners();
    TArray<FString> StorageNames;
    StorageNames.Reserve(ActiveStorage.Num());
    for (const FPartnerRecord& Partner : ActiveStorage)
    {
        StorageNames.Add(Partner.DisplayName);
    }

    TArray<bool> PartyDeployed;
    PartyDeployed.Reserve(PartyPartners.Num());
    for (const FPartnerRecord& Partner : PartyPartners)
    {
        PartyDeployed.Add(IsPartnerDeployed(Partner.PartnerId));
    }

    CaptureHUD->SetPartnerState(
        PartyNames,
        StorageNames,
        SelectedPartnerIndex,
        bComputerOpen,
        bComputerSelectingStorage,
        ComputerSelectionIndex,
        PartyDeployed
    );
    CaptureHUD->SetStorageLabels(
        bBaseStorageOpen ? TEXT("BASE PARTNER CHEST") : TEXT("PARTNER COMPUTER"),
        bBaseStorageOpen ? TEXT("BASE STORAGE") : TEXT("COMPUTER STORAGE")
    );
}

void ACaptureLauncher::LoadPartnerData()
{
    const FString SaveSlotName = GetSaveSlotName();
    if (!UGameplayStatics::DoesSaveGameExist(SaveSlotName, 0))
    {
        return;
    }

    UCapturePartnerSaveGame* SaveGame = Cast<UCapturePartnerSaveGame>(
        UGameplayStatics::LoadGameFromSlot(SaveSlotName, 0));
    if (!SaveGame)
    {
        return;
    }

    auto LoadRecords = [](const TArray<FCapturedPartnerSaveData>& Source, TArray<FPartnerRecord>& Destination)
    {
        for (const FCapturedPartnerSaveData& SavedPartner : Source)
        {
            if (TSubclassOf<AAnimalCharacter> AnimalClass = SavedPartner.AnimalClass.TryLoadClass<AAnimalCharacter>())
            {
                FPartnerRecord Partner;
                Partner.AnimalClass = AnimalClass;
                Partner.DisplayName = SavedPartner.DisplayName;
                Partner.PartnerId = SavedPartner.PartnerId.IsEmpty()
                    ? FGuid::NewGuid().ToString(EGuidFormats::Digits)
                    : SavedPartner.PartnerId;
                Partner.BehaviorMode = static_cast<EPartnerBehaviorMode>(FMath::Min<uint8>(SavedPartner.BehaviorMode, static_cast<uint8>(EPartnerBehaviorMode::AttackMyTarget)));
                Destination.Add(Partner);
            }
        }
    };

    PartyPartners.Empty();
    ComputerPartners.Empty();
    BasePartners.Empty();
    BasePartnersByTerminal.Empty();
    LoadRecords(SaveGame->PartyPartners, PartyPartners);
    LoadRecords(SaveGame->ComputerPartners, ComputerPartners);
    LoadRecords(SaveGame->BasePartners, BasePartners);
    for (const TPair<FString, FBasePartnerStorageSaveData>& SavedBase : SaveGame->BasePartnersByTerminal)
    {
        TArray<FPartnerRecord>& BaseStorage = BasePartnersByTerminal.FindOrAdd(SavedBase.Key);
        LoadRecords(SavedBase.Value.Partners, BaseStorage);
        BaseStorage.SetNum(FMath::Min(BaseStorage.Num(), 6));
    }
    PartyPartners.SetNum(FMath::Min(PartyPartners.Num(), 6));
    SelectedPartnerIndex = PartyPartners.IsValidIndex(SaveGame->SelectedPartnerIndex)
        ? SaveGame->SelectedPartnerIndex
        : (PartyPartners.IsEmpty() ? -1 : 0);

    HotbarItems = SaveGame->HotbarItems;
    BackpackItems = SaveGame->BackpackItems;
    Hunger = SaveGame->Hunger;
    SelectedSlot = SaveGame->SelectedHotbarSlot;
}

void ACaptureLauncher::SavePartnerData() const
{
    UCapturePartnerSaveGame* SaveGame = Cast<UCapturePartnerSaveGame>(
        UGameplayStatics::CreateSaveGameObject(UCapturePartnerSaveGame::StaticClass()));
    if (!SaveGame)
    {
        return;
    }

    auto SaveRecords = [](const TArray<FPartnerRecord>& Source, TArray<FCapturedPartnerSaveData>& Destination)
    {
        Destination.Reserve(Source.Num());
        for (const FPartnerRecord& Partner : Source)
        {
            if (!Partner.AnimalClass)
            {
                continue;
            }

            FCapturedPartnerSaveData SavedPartner;
            SavedPartner.AnimalClass = FSoftClassPath(Partner.AnimalClass.Get());
            SavedPartner.DisplayName = Partner.DisplayName;
            SavedPartner.PartnerId = Partner.PartnerId;
            SavedPartner.BehaviorMode = static_cast<uint8>(Partner.BehaviorMode);
            Destination.Add(SavedPartner);
        }
    };

    SaveRecords(PartyPartners, SaveGame->PartyPartners);
    SaveRecords(ComputerPartners, SaveGame->ComputerPartners);
    SaveRecords(BasePartners, SaveGame->BasePartners);
    SaveGame->BasePartnersByTerminal.Empty();
    for (const TPair<FString, TArray<FPartnerRecord>>& BaseStorage : BasePartnersByTerminal)
    {
        FBasePartnerStorageSaveData& SavedBase = SaveGame->BasePartnersByTerminal.FindOrAdd(BaseStorage.Key);
        SaveRecords(BaseStorage.Value, SavedBase.Partners);
    }
    SaveGame->SelectedPartnerIndex = SelectedPartnerIndex;
    SaveGame->HotbarItems = HotbarItems;
    SaveGame->BackpackItems = BackpackItems;
    SaveGame->Hunger = Hunger;
    SaveGame->SelectedHotbarSlot = SelectedSlot;
    SaveGame->bCreativeMode = IsCreativeMode();
    UGameplayStatics::SaveGameToSlot(SaveGame, GetSaveSlotName(), 0);
}

void ACaptureLauncher::SelectHand()
{
    SelectHotbarSlot(0);
}

void ACaptureLauncher::SelectCaptureBall()
{
    SelectHotbarSlot(1);
}

void ACaptureLauncher::SelectSword()
{
    SelectHotbarSlot(2);
}

void ACaptureLauncher::SelectSlotFour() { SelectHotbarSlot(3); }
void ACaptureLauncher::SelectSlotFive() { SelectHotbarSlot(4); }
void ACaptureLauncher::SelectSlotSix() { SelectHotbarSlot(5); }
void ACaptureLauncher::SelectSlotSeven() { SelectHotbarSlot(6); }
void ACaptureLauncher::SelectSlotEight() { SelectHotbarSlot(7); }
void ACaptureLauncher::SelectSlotNine() { SelectHotbarSlot(8); }

void ACaptureLauncher::SelectHotbarSlot(int32 NewSlot)
{
    if (bComputerOpen || bInventoryOpen || bHandAttackActive || bCaptureThrowPending || bEating || !HotbarItems.IsValidIndex(NewSlot))
    {
        return;
    }

    if (bCaptureAimActive)
    {
        bCaptureAimActive = false;
        RestorePlayerAnimation();
    }

    const bool bWasSwordSelected = GetSelectedItemType() == EInventoryItemType::GreatSword;
    const bool bWillSelectSword = HotbarItems[NewSlot].ItemType == EInventoryItemType::GreatSword;
    if (bWasSwordSelected && !bWillSelectSword)
    {
        // Leaving the sword must return to the character animation blueprint;
        // otherwise the last sword stance remains frozen after an item switch.
        StopSwordLocomotion();
    }
    SelectedSlot = NewSlot;
    UpdateHotbar();
    if (!bWasSwordSelected && GetSelectedItemType() == EInventoryItemType::GreatSword && SwordDrawAnimation)
    {
        CurrentSwordLocomotionAnimation = nullptr;
        bSwordWasFalling = false;
        if (APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0))
        {
            LastSwordControlYaw = PlayerController->GetControlRotation().Yaw;
        }
        PlayPlayerAnimation(SwordDrawAnimation, false);
        SwordLocomotionLockUntil = GetWorld()->GetTimeSeconds()
            + SwordDrawAnimation->GetPlayLength()
            + (SwordDrawFollowUpAnimation ? SwordDrawFollowUpAnimation->GetPlayLength() : 0.0f);
        if (SwordDrawFollowUpAnimation)
        {
            GetWorldTimerManager().SetTimer(
                SwordDrawFollowUpTimer,
                this,
                &ACaptureLauncher::PlaySwordDrawFollowUp,
                FMath::Max(0.01f, SwordDrawAnimation->GetPlayLength()),
                false
            );
        }
    }
}

void ACaptureLauncher::PlaySwordDrawFollowUp()
{
    if (GetSelectedItemType() == EInventoryItemType::GreatSword && SwordDrawFollowUpAnimation && !bPlayerDefeated && !bHandAttackActive)
    {
        PlayPlayerAnimation(SwordDrawFollowUpAnimation, false);
    }
}

void ACaptureLauncher::UpdateSwordLocomotion()
{
    if (GetSelectedItemType() != EInventoryItemType::GreatSword || bHandAttackActive || bCaptureAimActive || bCaptureThrowPending
        || bPlayerDefeated || bComputerOpen || bBaseStorageOpen || bPartnerCommandMenuOpen || !GetWorld())
    {
        return;
    }

    APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
    ACharacter* PlayerCharacter = Cast<ACharacter>(PlayerController ? PlayerController->GetPawn() : nullptr);
    UCharacterMovementComponent* Movement = PlayerCharacter ? PlayerCharacter->GetCharacterMovement() : nullptr;
    if (!PlayerController || !PlayerCharacter || !Movement || GetWorld()->GetTimeSeconds() < SwordLocomotionLockUntil)
    {
        return;
    }

    if (Movement->IsFalling())
    {
        if (!bSwordWasFalling)
        {
            bSwordWasFalling = true;
            CurrentSwordLocomotionAnimation = nullptr;
            const bool bMovingJump = PlayerCharacter->GetVelocity().SizeSquared2D() > FMath::Square(80.0f);
            UAnimSequence* JumpAnimation = bMovingJump
                ? SwordJumpMovingAnimation.Get()
                : SwordJumpIdleAnimation.Get();
            PlayPlayerAnimation(JumpAnimation, false);
            SwordLocomotionLockUntil = GetWorld()->GetTimeSeconds()
                + (JumpAnimation ? JumpAnimation->GetPlayLength() : 0.1f);
        }
        return;
    }

    if (bSwordWasFalling)
    {
        bSwordWasFalling = false;
        CurrentSwordLocomotionAnimation = nullptr;
    }

    const FVector Velocity = PlayerCharacter->GetVelocity();
    const float Speed = Velocity.Size2D();
    const float CurrentYaw = PlayerController->GetControlRotation().Yaw;
    if (Speed < 8.0f)
    {
        const float YawDelta = FMath::FindDeltaAngleDegrees(LastSwordControlYaw, CurrentYaw);
        if (FMath::Abs(YawDelta) >= 150.0f)
        {
            UAnimSequence* TurnAnimation = YawDelta > 0.0f
                ? SwordTurn180RightAnimation.Get()
                : SwordTurn180LeftAnimation.Get();
            LastSwordControlYaw = CurrentYaw;
            CurrentSwordLocomotionAnimation = nullptr;
            PlayPlayerAnimation(TurnAnimation, false);
            SwordLocomotionLockUntil = GetWorld()->GetTimeSeconds()
                + (TurnAnimation ? TurnAnimation->GetPlayLength() : 0.1f);
            return;
        }
        if (FMath::Abs(YawDelta) >= 70.0f)
        {
            UAnimSequence* TurnAnimation = YawDelta > 0.0f
                ? SwordTurnRightAnimation.Get()
                : SwordTurnLeftAnimation.Get();
            LastSwordControlYaw = CurrentYaw;
            CurrentSwordLocomotionAnimation = nullptr;
            PlayPlayerAnimation(TurnAnimation, false);
            SwordLocomotionLockUntil = GetWorld()->GetTimeSeconds()
                + (TurnAnimation ? TurnAnimation->GetPlayLength() : 0.1f);
            return;
        }
        LastSwordControlYaw = CurrentYaw;
        PlaySwordLocomotionAnimation(GetSwordIdleAnimation());
        return;
    }

    LastSwordControlYaw = CurrentYaw;
    const FVector MovementDirection = Velocity.GetSafeNormal2D();
    const FRotator ControlYaw(0.0f, CurrentYaw, 0.0f);
    const FVector Forward = FRotationMatrix(ControlYaw).GetUnitAxis(EAxis::X);
    const FVector Right = FRotationMatrix(ControlYaw).GetUnitAxis(EAxis::Y);
    const float ForwardAmount = FVector::DotProduct(MovementDirection, Forward);
    const float RightAmount = FVector::DotProduct(MovementDirection, Right);
    const bool bRunning = Movement->MaxWalkSpeed >= 650.0f;

    if (ForwardAmount >= 0.6f)
    {
        PlaySwordLocomotionAnimation(bRunning ? SwordRunForwardAnimation.Get() : SwordWalkForwardAnimation.Get());
    }
    else if (ForwardAmount <= -0.6f)
    {
        PlaySwordLocomotionAnimation(bRunning ? SwordRunBackwardAnimation.Get() : SwordWalkBackwardAnimation.Get());
    }
    else if (RightAmount >= 0.0f)
    {
        PlaySwordLocomotionAnimation(bRunning ? SwordStrafeRightAltAnimation.Get() : SwordStrafeRightAnimation.Get());
    }
    else
    {
        PlaySwordLocomotionAnimation(bRunning ? SwordStrafeLeftAltAnimation.Get() : SwordStrafeLeftAnimation.Get());
    }
}

void ACaptureLauncher::PlaySwordLocomotionAnimation(UAnimSequence* Animation)
{
    if (!Animation)
    {
        return;
    }

    ACharacter* PlayerCharacter = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
    USkeletalMeshComponent* PlayerMesh = PlayerCharacter ? PlayerCharacter->GetMesh() : nullptr;
    if (!PlayerMesh)
    {
        return;
    }

    if (CurrentSwordLocomotionAnimation == Animation)
    {
        // These imported locomotion clips include a recovery tail. A normal
        // full-sequence loop returns through that tail and causes the visible
        // flash reported during sustained movement. Wrap just before it.
        if (UAnimSingleNodeInstance* SingleNode = PlayerMesh->GetSingleNodeInstance())
        {
            const float Length = Animation->GetPlayLength();
            const float LoopStart = FMath::Min(0.06f, Length * 0.1f);
            const float LoopEnd = FMath::Max(LoopStart + 0.02f, Length - FMath::Clamp(Length * 0.12f, 0.08f, 0.18f));
            const float CurrentTime = SingleNode->GetCurrentTime();
            if (CurrentTime >= LoopEnd)
            {
                SingleNode->SetPositionWithPreviousTime(LoopStart, CurrentTime, false);
            }
        }
        return;
    }

    GetWorldTimerManager().ClearTimer(AnimationRestoreTimer);
    PlayerMesh->PlayAnimation(Animation, true);
    if (UAnimSingleNodeInstance* SingleNode = PlayerMesh->GetSingleNodeInstance())
    {
        SingleNode->SetLooping(true);
        SingleNode->SetPosition(0.06f, false);
    }
    CurrentSwordLocomotionAnimation = Animation;
}

void ACaptureLauncher::StopSwordLocomotion()
{
    GetWorldTimerManager().ClearTimer(SwordDrawFollowUpTimer);
    SwordLocomotionLockUntil = 0.0f;
    bSwordWasFalling = false;
    CurrentSwordLocomotionAnimation = nullptr;
    RestorePlayerAnimation();
}

UAnimSequence* ACaptureLauncher::GetSwordIdleAnimation() const
{
    TArray<UAnimSequence*> IdleAnimations = {
        SwordIdleAnimation01.Get(),
        SwordIdleAnimation02.Get(),
        SwordIdleAnimation03.Get(),
        SwordIdleAnimation04.Get(),
        SwordIdleAnimation05.Get()
    };
    IdleAnimations.RemoveAll([](const UAnimSequence* Animation) { return Animation == nullptr; });
    for (UAnimSequence* Animation : IdleAnimations)
    {
        if (CurrentSwordLocomotionAnimation == Animation)
        {
            return Animation;
        }
    }
    return IdleAnimations.Num() > 0 ? IdleAnimations[FMath::RandRange(0, IdleAnimations.Num() - 1)] : nullptr;
}

void ACaptureLauncher::CycleHotbar(int32 Direction)
{
    if (bComputerOpen || bHandAttackActive || bCaptureThrowPending || Direction == 0)
    {
        return;
    }

    int32 NextSlot = (SelectedSlot + Direction) % 9;
    if (NextSlot < 0)
    {
        NextSlot += 9;
    }
    SelectHotbarSlot(NextSlot);
}

void ACaptureLauncher::SelectPreviousHotbarItem()
{
    CycleHotbar(-1);
}

void ACaptureLauncher::SelectNextHotbarItem()
{
    CycleHotbar(1);
}

void ACaptureLauncher::UpdateHotbar()
{
    if (APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0))
    {
        if (ACaptureHUD* CaptureHUD = Cast<ACaptureHUD>(PlayerController->GetHUD()))
        {
            CaptureHUD->SetInventoryState(SelectedSlot, PlayerHealth, Hunger, HotbarItems);
            CaptureHUD->SetCaptureAimState(bCaptureAimActive);
        }
    }
    UpdateHeldItemVisual();
    RefreshPartnerHUD();
}

void ACaptureLauncher::CreateHeldItemVisual()
{
    APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
    ACharacter* PlayerCharacter = Cast<ACharacter>(PlayerPawn);
    if (!PlayerCharacter)
    {
        return;
    }

    USkeletalMeshComponent* PlayerMesh = PlayerCharacter->GetMesh();
    if (!PlayerMesh)
    {
        return;
    }

    const FName HandAttachment(TEXT("hand_r"));
    const bool bHasHandAttachment = PlayerMesh->DoesSocketExist(HandAttachment)
        || PlayerMesh->GetBoneIndex(HandAttachment) != INDEX_NONE;

    if (!HeldBallMesh)
    {
        HeldBallMesh = NewObject<UStaticMeshComponent>(PlayerCharacter, TEXT("HeldCaptureBall"));
        HeldBallMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        HeldBallMesh->SetCastShadow(false);
        if (UStaticMesh* BallMeshAsset = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Fab/Ball/ball/StaticMeshes/Sphere001_Material__26_0")))
        {
            HeldBallMesh->SetStaticMesh(BallMeshAsset);
        }
        HeldBallMesh->RegisterComponent();
        HeldBallMesh->AttachToComponent(
            PlayerMesh,
            FAttachmentTransformRules::SnapToTargetNotIncludingScale,
            bHasHandAttachment ? HandAttachment : NAME_None
        );
        // The source mesh is exceptionally large. Keep the held version inside
        // Manny's palm instead of using the projectile's larger world scale.
        HeldBallMesh->SetRelativeScale3D(FVector(0.00072f));
        // hand_r is the wrist bone. Move only along its palm axis so the ball
        // remains in the hand through every sword and throw pose.
        HeldBallMesh->SetRelativeLocation(FVector(5.5f, 0.0f, 0.0f));
        HeldBallMesh->SetRelativeRotation(FRotator::ZeroRotator);
        HeldBallMesh->SetVisibility(false, true);
    }

    if (!HeldSwordMesh)
    {
        HeldSwordMesh = NewObject<UStaticMeshComponent>(PlayerCharacter, TEXT("HeldGreatSword"));
        HeldSwordMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        HeldSwordMesh->SetCastShadow(true);
        if (UStaticMesh* SwordMeshAsset = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Sword/Sword/SM_Sword.SM_Sword")))
        {
            HeldSwordMesh->SetStaticMesh(SwordMeshAsset);
        }
        HeldSwordMesh->RegisterComponent();
        HeldSwordMesh->AttachToComponent(
            PlayerMesh,
            FAttachmentTransformRules::SnapToTargetNotIncludingScale,
            bHasHandAttachment ? HandAttachment : NAME_None
        );
        // The sword asset's pivot is the hilt. Align that pivot exactly to the
        // hand bone so it inherits the real animated hand transform.
        HeldSwordMesh->SetRelativeLocation(FVector::ZeroVector);
        HeldSwordMesh->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
        HeldSwordMesh->SetVisibility(false, true);
    }
}

void ACaptureLauncher::UpdateHeldItemVisual()
{
    if (!HeldBallMesh)
    {
        CreateHeldItemVisual();
    }
    if (HeldBallMesh)
    {
        HeldBallMesh->SetVisibility(GetSelectedItemType() == EInventoryItemType::CaptureBall, true);
    }
    if (HeldSwordMesh)
    {
        HeldSwordMesh->SetVisibility(GetSelectedItemType() == EInventoryItemType::GreatSword, true);
    }
}

void ACaptureLauncher::ConfigurePlayerAnimations()
{
    HandAttackAnimation = LoadObject<UAnimSequence>(
        nullptr,
        TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_01")
    );
    SwordDrawAnimation = LoadObject<UAnimSequence>(
        nullptr,
        TEXT("/Game/Characters/Mannequins/Anims/sword/Manny/MM_GreatSword_Draw")
    );
    SwordDrawFollowUpAnimation = LoadObject<UAnimSequence>(
        nullptr,
        TEXT("/Game/Characters/Mannequins/Anims/sword/Manny/MM_GreatSword_Draw_FollowUp")
    );
    SwordAttackAnimation = LoadObject<UAnimSequence>(
        nullptr,
        TEXT("/Game/Characters/Mannequins/Anims/sword/Manny/MM_GreatSword_Attack_Left")
    );
    SwordSlashAnimation = LoadObject<UAnimSequence>(
        nullptr,
        TEXT("/Game/Characters/Mannequins/Anims/sword/Manny/MM_GreatSword_Attack_Right")
    );
    SwordIdleAnimation01 = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Characters/Mannequins/Anims/sword/Manny/MM_GreatSword_Idle_01"));
    SwordIdleAnimation02 = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Characters/Mannequins/Anims/sword/Manny/MM_GreatSword_Idle_02"));
    SwordIdleAnimation03 = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Characters/Mannequins/Anims/sword/Manny/MM_GreatSword_Idle_03"));
    SwordIdleAnimation04 = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Characters/Mannequins/Anims/sword/Manny/MM_GreatSword_Idle_04"));
    SwordIdleAnimation05 = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Characters/Mannequins/Anims/sword/Manny/MM_GreatSword_Idle_05"));
    SwordWalkForwardAnimation = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Characters/Mannequins/Anims/sword/Manny/MM_GreatSword_Walk_Forward"));
    SwordWalkBackwardAnimation = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Characters/Mannequins/Anims/sword/Manny/MM_GreatSword_Walk_Backward"));
    SwordRunForwardAnimation = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Characters/Mannequins/Anims/sword/Manny/MM_GreatSword_Run_Forward"));
    SwordRunBackwardAnimation = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Characters/Mannequins/Anims/sword/Manny/MM_GreatSword_Run_Backward"));
    SwordStrafeRightAnimation = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Characters/Mannequins/Anims/sword/Manny/MM_GreatSword_Strafe_Right"));
    SwordStrafeLeftAnimation = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Characters/Mannequins/Anims/sword/Manny/MM_GreatSword_Strafe_Left"));
    SwordStrafeRightAltAnimation = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Characters/Mannequins/Anims/sword/Manny/MM_GreatSword_Strafe_Right_Alt"));
    SwordStrafeLeftAltAnimation = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Characters/Mannequins/Anims/sword/Manny/MM_GreatSword_Strafe_Left_Alt"));
    SwordTurnRightAnimation = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Characters/Mannequins/Anims/sword/Manny/MM_GreatSword_Turn_Right"));
    SwordTurnLeftAnimation = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Characters/Mannequins/Anims/sword/Manny/MM_GreatSword_Turn_Left"));
    SwordTurn180RightAnimation = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Characters/Mannequins/Anims/sword/Manny/MM_GreatSword_Turn180_Right"));
    SwordTurn180LeftAnimation = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Characters/Mannequins/Anims/sword/Manny/MM_GreatSword_Turn180_Left"));
    SwordJumpMovingAnimation = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Characters/Mannequins/Anims/sword/Manny/MM_GreatSword_Jump_Start"));
    SwordJumpIdleAnimation = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Characters/Mannequins/Anims/sword/Manny/MM_GreatSword_Jump_Land"));
    CaptureAimAnimation = LoadObject<UAnimSequence>(
        nullptr,
        TEXT("/Game/FreeAnimationsPack2/Animations/AS_LevitatingFireballCharge2")
    );
    CaptureThrowAnimation = LoadObject<UAnimSequence>(
        nullptr,
        TEXT("/Game/Characters/Mannequins/Anims/throw/MM_Throw_Retargeted")
    );
    PlayerDeathAnimation = LoadObject<UAnimSequence>(
        nullptr,
        TEXT("/Game/Characters/Mannequins/Anims/Death/MM_Death_Back_01")
    );
    EatAnimation = LoadObject<UAnimSequence>(
        nullptr,
        TEXT("/Game/FreeAnimationsPack2/Animations/AS_Eats")
    );

    ACharacter* PlayerCharacter = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
    USkeletalMeshComponent* PlayerMesh = PlayerCharacter ? PlayerCharacter->GetMesh() : nullptr;
    USkeleton* TargetSkeleton = PlayerMesh && PlayerMesh->GetSkeletalMeshAsset()
        ? PlayerMesh->GetSkeletalMeshAsset()->GetSkeleton()
        : nullptr;
    if (!TargetSkeleton)
    {
        return;
    }

    for (UAnimSequence* Animation : {
        HandAttackAnimation.Get(),
        SwordDrawAnimation.Get(),
        SwordDrawFollowUpAnimation.Get(),
        SwordAttackAnimation.Get(),
        SwordSlashAnimation.Get(),
        SwordIdleAnimation01.Get(), SwordIdleAnimation02.Get(), SwordIdleAnimation03.Get(), SwordIdleAnimation04.Get(), SwordIdleAnimation05.Get(),
        SwordWalkForwardAnimation.Get(), SwordWalkBackwardAnimation.Get(), SwordRunForwardAnimation.Get(), SwordRunBackwardAnimation.Get(),
        SwordStrafeRightAnimation.Get(), SwordStrafeLeftAnimation.Get(), SwordStrafeRightAltAnimation.Get(), SwordStrafeLeftAltAnimation.Get(),
        SwordTurnRightAnimation.Get(), SwordTurnLeftAnimation.Get(), SwordTurn180RightAnimation.Get(), SwordTurn180LeftAnimation.Get(),
        SwordJumpMovingAnimation.Get(), SwordJumpIdleAnimation.Get(),
        CaptureAimAnimation.Get(),
        CaptureThrowAnimation.Get(),
        PlayerDeathAnimation.Get()
        , EatAnimation.Get()
    })
    {
        if (Animation && Animation->GetSkeleton())
        {
            // Some aim and throw assets use a related mannequin skeleton. Marking the source
            // compatible lets UE map shared humanoid bones and ignore source-only bones.
            TargetSkeleton->AddCompatibleSkeleton(Animation->GetSkeleton());
        }
    }
}

void ACaptureLauncher::PlayPlayerAnimation(UAnimSequence* Animation, bool bLooping)
{
    ACharacter* PlayerCharacter = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
    USkeletalMeshComponent* PlayerMesh = PlayerCharacter ? PlayerCharacter->GetMesh() : nullptr;
    if (!PlayerMesh || !Animation)
    {
        return;
    }

    GetWorldTimerManager().ClearTimer(AnimationRestoreTimer);
    PlayerMesh->PlayAnimation(Animation, bLooping);
    if (!bLooping)
    {
        GetWorldTimerManager().SetTimer(
            AnimationRestoreTimer,
            this,
            &ACaptureLauncher::RestorePlayerAnimation,
            FMath::Max(0.1f, Animation->GetPlayLength()),
            false
        );
    }
}

void ACaptureLauncher::RestorePlayerAnimation()
{
    if (bPlayerDefeated || bCaptureAimActive || bCaptureThrowPending || bHandAttackActive)
    {
        return;
    }

    ACharacter* PlayerCharacter = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
    if (USkeletalMeshComponent* PlayerMesh = PlayerCharacter ? PlayerCharacter->GetMesh() : nullptr)
    {
        PlayerMesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
    }
    bEating = false;
    UpdateHeldItemVisual();
}

void ACaptureLauncher::StartPlayerDefeat()
{
    if (bPlayerDefeated)
    {
        return;
    }

    bPlayerDefeated = true;
    if (APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0))
    {
        PlayerController->SetIgnoreMoveInput(true);
        PlayerController->SetIgnoreLookInput(true);
    }

    if (ACharacter* PlayerCharacter = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
    {
        PlayerCharacter->GetCharacterMovement()->StopMovementImmediately();
        PlayerCharacter->GetCharacterMovement()->DisableMovement();
    }

    PlayPlayerAnimation(PlayerDeathAnimation, false);
    const float RespawnDelay = PlayerDeathAnimation ? FMath::Max(0.1f, PlayerDeathAnimation->GetPlayLength()) : 2.0f;
    GetWorldTimerManager().SetTimer(PlayerRespawnTimer, this, &ACaptureLauncher::RespawnPlayer, RespawnDelay, false);
}

void ACaptureLauncher::RespawnPlayer()
{
    APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
    APawn* PlayerPawn = PlayerController ? PlayerController->GetPawn() : nullptr;
    if (PlayerPawn)
    {
        if (AActor* StartActor = UGameplayStatics::GetActorOfClass(this, APlayerStart::StaticClass()))
        {
            PlayerPawn->SetActorLocation(StartActor->GetActorLocation() + FVector(0.0f, 0.0f, 100.0f));
        }
        if (ACharacter* PlayerCharacter = Cast<ACharacter>(PlayerPawn))
        {
            PlayerCharacter->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
            if (USkeletalMeshComponent* PlayerMesh = PlayerCharacter->GetMesh())
            {
                PlayerMesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
            }
        }
    }

    PlayerHealth = 30.0f;
    bPlayerDefeated = false;
    if (PlayerController)
    {
        PlayerController->SetIgnoreMoveInput(false);
        PlayerController->SetIgnoreLookInput(false);
    }
    UpdateHotbar();
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("You were defeated and returned to the start"));
    }
}

void ACaptureLauncher::ReceiveAnimalAttack(float Damage, const FVector& AttackOrigin)
{
    if (bPlayerDefeated || IsCreativeMode())
    {
        return;
    }
    APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
    APawn* PlayerPawn = PlayerController ? PlayerController->GetPawn() : nullptr;
    if (!PlayerPawn)
    {
        return;
    }

    PlayerHealth = FMath::Max(0.0f, PlayerHealth - Damage);
    AAnimalCharacter* Attacker = nullptr;
    float ClosestDistanceSquared = FMath::Square(300.0f);
    for (TActorIterator<AAnimalCharacter> It(GetWorld()); It; ++It)
    {
        AAnimalCharacter* Candidate = *It;
        if (!Candidate || Candidate->IsCompanion() || Candidate->IsInactive())
        {
            continue;
        }
        const float DistanceSquared = FVector::DistSquared(Candidate->GetActorLocation(), AttackOrigin);
        if (DistanceSquared <= ClosestDistanceSquared)
        {
            Attacker = Candidate;
            ClosestDistanceSquared = DistanceSquared;
        }
    }
    CommandPartnersToDefend(Attacker);
    if (ACharacter* PlayerCharacter = Cast<ACharacter>(PlayerPawn))
    {
        FVector KnockbackDirection = PlayerPawn->GetActorLocation() - AttackOrigin;
        KnockbackDirection.Z = 0.0f;
        PlayerCharacter->LaunchCharacter(KnockbackDirection.GetSafeNormal() * 350.0f + FVector(0.0f, 0.0f, 220.0f), true, true);
    }

    if (PlayerHealth <= 0.0f)
    {
        StartPlayerDefeat();
    }
    else if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 0.8f, FColor::Red, TEXT("An angry animal attacked you"));
    }

    UpdateHotbar();
}
