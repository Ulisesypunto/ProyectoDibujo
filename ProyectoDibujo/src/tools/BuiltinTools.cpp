#include "tools/BuiltinTools.h"
#include "tools/BrushTool.h"
#include "tools/EraserTool.h"
#include "tools/EyedropperTool.h"
#include "tools/ToolManager.h"

void registerBuiltinTools(ToolManager& manager) {
    manager.registerTool(std::make_unique<BrushTool>());
    manager.registerTool(std::make_unique<EraserTool>());
    manager.registerTool(std::make_unique<EyedropperTool>());
}
