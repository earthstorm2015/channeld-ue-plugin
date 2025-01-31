﻿#include "ReplicatedActorDecorator.h"

#include "PropertyDecoratorFactory.h"
#include "ReplicatorGeneratorDefinition.h"
#include "ReplicatorGeneratorUtils.h"
#include "GameFramework/GameStateBase.h"
#include "ReplicatorTemplate/CppReplicatorTemplate.h"

FReplicatedActorDecorator::FReplicatedActorDecorator(
	const UClass* TargetActorClass
	, const TFunction<void(FString&, bool)>& SetCompilableName
	, FString InProtoPackageName
	, FString InGoPackageImportPath
	, bool IsSingleton
	, bool IsChanneldUEBuiltinType
	, bool IsSkipGenReplicator
	, bool IsSkipGenChannelDataState
) : TargetClass(TargetActorClass)
    , ProtoPackageName(InProtoPackageName)
    , GoPackageImportPath(InGoPackageImportPath)
    , bSingleton(IsSingleton)
    , bChanneldUEBuiltinType(IsChanneldUEBuiltinType)
    , bSkipGenReplicator(IsSkipGenReplicator)
    , bSkipGenChannelDataState(IsSkipGenChannelDataState)
{
	TargetClass = TargetActorClass;
	bIsBlueprintGenerated = TargetClass->HasAnyClassFlags(CLASS_CompiledFromBlueprint);

	TargetActorCompilableName = TargetClass->GetName();
	if (SetCompilableName != nullptr)
	{
		SetCompilableName(
			TargetActorCompilableName,
			bIsBlueprintGenerated ? ChanneldReplicatorGeneratorUtils::IsCompilableClassName(TargetActorCompilableName) : true
		);
	}
}

void FReplicatedActorDecorator::InitPropertiesAndRPCs()
{
	FPropertyDecoratorFactory& PropertyDecoratorFactory = FPropertyDecoratorFactory::Get();

	// Construct all property decorator
	for (TFieldIterator<FProperty> It(TargetClass, EFieldIteratorFlags::ExcludeSuper); It; ++It)
	{
		FProperty* Property = *It;
		if (Property->Owner != TargetClass)
		{
			continue;
		}
		if (Property->HasAnyPropertyFlags(CPF_Net))
		{
			TSharedPtr<FPropertyDecorator> PropertyDecoratorPtr = PropertyDecoratorFactory.GetPropertyDecorator(Property, this);
			if (PropertyDecoratorPtr.IsValid())
			{
				Properties.Emplace(PropertyDecoratorPtr);
			}
		}
	}
	int32 IllegalPropNameIndex = 0;
	for (TSharedPtr<FPropertyDecorator> PropertyDecoratorPtr : Properties)
	{
		PropertyDecoratorPtr->Init(
			[&IllegalPropNameIndex]()
			{
				return FString::Printf(TEXT("_IllegalNameProp_%d_"), ++IllegalPropNameIndex);
			}
		);
	}

	// Construct all rpc func decorator
	TArray<FName> FunctionNames;
	TargetClass->GenerateFunctionList(FunctionNames);
	for (const FName FuncName : FunctionNames)
	{
		UFunction* Func = TargetClass->FindFunctionByName(FuncName, EIncludeSuperFlag::ExcludeSuper);
		if (!Func->HasAnyFunctionFlags(FUNC_Net)) { continue; }

		RPCs.Add(MakeShareable(new FRPCDecorator(Func, this)));
	}
	int32 IllegalParamNameIndex = 0;
	for (TSharedPtr<FPropertyDecorator> RPCDecorator : RPCs)
	{
		RPCDecorator->Init(
			[&IllegalParamNameIndex]()
			{
				return FString::Printf(TEXT("_IllegalNameParam_%d_"), ++IllegalParamNameIndex);
			}
		);
	}
}

bool FReplicatedActorDecorator::IsBlueprintType()
{
	return bIsBlueprintGenerated;
}

bool FReplicatedActorDecorator::IsSingleton()
{
	return bSingleton;
}

bool FReplicatedActorDecorator::IsChanneldUEBuiltinType()
{
	return bChanneldUEBuiltinType;
}

bool FReplicatedActorDecorator::IsSkipGenReplicator()
{
	return bSkipGenReplicator;
}

bool FReplicatedActorDecorator::IsSkipGenChannelDataState()
{
	return bSkipGenChannelDataState;
}

void FReplicatedActorDecorator::SetModuleInfo(const FModuleInfo& InModuleBelongTo)
{
	ModuleBelongTo = InModuleBelongTo;
}

FString FReplicatedActorDecorator::GetActorName()
{
	return TargetActorCompilableName;
}

FString FReplicatedActorDecorator::GetActorPathName()
{
	return TargetClass->GetPathName();
}

FString FReplicatedActorDecorator::GetOriginActorName()
{
	return TargetClass->GetName();
}

FString FReplicatedActorDecorator::GetPackagePathName()
{
	return TargetClass->GetPackage()->GetPathName();
}

FString FReplicatedActorDecorator::GetActorCPPClassName()
{
	return TargetClass->GetPrefixCPP() + GetOriginActorName();
}

UFunction* FReplicatedActorDecorator::FindFunctionByName(const FName& FuncName)
{
	return TargetClass->FindFunctionByName(FuncName, EIncludeSuperFlag::ExcludeSuper);
}

FString FReplicatedActorDecorator::GetIncludeActorHeaderPath()
{
	return ModuleBelongTo.RelativeToModule;
}

FString FReplicatedActorDecorator::GetAdditionalIncludeFiles()
{
	TSet<FString> IncludeFileSet;
	for (auto PropDecorator : Properties)
	{
		IncludeFileSet.Append(PropDecorator->GetAdditionalIncludes());
	}
	for (auto RPCDecorator : RPCs)
	{
		IncludeFileSet.Append(RPCDecorator->GetAdditionalIncludes());
	}
	TArray<FString> IncludeFiles = IncludeFileSet.Array();
	if (TargetClass->IsChildOf(UActorComponent::StaticClass()))
	{
		IncludeFiles.Add(TEXT("Engine/PackageMapClient.h"));
	}
	FString Result;
	for (FString IncludeFile : IncludeFiles)
	{
		Result.Append(FString::Printf(TEXT("#include \"%s\"\n"), *IncludeFile));
	}
	return Result;
}

FString FReplicatedActorDecorator::GetReplicatorClassName(bool WithPrefix /* = true */)
{
	FString ClassName = TEXT("Channeld") + GetActorName() + TEXT("Replicator");
	return WithPrefix ? TEXT("F") + ClassName : ClassName;
}

FString FReplicatedActorDecorator::GetCode_IndirectlyAccessiblePropertyPtrDeclarations()
{
	FString Result;
	for (TSharedPtr<FPropertyDecorator> Property : Properties)
	{
		if (!Property->IsDirectlyAccessible())
		{
			Result += Property->GetDeclaration_PropertyPtr() + TEXT(";\n");
		}
	}
	return Result;
}

FString FReplicatedActorDecorator::GetCode_AssignPropertyPointers()
{
	FString Result;
	for (TSharedPtr<FPropertyDecorator> Property : Properties)
	{
		if (!Property->IsDirectlyAccessible())
		{
			Result += FString::Printf(
				TEXT("{ %s; }\n"),
				*Property->GetCode_AssignPropPointer(
					FString::Printf(TEXT("%s.Get()"), *InstanceRefName),
					Property->GetPointerName()
				)
			);
		}
	}
	return Result;
}


FString FReplicatedActorDecorator::GetProtoPackageName()
{
	if (!IsBlueprintType() && IsChanneldUEBuiltinType())
	{
		// If the target actor class is build-in channeld class, we should use the ChanneldUE proto namespace.
		// Actually, ChanneldUE doesn't cover all the build-in engine replicated actor classes like 'UTimelineComponent',
		return GenManager_ChanneldUEBuildInProtoPackageName;
	}
	// return GetActorName().ToLower() + "pb";
	return ProtoPackageName;
}

FString FReplicatedActorDecorator::GetProtoPackagePathGo(const FString& ChannelDataPackageName)
{
	const FString PackageName = GetProtoPackageName();
	if (PackageName == ChannelDataPackageName)
	{
		return TEXT("");
	}
	return PackageName + ".";
}

FString FReplicatedActorDecorator::GetProtoNamespace()
{
	return GetProtoPackageName();
}

FString FReplicatedActorDecorator::GetProtoDefinitionsFileClearName()
{
	// Using lower case actor name as the proto file name.
	return GetActorName().ToLower();
}

FString FReplicatedActorDecorator::GetProtoDefinitionsFileName()
{
	return GetProtoDefinitionsFileClearName() + CodeGen_ProtoFileExtension;
}

FString FReplicatedActorDecorator::GetGoPackageImportPath()
{
	return GoPackageImportPath;
}

FString FReplicatedActorDecorator::GetProtoStateMessageType()
{
	// The proto state message of GameStateBase in unreal_common.proto is 'GameStateBase',
	// other proto state messages are named as 'ActorNameState'.
	if (TargetClass == AGameStateBase::StaticClass())
	{
		return TEXT("GameStateBase");
	}
	if (TargetClass == APlayerState::StaticClass())
	{
		return TEXT("PlayerState");
	}
	return GetActorName() + TEXT("State");
}

FString FReplicatedActorDecorator::GetCode_AllPropertiesSetDeltaState(const FString& FullStateName, const FString& DeltaStateName)
{
	if (Properties.Num() == 0)
	{
		return TEXT("");
	}
	FString SetDeltaStateCodeBuilder;
	for (const TSharedPtr<FPropertyDecorator> Property : Properties)
	{
		SetDeltaStateCodeBuilder.Append(Property->GetCode_SetDeltaState(InstanceRefName, FullStateName, DeltaStateName));
	}
	return SetDeltaStateCodeBuilder;
}

FString FReplicatedActorDecorator::GetCode_AllPropertiesOnStateChange(const FString& NewStateName)
{
	if (Properties.Num() == 0)
	{
		return TEXT("");
	}
	FString OnChangeStateCodeBuilder;
	for (const TSharedPtr<FPropertyDecorator> Property : Properties)
	{
		OnChangeStateCodeBuilder.Append(Property->GetCode_OnStateChange(InstanceRefName, NewStateName, true));
	}
	return OnChangeStateCodeBuilder;
}

FString FReplicatedActorDecorator::GetDefinition_ProtoStateMessage()
{
	FString FieldDefinitions;

	int32 Offset = 1;
	if (TargetClass->IsChildOf(UActorComponent::StaticClass()))
	{
		FieldDefinitions.Append(TEXT("bool removed = 1;"));
		Offset = 2;
	}
	for (int32 i = 0; i < Properties.Num(); i++)
	{
		const TSharedPtr<FPropertyDecorator> Property = Properties[i];
		FieldDefinitions += Property->GetDefinition_ProtoField(i + Offset) + TEXT(";\n");
	}
	FStringFormatNamedArguments FormatArgs;
	FormatArgs.Add(TEXT("Declare_StateMessageType"), GetProtoStateMessageType());
	FormatArgs.Add(TEXT("Declare_ProtoFields"), FieldDefinitions);
	FormatArgs.Add(TEXT("Declare_SubProtoFields"), TEXT(""));

	return FString::Format(CodeGen_ProtoStateMessageTemplate, FormatArgs);
}

int32 FReplicatedActorDecorator::GetRPCNum()
{
	return RPCs.Num();
}

FString FReplicatedActorDecorator::GetCode_SerializeFunctionParams()
{
	FString SerializeParamCodes;
	bool bIsFirst = true;
	for (int32 i = 0; i < RPCs.Num(); i++)
	{
		const TSharedPtr<FRPCDecorator> RPC = RPCs[i];
		if (!bIsFirst)
		{
			SerializeParamCodes.Append(TEXT("else "));
		}
		SerializeParamCodes.Append(RPC->GetCode_SerializeFunctionParams());
		bIsFirst = false;
	}
	return SerializeParamCodes;
}

FString FReplicatedActorDecorator::GetCode_DeserializeFunctionParams()
{
	FString DeserializeParamCodes;
	bool bIsFirst = true;
	for (int32 i = 0; i < RPCs.Num(); i++)
	{
		const TSharedPtr<FRPCDecorator> RPC = RPCs[i];
		if (!bIsFirst)
		{
			DeserializeParamCodes.Append(TEXT("else "));
		}
		DeserializeParamCodes.Append(RPC->GetCode_DeserializeFunctionParams());
		bIsFirst = false;
	}
	return DeserializeParamCodes;
}

FString FReplicatedActorDecorator::GetDeclaration_RPCParamStructs()
{
	FString RPCParamStructsDeclarations = TEXT("");

	if (GetRPCNum() > 0)
	{
		for (int32 i = 0; i < RPCs.Num(); i++)
		{
			const TSharedPtr<FRPCDecorator> RPC = RPCs[i];
			RPCParamStructsDeclarations.Append(RPC->GetDeclaration_PropPtrGroupStruct());
		}
	}
	return RPCParamStructsDeclarations;
}

FString FReplicatedActorDecorator::GetDefinition_RPCParamProtoDefinitions()
{
	FString RPCParamMessages;
	for (int32 i = 0; i < RPCs.Num(); i++)
	{
		const TSharedPtr<FRPCDecorator> RPC = RPCs[i];
		FStringFormatNamedArguments FormatArgs;
		FormatArgs.Add(TEXT("Declare_StateMessageType"), RPC->GetProtoStateMessageType());
		FormatArgs.Add(TEXT("Declare_ProtoFields"), RPC->GetDeclaration_ProtoFields());
		FormatArgs.Add(TEXT("Declare_SubProtoFields"), TEXT(""));

		RPCParamMessages.Append(FString::Format(CodeGen_ProtoStateMessageTemplate, FormatArgs));
	}
	return RPCParamMessages;
}

FString FReplicatedActorDecorator::GetInstanceRefName() const
{
	return InstanceRefName;
}

void FReplicatedActorDecorator::SetInstanceRefName(const FString& InInstanceRefName)
{
	this->InstanceRefName = InInstanceRefName;
}

FString FReplicatedActorDecorator::GetCode_GetWorldRef()
{
	return InstanceRefName + TEXT("->GetWorld()");
}

FString FReplicatedActorDecorator::GetCode_OverrideGetNetGUID()
{
	FStringFormatNamedArguments FormatArgs;
	FormatArgs.Add(TEXT("Declare_ReplicatorClassName"), GetReplicatorClassName());
	// Only GameState will return Channeld::GameStateNetId.
	if (TargetClass->IsChildOf(AGameStateBase::StaticClass()))
	{
		return FString::Format(GameState_GetNetGUIDTemplate, FormatArgs);
	}
	else if (TargetClass->IsChildOf(UActorComponent::StaticClass()))
	{
		FormatArgs.Add(TEXT("Ref_TargetInstanceRef"), InstanceRefName);
		return FString::Format(ActorComp_GetNetGUIDTemplate, FormatArgs);
	}
	return TEXT("");
}

void FReplicatedActorDecorator::SetConstClassPathFNameVarName(const FString& VarName)
{
	VariableName_ConstClassPathFName = VarName;
}

FString FReplicatedActorDecorator::GetDefinition_ChannelDataFieldNameProto()
{
	FString ProtoStateMessageType = GetProtoStateMessageType();
	ProtoStateMessageType[0] = tolower(ProtoStateMessageType[0]);
	return ProtoStateMessageType + (IsSingleton() ? TEXT("") : TEXT("s"));
}

FString FReplicatedActorDecorator::GetDefinition_ChannelDataFieldNameGo()
{
	FString ChannelDataFieldName = GetDefinition_ChannelDataFieldNameProto();
	ChannelDataFieldName[0] = toupper(ChannelDataFieldName[0]);
	return ChannelDataFieldName;
}

FString FReplicatedActorDecorator::GetDefinition_ChannelDataFieldNameCpp()
{
	return GetDefinition_ChannelDataFieldNameProto().ToLower();
}

FString FReplicatedActorDecorator::GetCode_ConstPathFNameVarDecl()
{
	return FString::Printf(TEXT("const FName %s = FName(\"%s\");"), *VariableName_ConstClassPathFName, *TargetClass->GetPathName());
}

FString FReplicatedActorDecorator::GetCode_ChannelDataProcessor_IsTargetClass()
{
	if (IsBlueprintType())
	{
		return FString::Printf(
			TEXT("ChanneldReplication::FindReplicatorStateInProto(TargetClass)->TargetClassPathFName == %s"),
			*VariableName_ConstClassPathFName
		);
	}
	else
	{
		return FString::Printf(
			TEXT("TargetClass == %s::StaticClass()"),
			*GetActorCPPClassName()
		);
	}
}

FString FReplicatedActorDecorator::GetDeclaration_ChanneldDataProcessor_RemovedStata()
{
	if (TargetClass == AActor::StaticClass() || TargetClass->IsChildOf(UActorComponent::StaticClass()))
	{
		return FString::Printf(TEXT("TUniquePtr<%s::%s> Removed%s;"), *GetProtoNamespace(), *GetProtoStateMessageType(), *GetProtoStateMessageType());
	}
	else
	{
		return TEXT("");
	}
}

FString FReplicatedActorDecorator::GetCode_ChanneldDataProcessor_InitRemovedState()
{
	if (TargetClass == AActor::StaticClass() || TargetClass->IsChildOf(UActorComponent::StaticClass()))
	{
		return FString::Printf(TEXT("Removed%s = MakeUnique<%s::%s>();\nRemoved%s->set_removed(true);\n"), *GetProtoStateMessageType(), *GetProtoNamespace(), *GetProtoStateMessageType(), *GetProtoStateMessageType());
	}
	return TEXT("");
}

FString FReplicatedActorDecorator::GetCode_ChannelDataProcessor_Merge(const TArray<TSharedPtr<FReplicatedActorDecorator>>& ActorChildren)
{
	FStringFormatNamedArguments FormatArgs;
	FormatArgs.Add(TEXT("Definition_ChannelDataFieldName"), GetDefinition_ChannelDataFieldNameCpp());
	if (IsSingleton())
	{
		return FString::Format(ActorDecor_ChannelDataProcessorMerge_Singleton, FormatArgs);
	}
	else
	{
		FString Code_MergeLoopInner;
		if (TargetClass == AActor::StaticClass())
		{
			FString Code_MergeEraseInner;
			for (const TSharedPtr<FReplicatedActorDecorator> ChildrenActor : ActorChildren)
			{
				// Singleton actors are not permanently removed from ChannelData.
				if (ChildrenActor->IsSingleton())
				{
					continue;;
				}
				Code_MergeEraseInner.Append(
					FString::Printf(
						TEXT("Dst->mutable_%s()->erase(Pair.first);\n"),
						*ChildrenActor->GetDefinition_ChannelDataFieldNameCpp()
					)
				);
			}
			FormatArgs.Add(TEXT("Code_MergeEraseInner"), Code_MergeEraseInner);
			FormatArgs.Add(TEXT("Code_DoMerge"), FString::Format(ActorDecor_ChannelDataProcessorMerge_DoMarge, FormatArgs));
			Code_MergeLoopInner = FString::Format(ActorDecor_ChannelDataProcessorMerge_Erase, FormatArgs);
		}
		else if (TargetClass->IsChildOf(UActorComponent::StaticClass()))
		{
			FormatArgs.Add(
				TEXT("Code_MergeEraseInner"),
				FString::Printf(
					TEXT("Dst->mutable_%s()->erase(Pair.first);\n"),
					*GetDefinition_ChannelDataFieldNameCpp()
				)
			);
			FormatArgs.Add(TEXT("Code_DoMerge"), FString::Format(ActorDecor_ChannelDataProcessorMerge_DoMarge, FormatArgs));
			Code_MergeLoopInner = FString::Format(ActorDecor_ChannelDataProcessorMerge_Erase, FormatArgs);
		}
		else
		{
			Code_MergeLoopInner = FString::Format(ActorDecor_ChannelDataProcessorMerge_DoMarge, FormatArgs);
		}

		FormatArgs.Add(TEXT("Code_MergeLoopInner"), Code_MergeLoopInner);
		return FString::Format(ActorDecor_ChannelDataProcessorMergeLoop, FormatArgs);
	}
}

FString FReplicatedActorDecorator::GetCode_ChannelDataProcessor_GetStateFromChannelData(const FString& ChannelDataMessageName)
{
	FStringFormatNamedArguments FormatArgs;
	FormatArgs.Add(TEXT("Code_Condition"), GetCode_ChannelDataProcessor_IsTargetClass());
	FormatArgs.Add(TEXT("Declaration_ChannelDataMessage"), ChannelDataMessageName);
	FormatArgs.Add(TEXT("Definition_ChannelDataFieldName"), GetDefinition_ChannelDataFieldNameCpp());
	if (TargetClass == AActor::StaticClass() || TargetClass->IsChildOf(UActorComponent::StaticClass()))
	{
		return FString::Format(ActorDecor_GetStateFromChannelData_Removable, FormatArgs);
	}
	else if (IsSingleton())
	{
		return FString::Format(ActorDecor_GetStateFromChannelData_Singleton, FormatArgs);
	}
	return FString::Format(ActorDecor_GetStateFromChannelData, FormatArgs);
}

FString FReplicatedActorDecorator::GetCode_ChannelDataProcessor_SetStateToChannelData(const FString& ChannelDataMessageName)
{
	FStringFormatNamedArguments FormatArgs;
	FormatArgs.Add(TEXT("Code_Condition"), GetCode_ChannelDataProcessor_IsTargetClass());
	FormatArgs.Add(TEXT("Declaration_ChannelDataMessage"), ChannelDataMessageName);
	FormatArgs.Add(TEXT("Definition_ChannelDataFieldName"), GetDefinition_ChannelDataFieldNameCpp());
	FormatArgs.Add(TEXT("Definition_ProtoNamespace"), GetProtoNamespace());
	FormatArgs.Add(TEXT("Definition_ProtoStateMsgName"), GetProtoStateMessageType());
	if (TargetClass == AActor::StaticClass() || TargetClass->IsChildOf(UActorComponent::StaticClass()))
	{
		return FString::Format(ActorDecor_SetStateToChannelData_Removable, FormatArgs);
	}
	else if (IsSingleton())
	{
		return FString::Format(ActorDecor_SetStateToChannelData_Singleton, FormatArgs);
	}
	return FString::Format(ActorDecor_SetStateToChannelData, FormatArgs);
}

FString FReplicatedActorDecorator::GetCode_ChannelDataProtoFieldDefinition(const int32& Index)
{
	if (IsSingleton())
	{
		return FString::Printf(TEXT("%s.%s %s = %d;\n"), *GetProtoPackageName(), *GetProtoStateMessageType(), *GetDefinition_ChannelDataFieldNameProto(), Index);
	}
	else
	{
		return FString::Printf(TEXT("map<uint32, %s.%s> %s = %d;\n"), *GetProtoPackageName(), *GetProtoStateMessageType(), *GetDefinition_ChannelDataFieldNameProto(), Index);
	}
}
