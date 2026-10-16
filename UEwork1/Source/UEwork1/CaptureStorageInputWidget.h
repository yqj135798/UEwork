#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CaptureStorageInputWidget.generated.h"

class ACaptureLauncher;

// A full-screen transparent UMG surface. Unlike HUD canvas drawing, this owns
// real Slate pointer events, so mouse capture survives a drag between slots.
UCLASS()
class UEWORK1_API UCaptureStorageInputWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void SetLauncher(ACaptureLauncher* InLauncher);

protected:
    virtual void NativeConstruct() override;
    virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
    TWeakObjectPtr<ACaptureLauncher> Launcher;
    bool bDragging = false;
};
