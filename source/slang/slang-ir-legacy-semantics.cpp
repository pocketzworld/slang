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
            rewriteDecoration(field->getKey(), isOutput);
            rewriteType(field->getFieldType(), isOutput);
        }
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
        IRInst* frontValue = nullptr;
        IRInst* backValue = nullptr;
        switch (basicType->getOp())
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
        IRInst* args[] = {newParam, frontValue, backValue};
        auto value = builder.emitIntrinsicInst(type, kIROp_Select, 3, args);
        param->replaceUsesWith(value);
        param->removeAndDeallocate();
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
