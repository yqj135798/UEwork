#pragma once

#include "CoreMinimal.h"
#include "AnimalCharacter.h"
#include "CapturePartnerSaveGame.h"
#include "GameFramework/Actor.h"
#include "CaptureLauncher.generated.h"

USTRUCT()
struct FPartnerRecord
{
    GENERATED_BODY()

    UPROPERTY()
    TSubclassOf<class AAnimalCharacter> AnimalClass;

    UPROPERTY()
    FString DisplayName;

    UPROPERTY()
    FString PartnerId;

    UPROPERTY()
    EPartnerBehaviorMode BehaviorMode = EPartnerBehaviorMode::Neutral;
};

UCLASS()
class UEWORK1_API ACaptureLauncher : public AActor
{
    GENERATED_BODY()

public:
    ACaptureLauncher();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    UFUNCTION(BlueprintCallable, Category = "Partner")
    void ApplyPartnerBehaviorFromCommandWheel(EPartnerBehaviorMode BehaviorMode);

    void ReceiveAnimalAttack(float Damage, const FVector& AttackOrigin);
    void RecallDefeatedPartner(class AAnimalCharacter* PartnerActor);
    void RegisterCapturedAnimal(class AAnimalCharacter* Animal);
    bool IsComputerOpen() const { return bComputerOpen; }
    bool IsPartnerCommandMenuOpen() const { return bPartnerCommandMenuOpen; }
    bool IsUsingUMGStorage() const;
    bool HandleStorageShortcut(const FKey& Key);
    void HandleComputerPointerInput(bool bPressed);
    void HandlePartnerCommandPointerInput(bool bPressed);
    void BeginStoragePointerDragAt(const FVector2D& ScreenPosition);
    void EndStoragePointerDragAt(const FVector2D& ScreenPosition);
    void SetBaseInteractionAvailable(class ACaptureBaseTerminal* BaseTerminal, bool bAvailable);
    void MovePartnerFromUMG(bool bSourceStorage, int32 SourceIndex, bool bTargetStorage, int32 TargetIndex);
    void MoveInventoryItem(bool bSourceBackpack, int32 SourceIndex, bool bTargetBackpack, int32 TargetIndex);
    bool HandleInventoryShortcut(const FKey& Key);
    void AddAnimalDrops(const class AAnimalCharacter* Animal);
    void LoadSelectedSaveProfile();

private:
    friend class UPartnerStorageWidget;
    friend class UPlayerInventoryWidget;
    void UsePrimaryAction();
    void UseSecondaryAction();
    void ReleasePrimaryAction();
    void BeginCaptureAim();
    void ReleaseCaptureAim();
    void SpawnCaptureBall();
    void AttackWithHand();
    void AttackWithSword(bool bUseSlash);
    void ResolveHandAttack();
    void FinishHandAttack();
    void DropSelectedItem();
    void ToggleComputer();
    void ToggleInventory();
    void OpenInventory();
    void CloseInventory();
    void InitializeInventory();
    void AddInventoryItem(EInventoryItemType ItemType, int32 Count);
    void ConsumeSelectedFood();
    void TickHunger(float DeltaSeconds);
    EInventoryItemType GetSelectedItemType() const;
    void SelectHotbarSlot(int32 NewSlot);
    void SelectSlotFour();
    void SelectSlotFive();
    void SelectSlotSix();
    void SelectSlotSeven();
    void SelectSlotEight();
    void SelectSlotNine();
    void InteractWithBase();
    void HandleYAction();
    void CancelReleaseConfirmation();
    void OpenBaseStorage();
    void CloseStorage();
    void ShowStorageInputWidget();
    void HideStorageInputWidget();
    void PopulatePartnerStorageWidget();
    void SelectPreviousPartner();
    void SelectNextPartner();
    void ChangeSelectedPartner(int32 Direction);
    void ChangeComputerList();
    void ReleaseComputerPartner();
    void ConfirmReleaseComputerPartner();
    void ClearReleaseConfirmation();
    void ToggleSelectedPartner();
    void BeginPartnerCommandHold();
    void EndPartnerCommandHold();
    void OpenPartnerCommandMenu();
    void ClosePartnerCommandMenu();
    void SelectPartnerCommandAtCursor();
    class AAnimalCharacter* FindCommandedPartner() const;
    void SetPartnerBehavior(class AAnimalCharacter* PartnerActor, EPartnerBehaviorMode BehaviorMode);
    void BeginComputerPointerDrag();
    void EndComputerPointerDrag();
    void MoveComputerPartner(bool bSourceStorage, int32 SourceIndex, bool bTargetStorage, int32 TargetIndex);
    TArray<FPartnerRecord>& GetActiveStoragePartners();
    const TArray<FPartnerRecord>& GetActiveStoragePartners() const;
    void MigrateLegacyBasePartners();
    void SpawnStoredBasePartners();
    void SyncActiveBasePartners();
    void SyncBasePartnerActors(class ACaptureBaseTerminal* BaseTerminal);
    void RecallPartner(const FString& PartnerId, bool bAnnounce);
    bool IsPartnerDeployed(const FString& PartnerId) const;
    void CommandPartnersToDefend(class AAnimalCharacter* Attacker);
    void CommandPartnersToAttackTarget(class AAnimalCharacter* Target);
    void RefreshPartnerHUD();
    void LoadPartnerData();
    void SavePartnerData() const;
    FString GetSaveSlotName() const;
    bool IsCreativeMode() const;
    FString MakePartnerDisplayName(const AAnimalCharacter* Animal) const;
    void SelectHand();
    void SelectCaptureBall();
    void SelectSword();
    void SelectPreviousHotbarItem();
    void SelectNextHotbarItem();
    void CycleHotbar(int32 Direction);
    void UpdateHotbar();
    void CreateHeldItemVisual();
    void UpdateHeldItemVisual();
    void ConfigurePlayerAnimations();
    void PlaySwordDrawFollowUp();
    void UpdateSwordLocomotion();
    void PlaySwordLocomotionAnimation(class UAnimSequence* Animation);
    void StopSwordLocomotion();
    class UAnimSequence* GetSwordIdleAnimation() const;
    void PlayPlayerAnimation(class UAnimSequence* Animation, bool bLooping);
    void RestorePlayerAnimation();
    void StartPlayerDefeat();
    void RespawnPlayer();

    UPROPERTY()
    TObjectPtr<class UStaticMeshComponent> HeldBallMesh;

    UPROPERTY()
    TObjectPtr<class UStaticMeshComponent> HeldSwordMesh;

    UPROPERTY()
    TObjectPtr<class UAnimSequence> HandAttackAnimation;

    UPROPERTY()
    TObjectPtr<class UAnimSequence> SwordDrawAnimation;

    UPROPERTY()
    TObjectPtr<class UAnimSequence> SwordDrawFollowUpAnimation;

    UPROPERTY()
    TObjectPtr<class UAnimSequence> SwordAttackAnimation;

    UPROPERTY()
    TObjectPtr<class UAnimSequence> SwordSlashAnimation;

    UPROPERTY()
    TObjectPtr<class UAnimSequence> SwordIdleAnimation01;

    UPROPERTY()
    TObjectPtr<class UAnimSequence> SwordIdleAnimation02;

    UPROPERTY()
    TObjectPtr<class UAnimSequence> SwordIdleAnimation03;

    UPROPERTY()
    TObjectPtr<class UAnimSequence> SwordIdleAnimation04;

    UPROPERTY()
    TObjectPtr<class UAnimSequence> SwordIdleAnimation05;

    UPROPERTY()
    TObjectPtr<class UAnimSequence> SwordWalkForwardAnimation;

    UPROPERTY()
    TObjectPtr<class UAnimSequence> SwordWalkBackwardAnimation;

    UPROPERTY()
    TObjectPtr<class UAnimSequence> SwordRunForwardAnimation;

    UPROPERTY()
    TObjectPtr<class UAnimSequence> SwordRunBackwardAnimation;

    UPROPERTY()
    TObjectPtr<class UAnimSequence> SwordStrafeRightAnimation;

    UPROPERTY()
    TObjectPtr<class UAnimSequence> SwordStrafeLeftAnimation;

    UPROPERTY()
    TObjectPtr<class UAnimSequence> SwordStrafeRightAltAnimation;

    UPROPERTY()
    TObjectPtr<class UAnimSequence> SwordStrafeLeftAltAnimation;

    UPROPERTY()
    TObjectPtr<class UAnimSequence> SwordTurnRightAnimation;

    UPROPERTY()
    TObjectPtr<class UAnimSequence> SwordTurnLeftAnimation;

    UPROPERTY()
    TObjectPtr<class UAnimSequence> SwordTurn180RightAnimation;

    UPROPERTY()
    TObjectPtr<class UAnimSequence> SwordTurn180LeftAnimation;

    UPROPERTY()
    TObjectPtr<class UAnimSequence> SwordJumpMovingAnimation;

    UPROPERTY()
    TObjectPtr<class UAnimSequence> SwordJumpIdleAnimation;

    UPROPERTY()
    TObjectPtr<class UAnimSequence> CaptureAimAnimation;

    UPROPERTY()
    TObjectPtr<class UAnimSequence> CaptureThrowAnimation;

    UPROPERTY()
    TObjectPtr<class UAnimSequence> PlayerDeathAnimation;

    UPROPERTY()
    TObjectPtr<class UAnimSequence> EatAnimation;

    int32 SelectedSlot;
    float PlayerHealth;
    float NextHandAttackTime;
    bool bCaptureAimActive;
    bool bCaptureThrowPending;
    bool bHandAttackActive;
    bool bEating;
    bool bPlayerDefeated;
    bool bComputerOpen;
    bool bInventoryOpen;
    bool bBaseStorageOpen;
    bool bBaseInteractionAvailable;
    bool bComputerSelectingStorage;
    bool bComputerDragActive;
    bool bComputerDragSourceStorage;
    bool bReleaseConfirmationPending;
    bool bReleaseConfirmationStorage;
    bool bPartnerCommandMenuOpen;
    bool bPartnerCommandHoldActive;
    FVector HandAttackDirection;
    float CurrentMeleeDamage;
    float CurrentMeleeRange;
    bool bSwordWasFalling;
    float SwordLocomotionLockUntil;
    float LastSwordControlYaw;
    TObjectPtr<class UAnimSequence> CurrentSwordLocomotionAnimation;
    int32 SelectedPartnerIndex;
    int32 ComputerSelectionIndex;
    int32 ComputerDragSourceIndex;
    int32 ReleaseConfirmationIndex;
    FString ReleaseConfirmationPartnerId;

    UPROPERTY()
    TArray<FPartnerRecord> PartyPartners;

    UPROPERTY()
    TArray<FPartnerRecord> ComputerPartners;

    UPROPERTY()
    TArray<FPartnerRecord> BasePartners;

    TMap<FString, TArray<FPartnerRecord>> BasePartnersByTerminal;
    TMap<FString, TMap<FString, TWeakObjectPtr<class AAnimalCharacter>>> DeployedBasePartners;
    FString ActiveBaseStorageId;

    TWeakObjectPtr<class ACaptureBaseTerminal> AvailableBaseTerminal;
    TWeakObjectPtr<class AAnimalCharacter> CommandedPartner;

    UPROPERTY()
    TObjectPtr<class UCaptureStorageInputWidget> StorageInputWidget;

    UPROPERTY()
    TObjectPtr<class UPartnerStorageWidget> PartnerStorageWidget;

    UPROPERTY()
    TObjectPtr<class UPlayerInventoryWidget> PlayerInventoryWidget;

    UPROPERTY()
    TArray<FInventoryItemStack> HotbarItems;

    UPROPERTY()
    TArray<FInventoryItemStack> BackpackItems;

    float Hunger;

    UPROPERTY()
    TObjectPtr<class UUserWidget> PartnerCommandWheelWidget;

    TMap<FString, TWeakObjectPtr<class AAnimalCharacter>> DeployedPartners;
    static const FString PartnerSaveSlot;
    FTimerHandle AnimationRestoreTimer;
    FTimerHandle SwordDrawFollowUpTimer;
    FTimerHandle ThrowTimer;
    FTimerHandle HandAttackHitTimer;
    FTimerHandle HandAttackFinishTimer;
    FTimerHandle ReleaseConfirmationTimer;
    FTimerHandle PartnerCommandHoldTimer;
    FTimerHandle PlayerRespawnTimer;
};
