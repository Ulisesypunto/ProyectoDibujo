#include "tools/BuiltinTools.h"
#include "tools/BrushTool.h"
#include "tools/EraserTool.h"
#include "tools/EyedropperTool.h"
#include "tools/FillTool.h"
#include "tools/LassoTool.h"
#include "tools/ToolManager.h"
#include "tools/TransformTool.h"

void registerBuiltinTools(ToolManager& manager) {
    manager.registerTool(std::make_unique<BrushTool>());
    manager.registerTool(std::make_unique<EraserTool>());
    manager.registerTool(std::make_unique<FillTool>());
    manager.registerTool(std::make_unique<EyedropperTool>());
    manager.registerTool(std::make_unique<LassoTool>());
    manager.registerTool(std::make_unique<TransformTool>());
}