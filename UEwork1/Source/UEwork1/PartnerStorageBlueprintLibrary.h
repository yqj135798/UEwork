#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "PartnerStorageBlueprintLibrary.generated.h"

/** Blueprint bridge used by WBP_PartnerSlot after a UMG drop succeeds. */
UCLASS()
class UEWORK1_API UPartnerStorageBlueprintLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Partner Storage", meta = (WorldContext = "WorldContextObject"))
    static bool MovePartnerSlot(
        UObject* WorldContextObject,
        bool bSourceStorage,
        int32 SourceIndex,
        bool bTargetStorage,
        int32 TargetIndex
    );
};
