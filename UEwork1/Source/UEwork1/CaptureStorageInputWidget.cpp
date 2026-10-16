#include "CaptureStorageInputWidget.h"

#include "Blueprint/SlateBlueprintLibrary.h"
#include "CaptureLauncher.h"
#include "Components/Border.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"
#include "Blueprint/WidgetTree.h"

void UCaptureStorageInputWidget::SetLauncher(ACaptureLauncher* InLauncher)
{
    Launcher = InLauncher;
}

void UCaptureStorageInputWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (!WidgetTree->RootWidget)
    {
        UBorder* PointerSurface = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
        PointerSurface->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
        PointerSurface->SetVisibility(ESlateVisibility::Visible);
        WidgetTree->RootWidget = PointerSurface;
    }
}

FReply UCaptureStorageInputWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    if (Launcher.IsValid() && Launcher->HandleStorageShortcut(InKeyEvent.GetKey()))
    {
        return FReply::Handled();
    }

    return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply UCaptureStorageInputWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && Launcher.IsValid())
    {
        FVector2D PixelPosition;
        FVector2D ViewportPosition;
        USlateBlueprintLibrary::AbsoluteToViewport(this, InMouseEvent.GetScreenSpacePosition(), PixelPosition, ViewportPosition);
        Launcher->BeginStoragePointerDragAt(PixelPosition);
        bDragging = true;
        return FReply::Handled().CaptureMouse(GetCachedWidget().ToSharedRef());
    }

    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UCaptureStorageInputWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && bDragging)
    {
        if (Launcher.IsValid())
        {
            FVector2D PixelPosition;
            FVector2D ViewportPosition;
            USlateBlueprintLibrary::AbsoluteToViewport(this, InMouseEvent.GetScreenSpacePosition(), PixelPosition, ViewportPosition);
            Launcher->EndStoragePointerDragAt(PixelPosition);
        }
        bDragging = false;
        return FReply::Handled().ReleaseMouseCapture();
    }

    return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}
