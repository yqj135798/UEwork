#include "PartnerStorageBlueprintLibrary.h"

#include "CaptureLauncher.h"
#include "Kismet/GameplayStatics.h"

bool UPartnerStorageBlueprintLibrary::MovePartnerSlot(
    UObject* WorldContextObject,
    bool bSourceStorage,
    int32 SourceIndex,
    bool bTargetStorage,
    int32 TargetIndex)
{
    if (!WorldContextObject || TargetIndex < 0)
    {
        return false;
    }

    ACaptureLauncher* Launcher = Cast<ACaptureLauncher>(
        UGameplayStatics::GetActorOfClass(WorldContextObject, ACaptureLauncher::StaticClass()));
    if (!Launcher)
    {
        return false;
    }

    Launcher->MovePartnerFromUMG(bSourceStorage, SourceIndex, bTargetStorage, TargetIndex);
    return true;
}
