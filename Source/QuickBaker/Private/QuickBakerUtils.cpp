// Copyright (c) 2025 Kurorekishi (EmbarrassingMoment). Licensed under MIT License.

#include "QuickBakerUtils.h"

FString FQuickBakerUtils::GetTextureNameFromMaterial(const FString& MaterialName)
{
	FString Name = MaterialName;
	if (Name.StartsWith("M_"))
	{
		Name = "T_" + Name.RightChop(2);
	}
	else if (Name.StartsWith("MI_"))
	{
		Name = "T_" + Name.RightChop(3);
	}
	else
	{
		Name = "T_" + Name;
	}
	return Name;
}
