// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CPP_StaticMeshMover.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CPP_Pickup.generated.h"

UCLASS()
class UNREAL1_API ACPP_Pickup : public AActor
{
	GENERATED_BODY()

public:
	// UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Settings", meta = (AllowPrivateAccess = "true"))
	// TObjectPtr<UBoxComponent> boxComponent;
	//
	// UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings", meta = (AllowPrivateAccess = "true"))
	// TObjectPtr<UStaticMeshComponent> meshComp;
	//
	// UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings", meta = (AllowPrivateAccess = "true"))
	// TObjectPtr<UCPP_StaticMeshMover> StaticMeshMover ;

	// Sets default values for this actor's properties
	ACPP_Pickup();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
};
