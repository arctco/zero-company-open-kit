#pragma once

namespace ZCOMOpenKit::HelmetRenderFit
{
    // Lifecycle entry points are called from the same game/update thread used
    // by the rest of the native mod. The render hooks themselves remain
    // allocation-free, lock-free, and logging-free.
    auto initialize() -> bool;
    auto update() -> void;
    auto shutdown() -> void;
}
