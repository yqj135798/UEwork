#include "PartnerStorageWidget.h"

#include "CaptureLauncher.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"
#include "Styling/SlateTypes.h"
#include "Blueprint/WidgetTree.h"

namespace
{
    constexpr FLinearColor WindowColor(0.035f, 0.070f, 0.115f, 0.985f);
    constexpr FLinearColor PartyColor(0.075f, 0.185f, 0.250f, 1.0f);
    constexpr FLinearColor StorageColor(0.115f, 0.095f, 0.220f, 1.0f);
    constexpr FLinearColor EmptyColor(0.045f, 0.055f, 0.075f, 1.0f);

    UTextBlock* MakeText(UWidgetTree* Tree, const FString& Value, int32 Size, const FLinearColor& Color = FLinearColor::White)
    {
        UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        Text->SetText(FText::FromString(Value));
        Text->SetFont(FSlateFontInfo(Text->GetFont().FontObject, Size));
        Text->SetColorAndOpacity(FSlateColor(Color));
        Text->SetJustification(ETextJustify::Center);
        return Text;
    }

    USizeBox* WrapFixed(UWidgetTree* Tree, UWidget* Content, float Width, float Height)
    {
        USizeBox* Box = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
        Box->SetWidthOverride(Width);
        Box->SetHeightOverride(Height);
        Box->AddChild(Content);
        return Box;
    }
}

void UPartnerSlotWidget::SetSlotData(ACaptureLauncher* InLauncher, bool bInStorage, int32 InIndex, const FString& InName, bool bOccupied, bool bAllowInput)
{
    Launcher = InLauncher;
    bStorageSlot = bInStorage;
    SlotIndex = InIndex;
    PartnerName = InName;
    bHasPartner = bOccupied;
    bInputEnabled = bAllowInput;
    RefreshVisual();
}

void UPartnerSlotWidget::InitializeSlotLayout()
{
    if (SlotBorder && SlotText)
    {
        return;
    }

    // These slots are native UUserWidgets created as children at runtime. Build the
    // tree before adding one to a panel so Slate always receives a visible hit target.
    SlotBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    SlotBorder->SetPadding(FMargin(10.0f, 8.0f));
    SlotText = MakeText(WidgetTree, TEXT("空位"), 18);
    SlotBorder->SetContent(SlotText);
    WidgetTree->RootWidget = SlotBorder;
}

void UPartnerSlotWidget::NativeConstruct()
{
    Super::NativeConstruct();
    InitializeSlotLayout();
    RefreshVisual();
}

void UPartnerSlotWidget::RefreshVisual()
{
    if (!SlotBorder || !SlotText)
    {
        return;
    }

    const FString Group = bStorageSlot ? TEXT("仓库") : TEXT("队伍");
    const FString Label = bHasPartner
        ? FString::Printf(TEXT("%s %d   %s"), *Group, SlotIndex + 1, *PartnerName)
        : FString::Printf(TEXT("%s %d   空位"), *Group, SlotIndex + 1);
    SlotText->SetText(FText::FromString(Label));
    SlotText->SetColorAndOpacity(FSlateColor(bHasPartner ? FLinearColor::White : FLinearColor(0.60f, 0.65f, 0.72f, 1.0f)));
    SlotBorder->SetBrushColor(bHasPartner ? (bStorageSlot ? StorageColor : PartyColor) : EmptyColor);
}

FReply UPartnerSlotWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (bInputEnabled && bHasPartner && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        return FReply::Handled().DetectDrag(GetCachedWidget().ToSharedRef(), EKeys::LeftMouseButton);
    }

    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UPartnerSlotWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
    Super::NativeOnDragDetected(InGeometry, InMouseEvent, OutOperation);
    if (!bInputEnabled || !bHasPartner || !Launcher.IsValid())
    {
        return;
    }

    UClass* OperationClass = LoadClass<UPartnerDragDropOperation>(
        nullptr,
        TEXT("/Game/UI/BP_PartnerDragOperation.BP_PartnerDragOperation_C")
    );
    UPartnerDragDropOperation* Operation = NewObject<UPartnerDragDropOperation>(
        this,
        OperationClass ? OperationClass : UPartnerDragDropOperation::StaticClass()
    );
    Operation->bSourceStorage = bStorageSlot;
    Operation->SourceIndex = SlotIndex;
    Operation->Pivot = EDragPivot::MouseDown;
    // Drag operation offsets are normalized ratios, not screen pixels. Keeping
    // this at zero anchors the preview at the mouse-down point instead of
    // moving it far outside the viewport.
    Operation->Offset = FVector2D::ZeroVector;

    UClass* DragVisualClass = LoadClass<UPartnerSlotWidget>(
        nullptr,
        TEXT("/Game/UI/WBP_PartnerSlot.WBP_PartnerSlot_C")
    );
    UPartnerSlotWidget* DragVisual = DragVisualClass
        ? CreateWidget<UPartnerSlotWidget>(GetOwningPlayer(), DragVisualClass)
        : CreateWidget<UPartnerSlotWidget>(GetOwningPlayer());
    if (!DragVisual)
    {
        return;
    }
    DragVisual->InitializeSlotLayout();
    DragVisual->SetSlotData(nullptr, bStorageSlot, SlotIndex, PartnerName, true, false);
    DragVisual->SetRenderOpacity(0.92f);
    Operation->DefaultDragVisual = DragVisual;
    OutOperation = Operation;
}

bool UPartnerSlotWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    if (UPartnerDragDropOperation* Operation = Cast<UPartnerDragDropOperation>(InOperation))
    {
        if (Launcher.IsValid() && (Operation->bSourceStorage != bStorageSlot || Operation->SourceIndex != SlotIndex))
        {
            Launcher->MovePartnerFromUMG(Operation->bSourceStorage, Operation->SourceIndex, bStorageSlot, SlotIndex);
            return true;
        }
    }

    return Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);
}

void UPartnerStorageWidget::NativeConstruct()
{
    Super::NativeConstruct();
    BuildLayout();
    BuildSlots();
}

FReply UPartnerStorageWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    if (Launcher.IsValid() && Launcher->HandleStorageShortcut(InKeyEvent.GetKey()))
    {
        return FReply::Handled();
    }

    return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UPartnerStorageWidget::RefreshFromLauncher(ACaptureLauncher* InLauncher)
{
    Launcher = InLauncher;
    BuildLayout();
    BuildSlots();
}

void UPartnerStorageWidget::BuildLayout()
{
    if (bLayoutBuilt)
    {
        return;
    }

    UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
    WidgetTree->RootWidget = Root;

    UBorder* Backdrop = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    Backdrop->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.68f));
    if (UCanvasPanelSlot* BackdropSlot = Root->AddChildToCanvas(Backdrop))
    {
        BackdropSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
        BackdropSlot->SetOffsets(FMargin(0.0f));
    }

    UBorder* Window = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    Window->SetBrushColor(WindowColor);
    Window->SetPadding(FMargin(24.0f));
    if (UCanvasPanelSlot* WindowSlot = Root->AddChildToCanvas(Window))
    {
        WindowSlot->SetAnchors(FAnchors(0.5f, 0.5f));
        WindowSlot->SetAlignment(FVector2D(0.5f, 0.5f));
        WindowSlot->SetSize(FVector2D(1240.0f, 760.0f));
    }

    UVerticalBox* WindowContent = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    Window->SetContent(WindowContent);
    UTextBlock* Title = MakeText(WidgetTree, TEXT("伙伴电脑 / 储存箱"), 30, FLinearColor(0.45f, 0.86f, 1.0f, 1.0f));
    if (UVerticalBoxSlot* TitleSlot = WindowContent->AddChildToVerticalBox(WrapFixed(WidgetTree, Title, 0.0f, 54.0f)))
    {
        TitleSlot->SetHorizontalAlignment(HAlign_Fill);
        TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));
    }

    UHorizontalBox* Columns = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    if (UVerticalBoxSlot* ColumnsSlot = WindowContent->AddChildToVerticalBox(Columns))
    {
        ColumnsSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    }

    UBorder* PartyPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    PartyPanel->SetBrushColor(FLinearColor(0.055f, 0.10f, 0.15f, 1.0f));
    PartyPanel->SetPadding(FMargin(16.0f));
    if (UHorizontalBoxSlot* PartyPanelSlot = Columns->AddChildToHorizontalBox(PartyPanel))
    {
        PartyPanelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
        PartyPanelSlot->SetPadding(FMargin(0.0f, 0.0f, 18.0f, 0.0f));
    }
    UVerticalBox* PartyContent = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    PartyPanel->SetContent(PartyContent);
    PartyContent->AddChildToVerticalBox(WrapFixed(WidgetTree, MakeText(WidgetTree, TEXT("随身队伍（最多 6 只）"), 20, FLinearColor(0.50f, 0.82f, 1.0f, 1.0f)), 300.0f, 38.0f));
    PartySlotsBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    if (UVerticalBoxSlot* PartySlotsLayout = PartyContent->AddChildToVerticalBox(PartySlotsBox))
    {
        PartySlotsLayout->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    }

    UBorder* StoragePanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    StoragePanel->SetBrushColor(FLinearColor(0.09f, 0.06f, 0.16f, 1.0f));
    StoragePanel->SetPadding(FMargin(16.0f));
    if (UHorizontalBoxSlot* StoragePanelSlot = Columns->AddChildToHorizontalBox(StoragePanel))
    {
        StoragePanelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    }
    UVerticalBox* StorageContent = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    StoragePanel->SetContent(StorageContent);
    StorageContent->AddChildToVerticalBox(WrapFixed(WidgetTree, MakeText(WidgetTree, TEXT("电脑仓库   鼠标左键拖动即可移动 / 对调"), 20, FLinearColor(0.78f, 0.66f, 1.0f, 1.0f)), 0.0f, 38.0f));
    UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
    Scroll->SetOrientation(EOrientation::Orient_Vertical);
    StorageSlotsGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass());
    Scroll->AddChild(StorageSlotsGrid);
    if (UVerticalBoxSlot* ScrollLayout = StorageContent->AddChildToVerticalBox(Scroll))
    {
        ScrollLayout->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    }

    bLayoutBuilt = true;
}

UPartnerSlotWidget* UPartnerStorageWidget::CreateSlot(bool bStorage, int32 Index, const FString& Name, bool bOccupied)
{
    // Instantiate the Blueprint subclass so the slot is inspectable and extendable
    // from WBP_PartnerSlot, while retaining a native fallback for a missing asset.
    UClass* SlotWidgetClass = LoadClass<UPartnerSlotWidget>(
        nullptr,
        TEXT("/Game/UI/WBP_PartnerSlot.WBP_PartnerSlot_C")
    );
    UPartnerSlotWidget* SlotWidget = SlotWidgetClass
        ? CreateWidget<UPartnerSlotWidget>(GetOwningPlayer(), SlotWidgetClass)
        : CreateWidget<UPartnerSlotWidget>(GetOwningPlayer());
    if (!SlotWidget)
    {
        return nullptr;
    }
    SlotWidget->InitializeSlotLayout();
    SlotWidget->SetSlotData(Launcher.Get(), bStorage, Index, Name, bOccupied);
    return SlotWidget;
}

void UPartnerStorageWidget::BuildSlots()
{
    if (!PartySlotsBox || !StorageSlotsGrid || !Launcher.IsValid())
    {
        return;
    }

    PartySlotsBox->ClearChildren();
    const TArray<FPartnerRecord>& Party = Launcher->PartyPartners;
    for (int32 Index = 0; Index < 6; ++Index)
    {
        const bool bOccupied = Party.IsValidIndex(Index);
        UPartnerSlotWidget* PartnerSlot = CreateSlot(false, Index, bOccupied ? Party[Index].DisplayName : FString(), bOccupied);
        if (!PartnerSlot)
        {
            continue;
        }
        USizeBox* SizedSlot = WrapFixed(WidgetTree, PartnerSlot, 300.0f, 72.0f);
        if (UVerticalBoxSlot* Layout = PartySlotsBox->AddChildToVerticalBox(SizedSlot))
        {
            Layout->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 9.0f));
        }
    }

    StorageSlotsGrid->ClearChildren();
    const TArray<FPartnerRecord>& Storage = Launcher->GetActiveStoragePartners();
    for (int32 Index = 0; Index < Storage.Num() + 1; ++Index)
    {
        const bool bOccupied = Storage.IsValidIndex(Index);
        UPartnerSlotWidget* PartnerSlot = CreateSlot(true, Index, bOccupied ? Storage[Index].DisplayName : FString(), bOccupied);
        if (!PartnerSlot)
        {
            continue;
        }
        USizeBox* SizedSlot = WrapFixed(WidgetTree, PartnerSlot, 190.0f, 102.0f);
        if (UUniformGridSlot* Layout = StorageSlotsGrid->AddChildToUniformGrid(SizedSlot, Index / 4, Index % 4))
        {
            Layout->SetHorizontalAlignment(HAlign_Fill);
            Layout->SetVerticalAlignment(VAlign_Fill);
        }
    }
}
