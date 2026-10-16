#pragma once

#include "CoreMinimal.h"
#include "Blueprint/DragDropOperation.h"
#include "Blueprint/UserWidget.h"
#include "PartnerStorageWidget.generated.h"

class ACaptureLauncher;
class UBorder;
class UTextBlock;
class UVerticalBox;
class UUniformGridPanel;

/** Carries the source location while a partner slot is being dragged. */
UCLASS()
class UEWORK1_API UPartnerDragDropOperation : public UDragDropOperation
{
    GENERATED_BODY()

public:
    bool bSourceStorage = true;
    int32 SourceIndex = INDEX_NONE;
};

/** A fixed-size slot which owns its drag/drop handling instead of Blueprint graphs. */
UCLASS()
class UEWORK1_API UPartnerSlotWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** Builds the native widget tree before this slot is inserted into a parent layout. */
    void InitializeSlotLayout();
    void SetSlotData(ACaptureLauncher* InLauncher, bool bInStorage, int32 InIndex, const FString& InName, bool bOccupied, bool bAllowInput = true);

protected:
    virtual void NativeConstruct() override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
    virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

private:
    void RefreshVisual();

    TWeakObjectPtr<ACaptureLauncher> Launcher;
    TObjectPtr<UBorder> SlotBorder;
    TObjectPtr<UTextBlock> SlotText;
    bool bStorageSlot = true;
    bool bHasPartner = false;
    bool bInputEnabled = true;
    int32 SlotIndex = INDEX_NONE;
    FString PartnerName;
};

/** Complete partner computer/base storage surface, built with stable layout rules at runtime. */
UCLASS()
class UEWORK1_API UPartnerStorageWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void RefreshFromLauncher(ACaptureLauncher* InLauncher);

protected:
    virtual void NativeConstruct() override;
    virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
    void BuildLayout();
    void BuildSlots();
    UPartnerSlotWidget* CreateSlot(bool bStorage, int32 Index, const FString& Name, bool bOccupied);

    TWeakObjectPtr<ACaptureLauncher> Launcher;
    TObjectPtr<UVerticalBox> PartySlotsBox;
    TObjectPtr<UUniformGridPanel> StorageSlotsGrid;
    bool bLayoutBuilt = false;
};
