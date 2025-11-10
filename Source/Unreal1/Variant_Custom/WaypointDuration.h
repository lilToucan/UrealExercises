#pragma once
#include "CoreMinimal.h"
#include "WaypointDuration.generated.h"

USTRUCT(BlueprintType)
struct FWaypointDuration
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (MakeEditWidget),category = "Settings")
	FVector Waypoint;
	UPROPERTY(BlueprintReadWrite, EditAnywhere,category = "Settings")
	float Duration;
};