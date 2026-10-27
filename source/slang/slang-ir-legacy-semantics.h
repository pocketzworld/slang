#pragma once

#include "../core/slang-basic.h"
#include "slang-profile.h"

namespace Slang
{
struct IRModule;

/// `-legacy-semantics` (and `-Gec`): the Direct3D 9 semantics that FXC's `/Gec` maps to
/// system values. `POSITION` on a vertex output or fragment input and `VPOS` on a fragment
/// input are `SV_Position`, `VFACE` on a fragment input is `SV_IsFrontFace`, and `COLORn` and
/// `DEPTH` on a fragment output are `SV_Target[n]` and `SV_Depth`. Every other semantic (a
/// vertex input `POSITION`, a varying `COLOR`) is left as it is.
///
/// Returns true, with the new name (its index appended), when `semanticName` is mapped.
bool getLegacySemanticReplacement(
    Stage stage,
    bool isOutput,
    UnownedStringSlice semanticName,
    String& outName);

/// Rewrite the semantics of every entry point's varying parameters and results in `module`
/// (see getLegacySemanticReplacement); a `float` `VFACE` input becomes a `bool`
/// `SV_IsFrontFace` read as `1` (front) or `-1` (back), as Direct3D 9 gave it.
void applyLegacySemantics(IRModule* module);
} // namespace Slang
