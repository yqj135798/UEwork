#pragma once

#include "CoreMinimal.h"
#include "AnimalCharacter.h"
#include "GoblinCharacter.generated.h"

/** Capturable biped goblin using the project's shared humanoid animation library. */
UCLASS()
class UEWORK1_API AGoblinCharacter : public AAnimalCharacter
{
    GENERATED_BODY()

public:
    AGoblinCharacter();
};
