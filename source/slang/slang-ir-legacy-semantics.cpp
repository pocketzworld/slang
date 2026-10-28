#include "slang-ir-legacy-semantics.h"

#include "../core/slang-char-util.h"
#include "slang-ir-insts.h"
#include "slang-ir.h"

namespace Slang
{
bool getLegacySemanticReplacement(
    Stage stage,
    bool isOutput,
    UnownedStringSlice semanticName,
    String& outName)
{
    // Split the trailing index (`COLOR1` is `COLOR`, 1).
    auto end = semanticName.end();
    while (end != semanticName.begin() && CharUtil::isDigit(end[-1]))
        --end;
    UnownedStringSlice baseName(semanticName.begin(), end);
    UnownedStringSlice indexText(end, semanticName.end());
    bool indexIsZero = true;
    for (auto c : indexText)
        if (c != '0')
            indexIsZero = false;

    char const* replacement = nullptr;
    bool keepIndex = false;
    if (stage == Stage::Vertex && isOutput)
    {
        if (baseName.caseInsensitiveEquals(toSlice("POSITION")) && indexIsZero)
            replacement = "SV_Position";
    }
    else if (stage == Stage::Fragment && !isOutput)
    {
        if ((baseName.caseInsensitiveEquals(toSlice("POSITION")) ||
             baseName.caseInsensitiveEquals(toSlice("VPOS"))) &&
            indexIsZero)
            replacement = "SV_Position";
        else if (baseName.caseInsensitiveEquals(toSlice("VFACE")) && indexIsZero)
            replacement = "SV_IsFrontFace";
    }
    else if (stage == Stage::Fragment && isOutput)
    {
        if (baseName.caseInsensitiveEquals(toSlice("COLOR")))
        {
            replacement = "SV_Target";
            keepIndex = true;
        }
        else if (baseName.caseInsensitiveEquals(toSlice("DEPTH")) && indexIsZero)
            replacement = "SV_Depth";
    }
    if (!replacement)
        return false;

    StringBuilder sb;
    sb << replacement;
    if (keepIndex)
        sb << indexText;
    outName = sb.produceString();
    return true;
}

namespace
{
struct LegacySemanticsContext
{
    IRModule* module;
    Stage stage = Stage::Unknown;
    HashSet<IRInst*> visitedTypes;
    // VFACE fields rewritten to SV_IsFrontFace, and the struct types handed in their place.
    HashSet<IRInst*> frontFaceKeys;
    Dictionary<IRStructType*, IRType*> frontFaceInputTypes;

    // Returns the new name when `inst`'s semantic was rewritten.
    bool rewriteDecoration(IRInst* inst, bool isOutput, String* outNewName = nullptr)
    {
        auto decoration = inst->findDecoration<IRSemanticDecoration>();
        if (!decoration)
            return false;
        String newName;
        if (!getLegacySemanticReplacement(stage, isOutput, decoration->getSemanticName(), newName))
            return false;
        IRBuilder builder(module);
        builder.addSemanticDecoration(
            inst,
            newName.getUnownedSlice(),
            decoration->getSemanticIndex());
        decoration->removeAndDeallocate();
        if (outNewName)
            *outNewName = newName;
        return true;
    }

    void rewriteType(IRType* type, bool isOutput)
    {
        for (;;)
        {
            if (auto arrayType = as<IRArrayTypeBase>(type))
                type = arrayType->getElementType();
            else
                break;
        }
        auto structType = as<IRStructType>(type);
        if (!structType || !visitedTypes.add(structType))
            return;
        for (auto field : structType->getFields())
        {
            String newName;
            if (rewriteDecoration(field->getKey(), isOutput, &newName) &&
                newName == toSlice("SV_IsFrontFace"))
                frontFaceKeys.add(field->getKey());
            rewriteType(field->getFieldType(), isOutput);
        }
    }

    // VFACE's value of the system value's bool: +1 for a front face, -1 for a back one.
    IRInst* frontFaceValue(IRBuilder& builder, IRType* type, IRInst* isFront)
    {
        IRInst* frontValue = nullptr;
        IRInst* backValue = nullptr;
        switch (type->getOp())
        {
        case kIROp_HalfType:
        case kIROp_FloatType:
        case kIROp_DoubleType:
            frontValue = builder.getFloatValue(type, 1.0);
            backValue = builder.getFloatValue(type, -1.0);
            break;
        default:
            frontValue = builder.getIntValue(type, 1);
            backValue = builder.getIntValue(type, -1);
            break;
        }
        IRInst* args[] = {isFront, frontValue, backValue};
        return builder.emitIntrinsicInst(type, kIROp_Select, 3, args);
    }

    // A `float face : VFACE` input is +1 for a front face and -1 for a back one; the system
    // value is a bool.
    void convertFrontFaceParam(IRFunc* func, IRParam* param)
    {
        auto type = param->getDataType();
        auto basicType = as<IRBasicType>(type);
        if (!basicType || basicType->getOp() == kIROp_BoolType)
            return;
        auto firstBlock = func->getFirstBlock();
        if (!firstBlock)
            return;

        IRBuilder builder(module);
        auto newParam = builder.createParam(builder.getBoolType());
        newParam->insertBefore(param);
        param->transferDecorationsTo(newParam);

        builder.setInsertBefore(firstBlock->getFirstOrdinaryInst());
        auto value = frontFaceValue(builder, type, newParam);
        param->replaceUsesWith(value);
        param->removeAndDeallocate();
        fixUpFuncType(func);
    }

    // A struct of fragment inputs with a `float face : VFACE` field somewhere in it, as the
    // entry point is handed it: the same fields with that one a bool (the system value), or
    // null when it has none. A field keeps its key, and so its layout and semantic.
    IRType* getFrontFaceInputType(IRType* type)
    {
        auto structType = as<IRStructType>(type);
        if (!structType)
            return nullptr;
        if (auto found = frontFaceInputTypes.tryGetValue(structType))
            return *found;
        frontFaceInputTypes[structType] = nullptr;
        bool changed = false;
        List<IRType*> fieldTypes;
        for (auto field : structType->getFields())
        {
            auto fieldType = field->getFieldType();
            IRType* newType = nullptr;
            auto basicType = as<IRBasicType>(fieldType);
            if (frontFaceKeys.contains(field->getKey()) && basicType &&
                basicType->getOp() != kIROp_BoolType)
            {
                IRBuilder builder(module);
                newType = builder.getBoolType();
            }
            else
                newType = getFrontFaceInputType(fieldType);
            changed = changed || newType;
            fieldTypes.add(newType ? newType : fieldType);
        }
        if (!changed)
            return nullptr;

        IRBuilder builder(module);
        builder.setInsertBefore(structType);
        auto newStruct = builder.createStructType();
        if (auto nameHint = structType->findDecoration<IRNameHintDecoration>())
            builder.addNameHintDecoration(newStruct, nameHint->getName());
        Index i = 0;
        for (auto field : structType->getFields())
            builder.createStructField(newStruct, field->getKey(), fieldTypes[i++]);
        frontFaceInputTypes[structType] = newStruct;
        return newStruct;
    }

    // The value of the source's struct made from the one the entry point is handed
    // (getFrontFaceInputType): a VFACE field reads +1 for a front face and -1 for a back one.
    IRInst* makeFrontFaceValue(IRBuilder& builder, IRStructType* type, IRInst* handed)
    {
        List<IRInst*> fields;
        for (auto field : type->getFields())
        {
            auto fieldType = field->getFieldType();
            auto handedType = getFrontFaceInputType(fieldType);
            auto basicType = as<IRBasicType>(fieldType);
            if (frontFaceKeys.contains(field->getKey()) && basicType &&
                basicType->getOp() != kIROp_BoolType)
            {
                auto isFront =
                    builder.emitFieldExtract(builder.getBoolType(), handed, field->getKey());
                fields.add(frontFaceValue(builder, fieldType, isFront));
            }
            else if (handedType)
            {
                auto inner = builder.emitFieldExtract(handedType, handed, field->getKey());
                fields.add(makeFrontFaceValue(builder, as<IRStructType>(fieldType), inner));
            }
            else
                fields.add(builder.emitFieldExtract(fieldType, handed, field->getKey()));
        }
        return builder.emitMakeStruct(type, fields);
    }

    // A struct input whose VFACE field is a float: handed as the system value's bool, and the
    // source's struct made of it where the function starts.
    void convertFrontFaceStructParam(IRFunc* func, IRParam* param)
    {
        auto structType = as<IRStructType>(param->getDataType());
        auto handedType = getFrontFaceInputType(structType);
        auto firstBlock = func->getFirstBlock();
        if (!handedType || !firstBlock)
            return;
        IRBuilder builder(module);
        auto newParam = builder.createParam(handedType);
        newParam->insertBefore(param);
        param->transferDecorationsTo(newParam);
        builder.setInsertBefore(firstBlock->getFirstOrdinaryInst());
        auto value = makeFrontFaceValue(builder, structType, newParam);
        param->replaceUsesWith(value);
        param->removeAndDeallocate();
        fixUpFuncType(func);
    }

    // Where a fragment input reads SV_Position: the parameter, and the field of it when it is
    // one of a struct parameter's.
    struct PositionInput
    {
        IRParam* param;
        IRStructKey* key;
        IRType* type;
    };

    void findPositionInputs(IRFunc* func, List<PositionInput>& outInputs)
    {
        auto isPosition = [](IRInst* inst)
        {
            auto semantic = inst->findDecoration<IRSemanticDecoration>();
            return semantic &&
                   semantic->getSemanticName().caseInsensitiveEquals(toSlice("SV_Position"));
        };
        for (auto param : func->getParams())
        {
            auto type = param->getDataType();
            if (as<IROutParamTypeBase>(type) || as<IRBorrowInOutParamType>(type))
                continue;
            if (isPosition(param))
                outInputs.add({param, nullptr, type});
            else if (auto structType = as<IRStructType>(type))
                for (auto field : structType->getFields())
                    if (isPosition(field->getKey()))
                        outInputs.add({param, field->getKey(), field->getFieldType()});
        }
    }

    // Two fragment inputs of SV_Position (a struct's POSITION or SV_Position, and a VPOS
    // parameter beside it, as Direct3D 9 shaders have them) are one system value, which Metal
    // takes once: each later parameter reads the first one's value (the leading elements of
    // it for a shorter vector) and is removed.
    void mergePositionInputs(IRFunc* func)
    {
        List<PositionInput> inputs;
        findPositionInputs(func, inputs);
        if (inputs.getCount() < 2)
            return;
        auto firstBlock = func->getFirstBlock();
        if (!firstBlock)
            return;
        auto& kept = inputs[0];
        auto keptVector = as<IRVectorType>(kept.type);
        bool changed = false;
        for (Index i = 1; i < inputs.getCount(); i++)
        {
            auto& other = inputs[i];
            if (other.key || other.param == kept.param)
                continue;
            IRBuilder builder(module);
            builder.setInsertBefore(firstBlock->getFirstOrdinaryInst());
            IRInst* value =
                kept.key ? builder.emitFieldExtract(kept.type, kept.param, kept.key) : kept.param;
            if (other.type != kept.type)
            {
                auto otherVector = as<IRVectorType>(other.type);
                auto keptCount = keptVector ? as<IRIntLit>(keptVector->getElementCount()) : nullptr;
                auto otherCount =
                    otherVector ? as<IRIntLit>(otherVector->getElementCount()) : nullptr;
                if (!keptCount || !otherCount || otherCount->getValue() > keptCount->getValue() ||
                    otherVector->getElementType() != keptVector->getElementType())
                    continue;
                uint32_t indices[] = {0, 1, 2, 3};
                value =
                    builder.emitSwizzle(other.type, value, UInt(otherCount->getValue()), indices);
            }
            other.param->replaceUsesWith(value);
            other.param->removeAndDeallocate();
            changed = true;
        }
        if (changed)
            fixUpFuncType(func);
    }

    void processEntryPoint(IRFunc* func, IREntryPointDecoration* entryPointDecoration)
    {
        stage = entryPointDecoration->getProfile().getStage();
        if (stage != Stage::Vertex && stage != Stage::Fragment)
            return;
        visitedTypes.clear();

        // The result.
        rewriteDecoration(func, true);
        rewriteType(func->getResultType(), true);

        // The parameters: `out` ones are outputs, plain ones inputs (an `inout` is both, and
        // is left alone).
        List<IRParam*> frontFaceParams;
        for (auto param : func->getParams())
        {
            auto type = param->getDataType();
            if (as<IRBorrowInOutParamType>(type))
                continue;
            bool isOutput = false;
            if (auto outType = as<IROutParamType>(type))
            {
                isOutput = true;
                type = outType->getValueType();
            }
            String newName;
            if (rewriteDecoration(param, isOutput, &newName) &&
                newName == toSlice("SV_IsFrontFace"))
                frontFaceParams.add(param);
            rewriteType(type, isOutput);
        }
        for (auto param : frontFaceParams)
            convertFrontFaceParam(func, param);
        if (stage != Stage::Fragment)
            return;

        // Struct inputs with a VFACE field.
        List<IRParam*> structParams;
        for (auto param : func->getParams())
            if (getFrontFaceInputType(param->getDataType()))
                structParams.add(param);
        for (auto param : structParams)
            convertFrontFaceStructParam(func, param);

        mergePositionInputs(func);
    }
};
} // namespace

void applyLegacySemantics(IRModule* module)
{
    LegacySemanticsContext context;
    context.module = module;
    for (auto inst : module->getGlobalInsts())
    {
        auto func = as<IRFunc>(inst);
        if (!func)
            continue;
        if (auto entryPointDecoration = func->findDecoration<IREntryPointDecoration>())
            context.processEntryPoint(func, entryPointDecoration);
    }
}
} // namespace Slang
