#pragma once

#include "CapturePartnerSaveGame.h"
#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Types/SlateEnums.h"
#include "CapturePlayerController.generated.h"

class UUserWidget;

/**
 * Keeps the prototype playable when a level migration has not carried over
 * the Third Person template's Enhanced Input mapping context.
 */
UCLASS()
class UEWORK1_API ACapturePlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    virtual void BeginPlay() override;
    virtual void SetupInputComponent() override;
    virtual void PlayerTick(float DeltaTime) override;
    virtual bool InputKey(const FInputKeyEventArgs& Params) override;

    UFUNCTION(BlueprintCallable)
    void StartAdventure();

    UFUNCTION(BlueprintCallable)
    void QuitToDesktop();
    void OpenMainMenu();
    bool IsMainMenuOpen() const { return bMainMenuOpen; }
    bool IsCreativeMode() const { return bCreativeMode; }
    const FString& GetActiveSaveSlot() const { return ActiveSaveSlot; }

private:
    UFUNCTION()
    void BeginNewAdventure();

    UFUNCTION()
    void ShowSaveSelection();

    UFUNCTION()
    void ShowNewAdventure();

    UFUNCTION()
    void LoadSelectedAdventure();

    UFUNCTION()
    void RenameSelectedAdventure();

    UFUNCTION()
    void DeleteSelectedAdventure();

    UFUNCTION()
    void ReturnToHome();

    UFUNCTION()
    void SetSurvivalMode();

    UFUNCTION()
    void SetCreativeMode();

    UFUNCTION()
    void HandleModeSelection(FString SelectedItem, ESelectInfo::Type SelectionType);

    bool ShouldUseInputFallback() const;
    void LoadSaveIndex();
    void SaveSaveIndex() const;
    void RefreshSavePicker();
    void SetMenuSectionVisible(const FName& SectionName, bool bVisible) const;
    FString GetSaveNameInput() const;
    void SetMenuStatus(const FString& Status) const;
    FCaptureSaveSlotInfo* FindSaveByDisplayName(const FString& DisplayName);
    void TurnWithMouse(float Value);
    void LookUpWithMouse(float Value);
    void StartJump();
    void StopJump();

    UPROPERTY()
    TObjectPtr<UUserWidget> MainMenuWidget;

    TArray<FCaptureSaveSlotInfo> SaveSlots;
    FString ActiveSaveSlot = TEXT("CapturePartnerStorage");
    bool bCreativeMode = false;
    bool bDeleteSaveArmed = false;
    float MenuAnimationTime = 0.0f;
    bool bMainMenuOpen = false;
};
