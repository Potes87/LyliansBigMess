// Fill out your copyright notice in the Description page of Project Settings.

#include "CombatHelpers.h"
#include "DamageType_Crit.h"
#include "DamageType_CritHeadshot.h"
#include "DamageType_Headshot.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/HitResult.h"
#include "GameFramework/DamageType.h"

bool UCombatHelpers::IsHeadshot(const FHitResult& Hit, const USkeletalMeshComponent* Mesh, FName HeadBone, float HeadZOffset)
{
	if (!Mesh || Mesh->GetBoneIndex(HeadBone) == INDEX_NONE)
	{
		return false;
	}

	// The hit landed on the mesh itself, so trust the bone it hit.
	if (Hit.GetComponent() == Mesh && Hit.BoneName != NAME_None)
	{
		return Hit.BoneName == HeadBone || Mesh->BoneIsChildOf(Hit.BoneName, HeadBone);
	}

	// The capsule caught the bullet, so compare the impact height against the head bone.
	const float HeadZ = Mesh->GetBoneLocation(HeadBone, EBoneSpaces::WorldSpace).Z;
	return Hit.ImpactPoint.Z >= HeadZ + HeadZOffset;
}

TSubclassOf<UDamageType> UCombatHelpers::MakeHitDamageType(bool bIsCrit, bool bIsHeadshot)
{
	if (bIsCrit && bIsHeadshot)
	{
		return UDamageType_CritHeadshot::StaticClass();
	}
	if (bIsCrit)
	{
		return UDamageType_Crit::StaticClass();
	}
	if (bIsHeadshot)
	{
		return UDamageType_Headshot::StaticClass();
	}
	return UDamageType::StaticClass();
}

void UCombatHelpers::BreakHitDamageType(const UDamageType* DamageType, bool& bIsCrit, bool& bIsHeadshot)
{
	bIsCrit = DamageType && DamageType->IsA<UDamageType_Crit>();
	bIsHeadshot = DamageType && (DamageType->IsA<UDamageType_Headshot>() || DamageType->IsA<UDamageType_CritHeadshot>());
}
