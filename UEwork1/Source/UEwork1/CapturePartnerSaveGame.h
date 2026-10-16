#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "CapturePartnerSaveGame.generated.h"

UENUM(BlueprintType)
enum class EInventoryItemType : uint8
{
    None,
    CaptureBall,
    GreatSword,
    ChickenMeat,
    RawMeat,
    FishMeat
};

USTRUCT(BlueprintType)
struct FInventoryItemStack
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EInventoryItemType ItemType = EInventoryItemType::None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Count = 0;

    bool IsEmpty() const { return ItemType == EInventoryItemType::None || Count <= 0; }
    void Clear() { ItemType = EInventoryItemType::None; Count = 0; }
};

USTRUCT()
struct FCapturedPartnerSaveData
{
    GENERATED_BODY()

    UPROPERTY()
    FSoftClassPath AnimalClass;

    UPROPERTY()
    FString DisplayName;

    // Stable identity lets a deployed companion survive party reordering.
    UPROPERTY()
    FString PartnerId;

    // 0 Neutral, 1 Passive, 2 AttackMyTarget. Old saves deserialize as Neutral.
    UPROPERTY()
    uint8 BehaviorMode = 0;
};

/** Persisted contents for one physical base terminal. */
USTRUCT()
struct FBasePartnerStorageSaveData
{
    GENERATED_BODY()

    UPROPERTY()
    TArray<FCapturedPartnerSaveData> Partners;
};

/** Lightweight entry stored in the profile index, separate from each adventure's data. */
USTRUCT(BlueprintType)
struct FCaptureSaveSlotInfo
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString SlotName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bCreativeMode = false;
};

UCLASS()
class UEWORK1_API UCapturePartnerSaveGame : public USaveGame
{
    GENERATED_BODY()

public:
    // Used only by the profile-index save object.
    UPROPERTY()
    TArray<FCaptureSaveSlotInfo> SaveSlots;

    UPROPERTY()
    FString SaveDisplayName;

    UPROPERTY()
    bool bCreativeMode = false;

    UPROPERTY()
    TArray<FCapturedPartnerSaveData> PartyPartners;

    UPROPERTY()
    TArray<FCapturedPartnerSaveData> ComputerPartners;

    UPROPERTY()
    TArray<FCapturedPartnerSaveData> BasePartners;

    // Keyed by the base terminal's stable world-space identifier. BasePartners
    // remains above solely to migrate saves created before bases were separate.
    UPROPERTY()
    TMap<FString, FBasePartnerStorageSaveData> BasePartnersByTerminal;

    UPROPERTY()
    int32 SelectedPartnerIndex = -1;

    // Kept in the same save slot as the party so old saves migrate without data loss.
    UPROPERTY()
    TArray<FInventoryItemStack> HotbarItems;

    UPROPERTY()
    TArray<FInventoryItemStack> BackpackItems;

    UPROPERTY()
    float Hunger = 100.0f;

    UPROPERTY()
    int32 SelectedHotbarSlot = 0;
};
