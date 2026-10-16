#include "CaptureHUD.h"

#include "AnimalCharacter.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GlobalRenderResources.h"

void ACaptureHUD::SetInventoryState(int32 InSelectedSlot, float InPlayerHealth, float InHunger, const TArray<FInventoryItemStack>& InHotbarItems)
{
    SelectedSlot = InSelectedSlot;
    PlayerHealth = InPlayerHealth;
    Hunger = InHunger;
    HotbarItems = InHotbarItems;
}

void ACaptureHUD::SetCaptureAimState(bool bInCaptureAimActive)
{
    bCaptureAimActive = bInCaptureAimActive;
}

void ACaptureHUD::SetPartnerState(
    const TArray<FString>& InPartyMembers,
    const TArray<FString>& InComputerMembers,
    int32 InSelectedPartnerIndex,
    bool bInComputerOpen,
    bool bInComputerSelectingStorage,
    int32 InComputerSelectionIndex,
    const TArray<bool>& InPartyDeployed
)
{
    PartyMembers = InPartyMembers;
    ComputerMembers = InComputerMembers;
    SelectedPartnerIndex = InSelectedPartnerIndex;
    bComputerOpen = bInComputerOpen;
    bComputerSelectingStorage = bInComputerSelectingStorage;
    ComputerSelectionIndex = InComputerSelectionIndex;
    PartyDeployed = InPartyDeployed;
}

void ACaptureHUD::SetComputerDragState(bool bInDragActive, const FString& InDragName)
{
    bComputerDragActive = bInDragActive;
    ComputerDragName = InDragName;
}

void ACaptureHUD::SetReleaseConfirmation(bool bInPending, const FString& InPartnerName)
{
    bReleaseConfirmationPending = bInPending;
    ReleaseConfirmationPartnerName = InPartnerName;
}

void ACaptureHUD::SetStorageLabels(const FString& InTitle, const FString& InStorageLabel)
{
    StorageTitle = InTitle;
    StorageLabel = InStorageLabel;
}

void ACaptureHUD::SetBaseInteractionPrompt(bool bInVisible)
{
    bBaseInteractionPromptVisible = bInVisible;
}

void ACaptureHUD::SetPartnerCommandMenuState(bool bInVisible, const FString& InPartnerName)
{
    bPartnerCommandMenuVisible = bInVisible;
    PartnerCommandPartnerName = InPartnerName;
}

bool ACaptureHUD::GetComputerSlotAtScreenPosition(const FVector2D& ScreenPosition, bool& bOutStorage, int32& OutIndex) const
{
    bOutStorage = false;
    OutIndex = -1;
    if (!bComputerOpen || !Canvas)
    {
        return false;
    }

    // Pointer events report raw viewport pixels, while Canvas coordinates are
    // DPI-scaled inside an editor PIE viewport. Normalize before testing slots.
    FVector2D CanvasPosition = ScreenPosition;
    if (APlayerController* PlayerController = GetOwningPlayerController())
    {
        int32 ViewportWidth = 0;
        int32 ViewportHeight = 0;
        PlayerController->GetViewportSize(ViewportWidth, ViewportHeight);
        if (ViewportWidth > 0 && ViewportHeight > 0)
        {
            CanvasPosition.X *= Canvas->SizeX / static_cast<float>(ViewportWidth);
            CanvasPosition.Y *= Canvas->SizeY / static_cast<float>(ViewportHeight);
        }
    }

    const float PanelWidth = FMath::Min(1050.0f, Canvas->SizeX - 70.0f);
    const float PanelHeight = FMath::Min(650.0f, Canvas->SizeY - 70.0f);
    const float PanelX = (Canvas->SizeX - PanelWidth) * 0.5f;
    const float PanelY = (Canvas->SizeY - PanelHeight) * 0.5f;
    const float ContentY = PanelY + 72.0f;
    const float PartyColumnWidth = PanelWidth * 0.31f;
    const float PartyX = PanelX + 24.0f;
    const float PartyWidth = PartyColumnWidth - 42.0f;
    const float PartyRowHeight = 52.0f;

    for (int32 Index = 0; Index < 6; ++Index)
    {
        const float RowY = ContentY + Index * (PartyRowHeight + 7.0f);
        if (CanvasPosition.X >= PartyX && CanvasPosition.X <= PartyX + PartyWidth
            && CanvasPosition.Y >= RowY && CanvasPosition.Y <= RowY + PartyRowHeight)
        {
            bOutStorage = false;
            OutIndex = Index;
            return true;
        }
    }

    constexpr int32 VisibleSlots = 15;
    constexpr int32 Columns = 3;
    const int32 StoragePage = ComputerSelectionIndex >= 0 && bComputerSelectingStorage
        ? ComputerSelectionIndex / VisibleSlots
        : 0;
    const float StorageX = PanelX + PartyColumnWidth + 42.0f;
    const float StorageWidth = PanelWidth - PartyColumnWidth - 66.0f;
    const float CellGap = 9.0f;
    const float CellWidth = (StorageWidth - CellGap * (Columns - 1)) / Columns;
    const float CellHeight = 75.0f;
    for (int32 Index = 0; Index < VisibleSlots; ++Index)
    {
        const int32 RowIndex = Index / Columns;
        const int32 ColumnIndex = Index % Columns;
        const float CellX = StorageX + ColumnIndex * (CellWidth + CellGap);
        const float CellY = ContentY + RowIndex * (CellHeight + CellGap);
        if (CanvasPosition.X >= CellX && CanvasPosition.X <= CellX + CellWidth
            && CanvasPosition.Y >= CellY && CanvasPosition.Y <= CellY + CellHeight)
        {
            bOutStorage = true;
            OutIndex = StoragePage * VisibleSlots + Index;
            return true;
        }
    }

    return false;
}

bool ACaptureHUD::GetPartnerCommandAtScreenPosition(const FVector2D& ScreenPosition, EPartnerBehaviorMode& OutBehavior) const
{
    if (!bPartnerCommandMenuVisible || !Canvas)
    {
        return false;
    }

    FVector2D CanvasPosition = ScreenPosition;
    if (APlayerController* PlayerController = GetOwningPlayerController())
    {
        int32 ViewportWidth = 0;
        int32 ViewportHeight = 0;
        PlayerController->GetViewportSize(ViewportWidth, ViewportHeight);
        if (ViewportWidth > 0 && ViewportHeight > 0)
        {
            CanvasPosition.X *= Canvas->SizeX / static_cast<float>(ViewportWidth);
            CanvasPosition.Y *= Canvas->SizeY / static_cast<float>(ViewportHeight);
        }
    }

    const FVector2D Offset = CanvasPosition - FVector2D(Canvas->SizeX * 0.5f, Canvas->SizeY * 0.5f);
    const float Radius = Offset.Size();
    if (Radius < 78.0f || Radius > 202.0f)
    {
        return false;
    }

    const float Angle = FMath::RadiansToDegrees(FMath::Atan2(Offset.Y, Offset.X));
    if (FMath::Abs(FMath::FindDeltaAngleDegrees(-90.0f, Angle)) <= 52.0f)
    {
        OutBehavior = EPartnerBehaviorMode::Neutral;
        return true;
    }
    if (FMath::Abs(FMath::FindDeltaAngleDegrees(150.0f, Angle)) <= 52.0f)
    {
        OutBehavior = EPartnerBehaviorMode::Passive;
        return true;
    }
    if (FMath::Abs(FMath::FindDeltaAngleDegrees(30.0f, Angle)) <= 52.0f)
    {
        OutBehavior = EPartnerBehaviorMode::AttackMyTarget;
        return true;
    }
    return false;
}

void ACaptureHUD::DrawHUD()
{
    Super::DrawHUD();
    if (!Canvas || !GEngine)
    {
        return;
    }

    if (bPartnerCommandMenuVisible)
    {
        FCanvasTileItem Dimmer(
            FVector2D::ZeroVector,
            FVector2D(Canvas->SizeX, Canvas->SizeY),
            FLinearColor(0.0f, 0.0f, 0.0f, 0.70f));
        Dimmer.BlendMode = SE_BLEND_Translucent;
        Canvas->DrawItem(Dimmer);

        const FVector2D Center(Canvas->SizeX * 0.5f, Canvas->SizeY * 0.5f);
        float MouseX = 0.0f;
        float MouseY = 0.0f;
        EPartnerBehaviorMode HoveredBehavior = EPartnerBehaviorMode::Neutral;
        const bool bHasHoveredBehavior = GetOwningPlayerController()
            && GetOwningPlayerController()->GetMousePosition(MouseX, MouseY)
            && GetPartnerCommandAtScreenPosition(FVector2D(MouseX, MouseY), HoveredBehavior);

        auto DrawCommandSector = [this, &Center, bHasHoveredBehavior, HoveredBehavior](float CenterAngle, EPartnerBehaviorMode Behavior, const FString& Title, const FString& Detail, const FLinearColor& BaseColor)
        {
            const bool bHovered = bHasHoveredBehavior && HoveredBehavior == Behavior;
            const FLinearColor Color = bHovered
                ? FLinearColor::LerpUsingHSV(BaseColor, FLinearColor::White, 0.38f)
                : BaseColor;
            constexpr float InnerRadius = 78.0f;
            constexpr float OuterRadius = 202.0f;
            constexpr float HalfAngle = 52.0f;
            constexpr int32 SegmentCount = 14;
            for (int32 SegmentIndex = 0; SegmentIndex < SegmentCount; ++SegmentIndex)
            {
                const float StartAngle = CenterAngle - HalfAngle + (HalfAngle * 2.0f * SegmentIndex / SegmentCount);
                const float EndAngle = CenterAngle - HalfAngle + (HalfAngle * 2.0f * (SegmentIndex + 1) / SegmentCount);
                auto PointOnRing = [&Center](float Radius, float Angle)
                {
                    const float Radians = FMath::DegreesToRadians(Angle);
                    return Center + FVector2D(FMath::Cos(Radians), FMath::Sin(Radians)) * Radius;
                };
                const FVector2D InnerStart = PointOnRing(InnerRadius, StartAngle);
                const FVector2D OuterStart = PointOnRing(OuterRadius, StartAngle);
                const FVector2D InnerEnd = PointOnRing(InnerRadius, EndAngle);
                const FVector2D OuterEnd = PointOnRing(OuterRadius, EndAngle);
                FCanvasTriangleItem FirstTriangle(InnerStart, OuterStart, OuterEnd, GWhiteTexture);
                FirstTriangle.SetColor(Color);
                FirstTriangle.BlendMode = SE_BLEND_Translucent;
                Canvas->DrawItem(FirstTriangle);
                FCanvasTriangleItem SecondTriangle(InnerStart, OuterEnd, InnerEnd, GWhiteTexture);
                SecondTriangle.SetColor(Color);
                SecondTriangle.BlendMode = SE_BLEND_Translucent;
                Canvas->DrawItem(SecondTriangle);
            }

            const float LabelRadians = FMath::DegreesToRadians(CenterAngle);
            const FVector2D LabelPosition = Center + FVector2D(FMath::Cos(LabelRadians), FMath::Sin(LabelRadians)) * 136.0f;
            FCanvasTextItem TitleItem(LabelPosition + FVector2D(0.0f, -12.0f), FText::FromString(Title), GEngine->GetSmallFont(), FLinearColor::White);
            TitleItem.bCentreX = true;
            Canvas->DrawItem(TitleItem);
            FCanvasTextItem DetailItem(LabelPosition + FVector2D(0.0f, 10.0f), FText::FromString(Detail), GEngine->GetSmallFont(), FLinearColor(0.86f, 0.90f, 0.92f, 1.0f));
            DetailItem.bCentreX = true;
            Canvas->DrawItem(DetailItem);
        };

        DrawCommandSector(-90.0f, EPartnerBehaviorMode::Neutral, TEXT("NEUTRAL"), TEXT("DEFEND OWNER"), FLinearColor(0.10f, 0.34f, 0.42f, 0.94f));
        DrawCommandSector(150.0f, EPartnerBehaviorMode::Passive, TEXT("PASSIVE"), TEXT("FOLLOW ONLY"), FLinearColor(0.22f, 0.25f, 0.30f, 0.94f));
        DrawCommandSector(30.0f, EPartnerBehaviorMode::AttackMyTarget, TEXT("ATTACK TARGET"), TEXT("ATTACK MY HIT"), FLinearColor(0.48f, 0.20f, 0.12f, 0.94f));

        FCanvasTileItem CenterPanel(Center - FVector2D(72.0f, 34.0f), FVector2D(144.0f, 68.0f), FLinearColor(0.025f, 0.04f, 0.055f, 0.98f));
        CenterPanel.BlendMode = SE_BLEND_Translucent;
        Canvas->DrawItem(CenterPanel);
        FCanvasTextItem Heading(Center + FVector2D(0.0f, -19.0f), FText::FromString(PartnerCommandPartnerName), GEngine->GetSmallFont(), FLinearColor(0.98f, 0.78f, 0.24f, 1.0f));
        Heading.bCentreX = true;
        Canvas->DrawItem(Heading);
        FCanvasTextItem CancelHint(Center + FVector2D(0.0f, 7.0f), FText::FromString(TEXT("CLICK SELECT")), GEngine->GetSmallFont(), FLinearColor::White);
        CancelHint.bCentreX = true;
        Canvas->DrawItem(CancelHint);
        FCanvasTextItem ExitHint(Center + FVector2D(0.0f, 24.0f), FText::FromString(TEXT("[E] CANCEL")), GEngine->GetSmallFont(), FLinearColor(0.72f, 0.78f, 0.82f, 1.0f));
        ExitHint.bCentreX = true;
        Canvas->DrawItem(ExitHint);
        return;
    }

    const float SlotWidth = FMath::Min(108.0f, (Canvas->SizeX - 88.0f) / 9.0f);
    const float SlotHeight = 58.0f;
    const float Gap = 8.0f;
    const float TotalWidth = SlotWidth * 9.0f + Gap * 8.0f;
    const float StartX = (Canvas->SizeX - TotalWidth) * 0.5f;
    const float SlotY = Canvas->SizeY - 90.0f;
    const FLinearColor SelectedColor(0.95f, 0.72f, 0.16f, 0.95f);
    const FLinearColor IdleColor(0.06f, 0.08f, 0.10f, 0.88f);

    auto GetHotbarIcon = [](EInventoryItemType ItemType) -> UTexture2D*
    {
        switch (ItemType)
        {
        case EInventoryItemType::CaptureBall:
            return LoadObject<UTexture2D>(nullptr, TEXT("/Game/UI/Icons/T_Icon_Ball.T_Icon_Ball"));
        case EInventoryItemType::GreatSword:
            return LoadObject<UTexture2D>(nullptr, TEXT("/Game/UI/Icons/T_Icon_Sword.T_Icon_Sword"));
        case EInventoryItemType::ChickenMeat:
            return LoadObject<UTexture2D>(nullptr, TEXT("/Game/UI/Icons/T_Icon_Chicken.T_Icon_Chicken"));
        case EInventoryItemType::RawMeat:
            return LoadObject<UTexture2D>(nullptr, TEXT("/Game/UI/Icons/T_Icon_Meat.T_Icon_Meat"));
        case EInventoryItemType::FishMeat:
            return LoadObject<UTexture2D>(nullptr, TEXT("/Game/UI/Icons/T_Icon_Fish.T_Icon_Fish"));
        default:
            return nullptr;
        }
    };

    auto DrawSlot = [this, SlotWidth, SlotHeight, &GetHotbarIcon](float X, float Y, const FString& Label, EInventoryItemType ItemType, bool bSelected, const FLinearColor& SelectedColor, const FLinearColor& IdleColor)
    {
        FCanvasTileItem Background(FVector2D(X, Y), FVector2D(SlotWidth, SlotHeight), bSelected ? SelectedColor : IdleColor);
        Background.BlendMode = SE_BLEND_Translucent;
        Canvas->DrawItem(Background);

        if (UTexture2D* IconTexture = GetHotbarIcon(ItemType))
        {
            const float IconExtent = FMath::Min(SlotWidth - 26.0f, SlotHeight - 26.0f);
            FCanvasTileItem IconItem(
                FVector2D(X + (SlotWidth - IconExtent) * 0.5f, Y + 3.0f),
                IconTexture->GetResource(),
                FVector2D(IconExtent, IconExtent),
                FLinearColor::White
            );
            IconItem.BlendMode = SE_BLEND_Translucent;
            Canvas->DrawItem(IconItem);
        }
        FCanvasTextItem TextItem(FVector2D(X + SlotWidth * 0.5f, Y + SlotHeight - 17.0f), FText::FromString(Label), GEngine->GetSmallFont(), FLinearColor::White);
        TextItem.bCentreX = true;
        Canvas->DrawItem(TextItem);
    };

    auto GetItemLabel = [](const FInventoryItemStack& Stack)
    {
        if (Stack.IsEmpty()) return FString(TEXT("EMPTY"));
        switch (Stack.ItemType)
        {
        case EInventoryItemType::CaptureBall: return FString(TEXT("BALL"));
        case EInventoryItemType::GreatSword: return FString(TEXT("SWORD +3"));
        case EInventoryItemType::ChickenMeat: return FString(TEXT("CHICKEN"));
        case EInventoryItemType::RawMeat: return FString(TEXT("MEAT"));
        case EInventoryItemType::FishMeat: return FString(TEXT("FISH"));
        default: return FString(TEXT("EMPTY"));
        }
    };
    for (int32 Index = 0; Index < 9; ++Index)
    {
        const FInventoryItemStack Stack = HotbarItems.IsValidIndex(Index) ? HotbarItems[Index] : FInventoryItemStack();
        const FString Count = Stack.IsEmpty() || Stack.Count == 1 ? FString() : FString::Printf(TEXT(" x%d"), Stack.Count);
        DrawSlot(StartX + Index * (SlotWidth + Gap), SlotY, FString::Printf(TEXT("[%d] %s%s"), Index + 1, *GetItemLabel(Stack), *Count), Stack.ItemType, SelectedSlot == Index, SelectedColor, IdleColor);
    }

    const float HealthWidth = 260.0f;
    const float HealthX = (Canvas->SizeX - HealthWidth) * 0.5f;
    const float HealthY = SlotY - 54.0f;
    FCanvasTileItem HealthBackground(FVector2D(HealthX, HealthY), FVector2D(HealthWidth, 16.0f), FLinearColor(0.05f, 0.05f, 0.05f, 0.9f));
    HealthBackground.BlendMode = SE_BLEND_Translucent;
    Canvas->DrawItem(HealthBackground);
    FCanvasTileItem HealthFill(FVector2D(HealthX + 2.0f, HealthY + 2.0f), FVector2D((HealthWidth - 4.0f) * FMath::Clamp(PlayerHealth / 30.0f, 0.0f, 1.0f), 12.0f), FLinearColor(0.85f, 0.16f, 0.14f, 1.0f));
    HealthFill.BlendMode = SE_BLEND_Translucent;
    Canvas->DrawItem(HealthFill);
    FCanvasTileItem HungerBackground(FVector2D(HealthX, HealthY - 20.0f), FVector2D(HealthWidth, 12.0f), FLinearColor(0.05f, 0.05f, 0.05f, 0.9f));
    HungerBackground.BlendMode = SE_BLEND_Translucent;
    Canvas->DrawItem(HungerBackground);
    FCanvasTileItem HungerFill(FVector2D(HealthX + 2.0f, HealthY - 18.0f), FVector2D((HealthWidth - 4.0f) * FMath::Clamp(Hunger / 100.0f, 0.0f, 1.0f), 8.0f), FLinearColor(0.94f, 0.62f, 0.18f, 1.0f));
    HungerFill.BlendMode = SE_BLEND_Translucent;
    Canvas->DrawItem(HungerFill);
    const EInventoryItemType SelectedItem = HotbarItems.IsValidIndex(SelectedSlot) ? HotbarItems[SelectedSlot].ItemType : EInventoryItemType::None;
    const FString ActionHint = bCaptureAimActive
        ? TEXT("AIMING - RELEASE LEFT MOUSE TO THROW")
        : (SelectedItem == EInventoryItemType::CaptureBall
            ? TEXT("HOLD LEFT MOUSE: AIM / RELEASE: THROW")
            : (SelectedItem == EInventoryItemType::GreatSword ? TEXT("LEFT: ATTACK / RIGHT: SLASH / SHIFT: RUN") : ((SelectedItem == EInventoryItemType::ChickenMeat || SelectedItem == EInventoryItemType::RawMeat || SelectedItem == EInventoryItemType::FishMeat) ? TEXT("RIGHT MOUSE: EAT") : TEXT("LEFT MOUSE: ATTACK"))));
    const int32 AttackValue = SelectedItem == EInventoryItemType::GreatSword ? 6 : 3;
    FCanvasTextItem HealthText(FVector2D(HealthX + HealthWidth * 0.5f, HealthY - 38.0f), FText::FromString(FString::Printf(TEXT("HP %.0f / 30   HUNGER %.0f / 100   ATK %d   [E] INVENTORY   %s"), PlayerHealth, Hunger, AttackValue, *ActionHint)), GEngine->GetSmallFont(), FLinearColor::White);
    HealthText.bCentreX = true;
    Canvas->DrawItem(HealthText);

    if (bCaptureAimActive)
    {
        const FVector2D Center(Canvas->SizeX * 0.5f, Canvas->SizeY * 0.5f);
        FCanvasTileItem HorizontalSight(
            Center - FVector2D(12.0f, 1.0f),
            FVector2D(24.0f, 2.0f),
            FLinearColor::White
        );
        FCanvasTileItem VerticalSight(
            Center - FVector2D(1.0f, 12.0f),
            FVector2D(2.0f, 24.0f),
            FLinearColor::White
        );
        Canvas->DrawItem(HorizontalSight);
        Canvas->DrawItem(VerticalSight);
    }

    // This mirrors the fight prototype's screen-space life-bar approach and
    // remains visible even when a skeletal mesh has an unusual local scale.
    if (APlayerController* PlayerController = GetOwningPlayerController())
    {
        for (TActorIterator<AAnimalCharacter> It(GetWorld()); It; ++It)
        {
            AAnimalCharacter* Animal = *It;
            if (!Animal || !Animal->ShouldDisplayCombatHealthBar())
            {
                continue;
            }

            FVector2D ScreenPosition;
            if (!PlayerController->ProjectWorldLocationToScreen(Animal->GetCombatHealthBarLocation(), ScreenPosition, true))
            {
                continue;
            }

            constexpr float BarWidth = 152.0f;
            constexpr float BarHeight = 13.0f;
            const FVector2D BarPosition(ScreenPosition.X - BarWidth * 0.5f, ScreenPosition.Y);
            FCanvasTileItem BarBackground(BarPosition, FVector2D(BarWidth, BarHeight), FLinearColor(0.02f, 0.02f, 0.02f, 0.9f));
            BarBackground.BlendMode = SE_BLEND_Translucent;
            Canvas->DrawItem(BarBackground);

            FCanvasTileItem BarFill(
                BarPosition + FVector2D(2.0f, 2.0f),
                FVector2D((BarWidth - 4.0f) * Animal->GetHealthPercent(), BarHeight - 4.0f),
                FLinearColor(0.18f, 0.84f, 0.26f, 1.0f));
            BarFill.BlendMode = SE_BLEND_Translucent;
            Canvas->DrawItem(BarFill);

            FCanvasTextItem NameText(
                FVector2D(ScreenPosition.X, ScreenPosition.Y - 18.0f),
                FText::FromString(FString::Printf(TEXT("%s  %.0f / %.0f"), *Animal->GetCombatDisplayName(), Animal->GetCurrentHealth(), Animal->GetMaxHealth())),
                GEngine->GetSmallFont(),
                FLinearColor::White);
            NameText.bCentreX = true;
            Canvas->DrawItem(NameText);
        }
    }

    const FLinearColor PanelColor(0.035f, 0.055f, 0.075f, 0.92f);
    const FLinearColor EmptyColor(0.11f, 0.15f, 0.18f, 0.90f);
    const FLinearColor ActiveColor(0.12f, 0.62f, 0.68f, 0.96f);
    const FLinearColor AccentColor(0.96f, 0.75f, 0.20f, 0.96f);
    const FLinearColor DangerColor(0.76f, 0.19f, 0.18f, 0.96f);

    auto DrawLabel = [this](const FVector2D& Position, const FString& Text, const FLinearColor& Color, float Scale = 1.0f)
    {
        FCanvasTextItem Item(Position, FText::FromString(Text), GEngine->GetSmallFont(), Color);
        Item.Scale = FVector2D(Scale, Scale);
        Canvas->DrawItem(Item);
    };

    if (bBaseInteractionPromptVisible && !bComputerOpen)
    {
        const FVector2D PromptPosition(Canvas->SizeX * 0.5f - 170.0f, Canvas->SizeY * 0.58f);
        FCanvasTileItem PromptBackground(PromptPosition, FVector2D(340.0f, 42.0f), FLinearColor(0.02f, 0.04f, 0.05f, 0.90f));
        PromptBackground.BlendMode = SE_BLEND_Translucent;
        Canvas->DrawItem(PromptBackground);
        DrawLabel(PromptPosition + FVector2D(45.0f, 12.0f), TEXT("[Y]  INTERACT WITH BASE"), AccentColor, 1.05f);
    }

    if (!bComputerOpen)
    {
        const float PartyX = 18.0f;
        const float PartyY = 56.0f;
        const float PartyWidth = 225.0f;
        const float PartyRowHeight = 34.0f;
        DrawLabel(FVector2D(PartyX, PartyY - 22.0f), FString::Printf(TEXT("PARTY  %d/6   [P] COMPUTER"), PartyMembers.Num()), FLinearColor::White);

        for (int32 Index = 0; Index < 6; ++Index)
        {
            const bool bFilled = PartyMembers.IsValidIndex(Index);
            const bool bSelected = bFilled && Index == SelectedPartnerIndex;
            FCanvasTileItem Row(
                FVector2D(PartyX, PartyY + Index * (PartyRowHeight + 4.0f)),
                FVector2D(PartyWidth, PartyRowHeight),
                bSelected ? ActiveColor : (bFilled ? PanelColor : EmptyColor)
            );
            Row.BlendMode = SE_BLEND_Translucent;
            Canvas->DrawItem(Row);

            const FString Name = bFilled ? PartyMembers[Index] : TEXT("-- EMPTY --");
            const FString Status = PartyDeployed.IsValidIndex(Index) && PartyDeployed[Index] ? TEXT("  OUT") : TEXT("");
            DrawLabel(
                FVector2D(PartyX + 10.0f, PartyY + Index * (PartyRowHeight + 4.0f) + 8.0f),
                FString::Printf(TEXT("%d  %s%s"), Index + 1, *Name, *Status),
                bSelected ? FLinearColor(0.02f, 0.04f, 0.05f, 1.0f) : FLinearColor::White
            );
        }
        return;
    }

    FCanvasTileItem Dimmer(
        FVector2D::ZeroVector,
        FVector2D(Canvas->SizeX, Canvas->SizeY),
        FLinearColor(0.0f, 0.0f, 0.0f, 0.78f)
    );
    Dimmer.BlendMode = SE_BLEND_Translucent;
    Canvas->DrawItem(Dimmer);

    const float PanelWidth = FMath::Min(1050.0f, Canvas->SizeX - 70.0f);
    const float PanelHeight = FMath::Min(650.0f, Canvas->SizeY - 70.0f);
    const float PanelX = (Canvas->SizeX - PanelWidth) * 0.5f;
    const float PanelY = (Canvas->SizeY - PanelHeight) * 0.5f;
    FCanvasTileItem MainPanel(FVector2D(PanelX, PanelY), FVector2D(PanelWidth, PanelHeight), PanelColor);
    MainPanel.BlendMode = SE_BLEND_Translucent;
    Canvas->DrawItem(MainPanel);

    DrawLabel(FVector2D(PanelX + 24.0f, PanelY + 20.0f), StorageTitle, AccentColor, 1.5f);
    constexpr int32 VisibleSlots = 15;
    const int32 StoragePage = ComputerSelectionIndex >= 0 && bComputerSelectingStorage
        ? ComputerSelectionIndex / VisibleSlots
        : 0;
    const int32 StoragePageCount = FMath::Max(1, FMath::DivideAndRoundUp(ComputerMembers.Num(), VisibleSlots));
    DrawLabel(FVector2D(PanelX + PanelWidth - 260.0f, PanelY + 25.0f), FString::Printf(TEXT("BOX 1  %d CREATURES  %d/%d"), ComputerMembers.Num(), StoragePage + 1, StoragePageCount), FLinearColor::White);

    const float ContentY = PanelY + 72.0f;
    const float PartyColumnWidth = PanelWidth * 0.31f;
    const float StorageX = PanelX + PartyColumnWidth + 42.0f;
    const float StorageWidth = PanelWidth - PartyColumnWidth - 66.0f;
    DrawLabel(FVector2D(PanelX + 24.0f, ContentY - 25.0f), TEXT("PARTY"), bComputerSelectingStorage ? FLinearColor::White : AccentColor, 1.1f);
    DrawLabel(FVector2D(StorageX, ContentY - 25.0f), StorageLabel, bComputerSelectingStorage ? AccentColor : FLinearColor::White, 1.1f);

    const float PartyRowHeight = 52.0f;
    for (int32 Index = 0; Index < 6; ++Index)
    {
        const bool bFilled = PartyMembers.IsValidIndex(Index);
        const bool bSelected = !bComputerSelectingStorage && Index == ComputerSelectionIndex;
        FCanvasTileItem Row(
            FVector2D(PanelX + 24.0f, ContentY + Index * (PartyRowHeight + 7.0f)),
            FVector2D(PartyColumnWidth - 42.0f, PartyRowHeight),
            bSelected ? ActiveColor : (bFilled ? EmptyColor : FLinearColor(0.06f, 0.08f, 0.10f, 0.55f))
        );
        Row.BlendMode = SE_BLEND_Translucent;
        Canvas->DrawItem(Row);
        DrawLabel(
            FVector2D(PanelX + 38.0f, ContentY + Index * (PartyRowHeight + 7.0f) + 16.0f),
            bFilled ? FString::Printf(TEXT("%d  %s"), Index + 1, *PartyMembers[Index]) : TEXT("-- EMPTY --"),
            bSelected ? FLinearColor(0.02f, 0.04f, 0.05f, 1.0f) : FLinearColor::White
        );
    }

    constexpr int32 Columns = 3;
    const float CellGap = 9.0f;
    const float CellWidth = (StorageWidth - CellGap * (Columns - 1)) / Columns;
    const float CellHeight = 75.0f;
    for (int32 Index = 0; Index < VisibleSlots; ++Index)
    {
        const int32 StorageIndex = StoragePage * VisibleSlots + Index;
        const bool bFilled = ComputerMembers.IsValidIndex(StorageIndex);
        const bool bSelected = bComputerSelectingStorage && StorageIndex == ComputerSelectionIndex;
        const int32 RowIndex = Index / Columns;
        const int32 ColumnIndex = Index % Columns;
        const FVector2D CellPosition(
            StorageX + ColumnIndex * (CellWidth + CellGap),
            ContentY + RowIndex * (CellHeight + CellGap)
        );
        FCanvasTileItem Cell(
            CellPosition,
            FVector2D(CellWidth, CellHeight),
            bSelected ? ActiveColor : (bFilled ? EmptyColor : FLinearColor(0.06f, 0.08f, 0.10f, 0.55f))
        );
        Cell.BlendMode = SE_BLEND_Translucent;
        Canvas->DrawItem(Cell);
        DrawLabel(
            CellPosition + FVector2D(10.0f, 12.0f),
            bFilled ? ComputerMembers[StorageIndex] : TEXT("EMPTY"),
            bSelected ? FLinearColor(0.02f, 0.04f, 0.05f, 1.0f) : FLinearColor::White
        );
        if (bFilled)
        {
            DrawLabel(CellPosition + FVector2D(10.0f, 39.0f), FString::Printf(TEXT("No. %03d"), StorageIndex + 1), bSelected ? FLinearColor(0.02f, 0.04f, 0.05f, 1.0f) : FLinearColor(0.65f, 0.72f, 0.75f, 1.0f));
        }
    }

    const float FooterY = PanelY + PanelHeight - 58.0f;
    FCanvasTileItem ReleaseButton(FVector2D(PanelX + 24.0f, FooterY), FVector2D(165.0f, 34.0f), DangerColor);
    ReleaseButton.BlendMode = SE_BLEND_Translucent;
    Canvas->DrawItem(ReleaseButton);
    DrawLabel(FVector2D(PanelX + 42.0f, FooterY + 8.0f), TEXT("[X] RELEASE"), FLinearColor::White);
    DrawLabel(FVector2D(PanelX + 215.0f, FooterY + 8.0f), TEXT("DRAG: MOVE / SWAP   [X] RELEASE   [P] CLOSE"), FLinearColor::White);

    if (bReleaseConfirmationPending)
    {
        FCanvasTileItem ModalDimmer(FVector2D::ZeroVector, FVector2D(Canvas->SizeX, Canvas->SizeY), FLinearColor(0.0f, 0.0f, 0.0f, 0.52f));
        ModalDimmer.BlendMode = SE_BLEND_Translucent;
        Canvas->DrawItem(ModalDimmer);
        const FVector2D ConfirmPosition(Canvas->SizeX * 0.5f - 270.0f, Canvas->SizeY * 0.5f - 110.0f);
        FCanvasTileItem ConfirmBackground(ConfirmPosition, FVector2D(540.0f, 220.0f), FLinearColor(0.20f, 0.025f, 0.025f, 0.99f));
        ConfirmBackground.BlendMode = SE_BLEND_Translucent;
        Canvas->DrawItem(ConfirmBackground);
        DrawLabel(ConfirmPosition + FVector2D(158.0f, 30.0f), TEXT("RELEASE CREATURE?"), FLinearColor(1.0f, 0.45f, 0.42f, 1.0f), 1.55f);
        DrawLabel(ConfirmPosition + FVector2D(76.0f, 94.0f), FString::Printf(TEXT("Release %s permanently?"), *ReleaseConfirmationPartnerName), FLinearColor::White, 1.15f);
        DrawLabel(ConfirmPosition + FVector2D(88.0f, 155.0f), TEXT("[Y] CONFIRM RELEASE     [N] CANCEL"), AccentColor, 1.1f);
    }

    if (bComputerDragActive)
    {
        APlayerController* PlayerController = GetOwningPlayerController();
        float MouseX = 0.0f;
        float MouseY = 0.0f;
        if (PlayerController && PlayerController->GetMousePosition(MouseX, MouseY))
        {
            FCanvasTileItem DragBackground(
                FVector2D(MouseX + 12.0f, MouseY + 12.0f),
                FVector2D(180.0f, 34.0f),
                FLinearColor(0.12f, 0.62f, 0.68f, 0.95f));
            DragBackground.BlendMode = SE_BLEND_Translucent;
            Canvas->DrawItem(DragBackground);
            DrawLabel(FVector2D(MouseX + 22.0f, MouseY + 20.0f), ComputerDragName, FLinearColor(0.02f, 0.04f, 0.05f, 1.0f));
        }
    }
}
