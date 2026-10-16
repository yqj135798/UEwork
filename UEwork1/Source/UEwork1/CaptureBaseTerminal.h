#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CaptureBaseTerminal.generated.h"

UCLASS()
class UEWORK1_API ACaptureBaseTerminal : public AActor
{
    GENERATED_BODY()

public:
    ACaptureBaseTerminal();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    void BindToExistingFountain(AActor* ExistingFountain);
    FString GetStorageId() const;
    FVector GetPartnerSpawnLocation(int32 SlotIndex) const;

private:
    UPROPERTY(VisibleAnywhere, Category = "Base")
    TObjectPtr<class USceneComponent> Root;

    UPROPERTY(VisibleAnywhere, Category = "Base")
    TObjectPtr<class UChildActorComponent> FountainVisual;

    UPROPERTY(EditDefaultsOnly, Category = "Base")
    float InteractionRadius = 420.0f;

    bool bWasPlayerInRange = false;

    TWeakObjectPtr<AActor> BoundFountain;
};
