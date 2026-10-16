#include "CapturePlayerController.h"

#include "CaptureLauncher.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableTextBox.h"
#include "Components/InputComponent.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "InputKeyEventArgs.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Math/RotationMatrix.h"
#include "Misc/Guid.h"

namespace
{
    const FString SaveIndexSlot(TEXT("RanchTales_SaveIndex"));
    const FString LegacySaveSlot(TEXT("CapturePartnerStorage"));
}

void ACapturePlayerController::BeginPlay()
{
    Super::BeginPlay();

    if (ShouldUseInputFallback())
    {
        UE_LOG(LogTemp, Log, TEXT("Capture prototype: wasteland keyboard and mouse fallback is active"));
    }

}

bool ACapturePlayerController::ShouldUseInputFallback() const
{
    const UWorld* World = GetWorld();
    return World && World->GetMapName().Contains(TEXT("Lvl_WastelandPrototype"));
}

void ACapturePlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    if (!InputComponent)
    {
        return;
    }

    FInputAxisKeyBinding& MouseXBinding = InputComponent->BindAxisKey(
        EKeys::MouseX, this, &ACapturePlayerController::TurnWithMouse);
    MouseXBinding.bConsumeInput = false;

    FInputAxisKeyBinding& MouseYBinding = InputComponent->BindAxisKey(
        EKeys::MouseY, this, &ACapturePlayerController::LookUpWithMouse);
    MouseYBinding.bConsumeInput = false;

    FInputKeyBinding& JumpPressedBinding = InputComponent->BindKey(
        EKeys::SpaceBar, IE_Pressed, this, &ACapturePlayerController::StartJump);
    JumpPressedBinding.bConsumeInput = false;

    FInputKeyBinding& JumpReleasedBinding = InputComponent->BindKey(
        EKeys::SpaceBar, IE_Released, this, &ACapturePlayerController::StopJump);
    JumpReleasedBinding.bConsumeInput = false;

    UE_LOG(LogTemp, Log, TEXT("Capture prototype: direct mouse axis bindings are registered"));
}

bool ACapturePlayerController::InputKey(const FInputKeyEventArgs& Params)
{
    if (bMainMenuOpen)
    {
        if (Params.Event == IE_Pressed && Params.Key == EKeys::Enter)
        {
            BeginNewAdventure();
            return true;
        }
        if (Params.Event == IE_Pressed && Params.Key == EKeys::Escape)
        {
            QuitToDesktop();
            return true;
        }
        return Super::InputKey(Params);
    }

    if (Params.Key == EKeys::LeftMouseButton
        && (Params.Event == IE_Pressed || Params.Event == IE_Released))
    {
        if (ACaptureLauncher* Launcher = Cast<ACaptureLauncher>(
            UGameplayStatics::GetActorOfClass(this, ACaptureLauncher::StaticClass())))
        {
            if (Launcher->IsPartnerCommandMenuOpen())
            {
                Launcher->HandlePartnerCommandPointerInput(Params.Event == IE_Pressed);
                return true;
            }
            if (Launcher->IsComputerOpen() && !Launcher->IsUsingUMGStorage())
            {
                Launcher->HandleComputerPointerInput(Params.Event == IE_Pressed);
                return true;
            }
        }
    }

    return Super::InputKey(Params);
}

void ACapturePlayerController::TurnWithMouse(float Value)
{
    if (ShouldUseInputFallback())
    {
        AddYawInput(Value);
    }
}

void ACapturePlayerController::LookUpWithMouse(float Value)
{
    if (ShouldUseInputFallback())
    {
        AddPitchInput(-Value);
    }
}

void ACapturePlayerController::StartJump()
{
    if (ShouldUseInputFallback())
    {
        if (ACharacter* PlayerCharacter = Cast<ACharacter>(GetPawn()))
        {
            PlayerCharacter->Jump();
        }
    }
}

void ACapturePlayerController::StopJump()
{
    if (ShouldUseInputFallback())
    {
        if (ACharacter* PlayerCharacter = Cast<ACharacter>(GetPawn()))
        {
            PlayerCharacter->StopJumping();
        }
    }
}

void ACapturePlayerController::PlayerTick(float DeltaTime)
{
    Super::PlayerTick(DeltaTime);

    if (bMainMenuOpen)
    {
        MenuAnimationTime += DeltaTime;
        if (MainMenuWidget)
        {
            if (UWidget* Panel = MainMenuWidget->GetWidgetFromName(TEXT("MenuPanel")))
            {
                Panel->SetRenderTranslation(FVector2D(0.0f, FMath::Sin(MenuAnimationTime * 0.75f) * 6.0f));
            }
        }
        return;
    }

    if (!ShouldUseInputFallback())
    {
        return;
    }

    APawn* ControlledPawn = GetPawn();
    if (!ControlledPawn || IsMoveInputIgnored())
    {
        return;
    }

    if (ACharacter* PlayerCharacter = Cast<ACharacter>(ControlledPawn))
    {
        const bool bSprintHeld = IsInputKeyDown(EKeys::LeftShift) || IsInputKeyDown(EKeys::RightShift);
        PlayerCharacter->GetCharacterMovement()->MaxWalkSpeed = bSprintHeld ? 720.0f : 480.0f;
    }

    // Polling physical keys is intentional: it remains available even if the
    // level's Enhanced Input mapping context is absent.
    const float ForwardValue = (IsInputKeyDown(EKeys::W) ? 1.0f : 0.0f)
        - (IsInputKeyDown(EKeys::S) ? 1.0f : 0.0f);
    const float RightValue = (IsInputKeyDown(EKeys::D) ? 1.0f : 0.0f)
        - (IsInputKeyDown(EKeys::A) ? 1.0f : 0.0f);

    if (FMath::IsNearlyZero(ForwardValue) && FMath::IsNearlyZero(RightValue))
    {
        return;
    }

    const FRotator YawRotation(0.0f, GetControlRotation().Yaw, 0.0f);
    if (!FMath::IsNearlyZero(ForwardValue))
    {
        ControlledPawn->AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X), ForwardValue);
    }
    if (!FMath::IsNearlyZero(RightValue))
    {
        ControlledPawn->AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y), RightValue);
    }
}

void ACapturePlayerController::OpenMainMenu()
{
    if (bMainMenuOpen)
    {
        return;
    }

    static const TCHAR* MainMenuClassPath = TEXT("/Game/UI/WBP_MainMenu.WBP_MainMenu_C");
    TSubclassOf<UUserWidget> MainMenuClass = LoadClass<UUserWidget>(nullptr, MainMenuClassPath);
    if (!MainMenuClass)
    {
        UE_LOG(LogTemp, Error, TEXT("Capture prototype: failed to load main menu widget blueprint"));
        return;
    }

    MainMenuWidget = CreateWidget<UUserWidget>(this, MainMenuClass);
    if (!MainMenuWidget)
    {
        UE_LOG(LogTemp, Error, TEXT("Capture prototype: failed to create main menu widget"));
        return;
    }

    // The home screen leads with Continue. Creating a profile is a separate
    // step so the name and play mode are chosen deliberately.
    if (UButton* StartButton = Cast<UButton>(MainMenuWidget->GetWidgetFromName(TEXT("StartButton"))))
    {
        StartButton->OnClicked.AddDynamic(this, &ACapturePlayerController::ShowSaveSelection);
    }
    if (UButton* ContinueButton = Cast<UButton>(MainMenuWidget->GetWidgetFromName(TEXT("ContinueButton"))))
    {
        ContinueButton->OnClicked.AddDynamic(this, &ACapturePlayerController::ShowNewAdventure);
    }
    if (UButton* QuitButton = Cast<UButton>(MainMenuWidget->GetWidgetFromName(TEXT("QuitButton"))))
    {
        QuitButton->OnClicked.AddDynamic(this, &ACapturePlayerController::QuitToDesktop);
    }
    if (UButton* Button = Cast<UButton>(MainMenuWidget->GetWidgetFromName(TEXT("LoadSaveButton"))))
    {
        Button->OnClicked.AddDynamic(this, &ACapturePlayerController::LoadSelectedAdventure);
    }
    if (UButton* Button = Cast<UButton>(MainMenuWidget->GetWidgetFromName(TEXT("RenameSaveButton"))))
    {
        Button->OnClicked.AddDynamic(this, &ACapturePlayerController::RenameSelectedAdventure);
    }
    if (UButton* Button = Cast<UButton>(MainMenuWidget->GetWidgetFromName(TEXT("DeleteSaveButton"))))
    {
        Button->OnClicked.AddDynamic(this, &ACapturePlayerController::DeleteSelectedAdventure);
    }
    if (UButton* Button = Cast<UButton>(MainMenuWidget->GetWidgetFromName(TEXT("BackButton"))))
    {
        Button->OnClicked.AddDynamic(this, &ACapturePlayerController::ReturnToHome);
    }
    if (UButton* Button = Cast<UButton>(MainMenuWidget->GetWidgetFromName(TEXT("NewAdventureButton"))))
    {
        Button->OnClicked.AddDynamic(this, &ACapturePlayerController::BeginNewAdventure);
    }
    if (UButton* Button = Cast<UButton>(MainMenuWidget->GetWidgetFromName(TEXT("NewAdventureBackButton"))))
    {
        Button->OnClicked.AddDynamic(this, &ACapturePlayerController::ReturnToHome);
    }
    if (UButton* Button = Cast<UButton>(MainMenuWidget->GetWidgetFromName(TEXT("SurvivalButton"))))
    {
        Button->OnClicked.AddDynamic(this, &ACapturePlayerController::SetSurvivalMode);
    }
    if (UButton* Button = Cast<UButton>(MainMenuWidget->GetWidgetFromName(TEXT("CreativeButton"))))
    {
        Button->OnClicked.AddDynamic(this, &ACapturePlayerController::SetCreativeMode);
    }
    if (UComboBoxString* ModePicker = Cast<UComboBoxString>(MainMenuWidget->GetWidgetFromName(TEXT("ModePicker"))))
    {
        ModePicker->ClearOptions();
        ModePicker->AddOption(TEXT("生存模式"));
        ModePicker->AddOption(TEXT("创造模式"));
        ModePicker->OnSelectionChanged.AddDynamic(this, &ACapturePlayerController::HandleModeSelection);
        ModePicker->SetSelectedIndex(0);
    }

    LoadSaveIndex();
    RefreshSavePicker();
    SetSurvivalMode();
    SetMenuSectionVisible(TEXT("NewAdventurePanel"), false);
    MainMenuWidget->SetIsFocusable(true);
    MainMenuWidget->AddToViewport(100);
    bMainMenuOpen = true;
    SetIgnoreMoveInput(true);
    SetIgnoreLookInput(true);
    bShowMouseCursor = true;
    bEnableClickEvents = true;
    bEnableMouseOverEvents = true;
    FInputModeUIOnly InputMode;
    InputMode.SetWidgetToFocus(MainMenuWidget->TakeWidget());
    InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    SetInputMode(InputMode);
    SetPause(true);
    UE_LOG(LogTemp, Log, TEXT("Capture prototype: main menu opened"));
}

void ACapturePlayerController::StartAdventure()
{
    if (!bMainMenuOpen)
    {
        return;
    }

    // The launcher exists before the menu does. Re-read only after the user
    // picks a profile, so each adventure owns an independent game state.
    if (ACaptureLauncher* Launcher = Cast<ACaptureLauncher>(
        UGameplayStatics::GetActorOfClass(this, ACaptureLauncher::StaticClass())))
    {
        Launcher->LoadSelectedSaveProfile();
    }

    SetPause(false);
    if (MainMenuWidget)
    {
        MainMenuWidget->RemoveFromParent();
        MainMenuWidget = nullptr;
    }
    bMainMenuOpen = false;
    SetIgnoreMoveInput(false);
    SetIgnoreLookInput(false);
    bShowMouseCursor = false;
    bEnableClickEvents = false;
    bEnableMouseOverEvents = false;
    SetInputMode(FInputModeGameOnly());
    if (PlayerCameraManager)
    {
        PlayerCameraManager->StartCameraFade(1.0f, 0.0f, 0.35f, FLinearColor::Black, false, true);
    }
}

void ACapturePlayerController::LoadSaveIndex()
{
    SaveSlots.Empty();
    if (UCapturePartnerSaveGame* Index = Cast<UCapturePartnerSaveGame>(
        UGameplayStatics::LoadGameFromSlot(SaveIndexSlot, 0)))
    {
        SaveSlots = Index->SaveSlots;
    }

    // Preserve the project's original one-slot progress as a named profile.
    const bool bLegacyAlreadyListed = SaveSlots.ContainsByPredicate([](const FCaptureSaveSlotInfo& Entry)
    {
        return Entry.SlotName == LegacySaveSlot;
    });
    if (!bLegacyAlreadyListed && UGameplayStatics::DoesSaveGameExist(LegacySaveSlot, 0))
    {
        FCaptureSaveSlotInfo LegacySave;
        LegacySave.SlotName = LegacySaveSlot;
        LegacySave.DisplayName = TEXT("旧旅程");
        LegacySave.bCreativeMode = false;
        SaveSlots.Add(LegacySave);
        SaveSaveIndex();
    }
}

void ACapturePlayerController::SaveSaveIndex() const
{
    UCapturePartnerSaveGame* Index = Cast<UCapturePartnerSaveGame>(
        UGameplayStatics::CreateSaveGameObject(UCapturePartnerSaveGame::StaticClass()));
    if (Index)
    {
        Index->SaveSlots = SaveSlots;
        UGameplayStatics::SaveGameToSlot(Index, SaveIndexSlot, 0);
    }
}

void ACapturePlayerController::RefreshSavePicker()
{
    if (!MainMenuWidget)
    {
        return;
    }

    if (UComboBoxString* Picker = Cast<UComboBoxString>(MainMenuWidget->GetWidgetFromName(TEXT("SaveSlotPicker"))))
    {
        Picker->ClearOptions();
        for (const FCaptureSaveSlotInfo& Entry : SaveSlots)
        {
            Picker->AddOption(Entry.DisplayName);
        }
        if (!SaveSlots.IsEmpty())
        {
            Picker->SetSelectedIndex(0);
        }
    }
}

FString ACapturePlayerController::GetSaveNameInput() const
{
    if (MainMenuWidget)
    {
        const UWidget* NewAdventurePanel = MainMenuWidget->GetWidgetFromName(TEXT("NewAdventurePanel"));
        if (NewAdventurePanel && NewAdventurePanel->GetVisibility() == ESlateVisibility::Visible)
        {
            if (const UEditableTextBox* NewAdventureInput = Cast<UEditableTextBox>(
                MainMenuWidget->GetWidgetFromName(TEXT("AdventureNameInput"))))
            {
                return NewAdventureInput->GetText().ToString().TrimStartAndEnd();
            }
        }
        if (const UEditableTextBox* Input = Cast<UEditableTextBox>(MainMenuWidget->GetWidgetFromName(TEXT("SaveNameInput"))))
        {
            return Input->GetText().ToString().TrimStartAndEnd();
        }
    }
    return FString();
}

void ACapturePlayerController::SetMenuStatus(const FString& Status) const
{
    if (MainMenuWidget)
    {
        if (UTextBlock* StatusText = Cast<UTextBlock>(MainMenuWidget->GetWidgetFromName(TEXT("MenuStatusText"))))
        {
            StatusText->SetText(FText::FromString(Status));
        }
        if (UTextBlock* StatusText = Cast<UTextBlock>(MainMenuWidget->GetWidgetFromName(TEXT("NewAdventureStatusText"))))
        {
            StatusText->SetText(FText::FromString(Status));
        }
    }
}

void ACapturePlayerController::SetMenuSectionVisible(const FName& SectionName, bool bVisible) const
{
    if (!MainMenuWidget)
    {
        return;
    }

    if (UWidget* Section = MainMenuWidget->GetWidgetFromName(SectionName))
    {
        Section->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }
}

FCaptureSaveSlotInfo* ACapturePlayerController::FindSaveByDisplayName(const FString& DisplayName)
{
    return SaveSlots.FindByPredicate([&DisplayName](const FCaptureSaveSlotInfo& Entry)
    {
        return Entry.DisplayName == DisplayName;
    });
}

void ACapturePlayerController::BeginNewAdventure()
{
    FString DisplayName = GetSaveNameInput();
    if (DisplayName.IsEmpty())
    {
        DisplayName = TEXT("新的牧场");
    }
    if (FindSaveByDisplayName(DisplayName))
    {
        SetMenuStatus(TEXT("这个存档名称已经存在，请换一个名称。"));
        return;
    }

    FCaptureSaveSlotInfo NewSave;
    NewSave.SlotName = FString::Printf(TEXT("RanchTales_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
    NewSave.DisplayName = DisplayName;
    NewSave.bCreativeMode = bCreativeMode;

    UCapturePartnerSaveGame* SaveGame = Cast<UCapturePartnerSaveGame>(
        UGameplayStatics::CreateSaveGameObject(UCapturePartnerSaveGame::StaticClass()));
    if (!SaveGame)
    {
        SetMenuStatus(TEXT("无法创建新存档。"));
        return;
    }
    SaveGame->SaveDisplayName = NewSave.DisplayName;
    SaveGame->bCreativeMode = NewSave.bCreativeMode;
    if (!UGameplayStatics::SaveGameToSlot(SaveGame, NewSave.SlotName, 0))
    {
        SetMenuStatus(TEXT("写入新存档失败。"));
        return;
    }

    SaveSlots.Add(NewSave);
    ActiveSaveSlot = NewSave.SlotName;
    SaveSaveIndex();
    StartAdventure();
}

void ACapturePlayerController::ShowSaveSelection()
{
    bDeleteSaveArmed = false;
    LoadSaveIndex();
    RefreshSavePicker();
    SetMenuSectionVisible(TEXT("HomePanel"), false);
    SetMenuSectionVisible(TEXT("NewAdventurePanel"), false);
    SetMenuSectionVisible(TEXT("SaveSelectionPanel"), true);
    SetMenuStatus(SaveSlots.IsEmpty()
        ? TEXT("还没有存档。请输入名称后开始新的冒险。")
        : TEXT("选择存档后可进入、改名或删除。删除需要再次确认。"));
}

void ACapturePlayerController::ShowNewAdventure()
{
    bDeleteSaveArmed = false;
    SetMenuSectionVisible(TEXT("HomePanel"), false);
    SetMenuSectionVisible(TEXT("SaveSelectionPanel"), false);
    SetMenuSectionVisible(TEXT("NewAdventurePanel"), true);
    SetSurvivalMode();
    SetMenuStatus(TEXT("输入名称并选择模式后，开始新的冒险。"));
}

void ACapturePlayerController::LoadSelectedAdventure()
{
    const UComboBoxString* Picker = MainMenuWidget
        ? Cast<UComboBoxString>(MainMenuWidget->GetWidgetFromName(TEXT("SaveSlotPicker")))
        : nullptr;
    FCaptureSaveSlotInfo* SelectedSave = Picker ? FindSaveByDisplayName(Picker->GetSelectedOption()) : nullptr;
    if (!SelectedSave)
    {
        SetMenuStatus(TEXT("请先选择一个存档。"));
        return;
    }

    ActiveSaveSlot = SelectedSave->SlotName;
    bCreativeMode = SelectedSave->bCreativeMode;
    StartAdventure();
}

void ACapturePlayerController::RenameSelectedAdventure()
{
    const UComboBoxString* Picker = MainMenuWidget
        ? Cast<UComboBoxString>(MainMenuWidget->GetWidgetFromName(TEXT("SaveSlotPicker")))
        : nullptr;
    FCaptureSaveSlotInfo* SelectedSave = Picker ? FindSaveByDisplayName(Picker->GetSelectedOption()) : nullptr;
    const FString NewName = GetSaveNameInput();
    if (!SelectedSave || NewName.IsEmpty())
    {
        SetMenuStatus(TEXT("选择存档，并在名称栏输入新名称。"));
        return;
    }
    if (FCaptureSaveSlotInfo* Duplicate = FindSaveByDisplayName(NewName); Duplicate && Duplicate != SelectedSave)
    {
        SetMenuStatus(TEXT("已有同名存档。"));
        return;
    }

    SelectedSave->DisplayName = NewName;
    if (UCapturePartnerSaveGame* SaveGame = Cast<UCapturePartnerSaveGame>(UGameplayStatics::LoadGameFromSlot(SelectedSave->SlotName, 0)))
    {
        SaveGame->SaveDisplayName = NewName;
        UGameplayStatics::SaveGameToSlot(SaveGame, SelectedSave->SlotName, 0);
    }
    SaveSaveIndex();
    RefreshSavePicker();
    SetMenuStatus(TEXT("存档名称已更新。"));
}

void ACapturePlayerController::DeleteSelectedAdventure()
{
    const UComboBoxString* Picker = MainMenuWidget
        ? Cast<UComboBoxString>(MainMenuWidget->GetWidgetFromName(TEXT("SaveSlotPicker")))
        : nullptr;
    FCaptureSaveSlotInfo* SelectedSave = Picker ? FindSaveByDisplayName(Picker->GetSelectedOption()) : nullptr;
    if (!SelectedSave)
    {
        SetMenuStatus(TEXT("请先选择一个存档。"));
        return;
    }
    if (!bDeleteSaveArmed)
    {
        bDeleteSaveArmed = true;
        SetMenuStatus(FString::Printf(TEXT("再次点击“删除存档”将永久删除：%s"), *SelectedSave->DisplayName));
        return;
    }

    const FString SlotToDelete = SelectedSave->SlotName;
    SaveSlots.RemoveAll([&SlotToDelete](const FCaptureSaveSlotInfo& Entry) { return Entry.SlotName == SlotToDelete; });
    UGameplayStatics::DeleteGameInSlot(SlotToDelete, 0);
    SaveSaveIndex();
    bDeleteSaveArmed = false;
    RefreshSavePicker();
    SetMenuStatus(TEXT("存档已删除。"));
}

void ACapturePlayerController::ReturnToHome()
{
    bDeleteSaveArmed = false;
    SetMenuSectionVisible(TEXT("SaveSelectionPanel"), false);
    SetMenuSectionVisible(TEXT("NewAdventurePanel"), false);
    SetMenuSectionVisible(TEXT("HomePanel"), true);
    SetMenuStatus(TEXT("选择主地图与模式后，开始新的冒险。"));
}

void ACapturePlayerController::SetSurvivalMode()
{
    bCreativeMode = false;
    SetMenuStatus(TEXT("生存模式：生命、饥饿与捕捉球消耗都会生效。"));
}

void ACapturePlayerController::SetCreativeMode()
{
    bCreativeMode = true;
    SetMenuStatus(TEXT("创造模式：当前版本关闭生命、饥饿与捕捉球消耗。"));
}

void ACapturePlayerController::HandleModeSelection(FString SelectedItem, ESelectInfo::Type SelectionType)
{
    if (SelectedItem == TEXT("创造模式"))
    {
        SetCreativeMode();
    }
    else
    {
        SetSurvivalMode();
    }
}

void ACapturePlayerController::QuitToDesktop()
{
    UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
}
