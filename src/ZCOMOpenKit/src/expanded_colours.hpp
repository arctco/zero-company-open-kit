#pragma once

namespace ZCOMOpenKit::ExpandedColours
{
    // Called from the same game/update thread as the rest of the module. The
    // catalogue hook itself is allocation-free and never logs.
    // include_extra_colours: offer the whole locked palette worth having (135
    // colours) rather than only upstream's curated forty.
    auto initialize(bool include_extra_colours) -> bool;
    auto update() -> void;
    auto shutdown() -> void;
}
