#pragma once
#include "runtime/core/meta/reflection/reflection.h"
#include <string>
#include <vector>

namespace Piccolo
{
    REFLECTION_TYPE(Label)
    CLASS(Label, Fields)
    {
        REFLECTION_BODY(Label);

    public:
        std::string m_label; // not support unordered_set up to now!
    };
} // namespace Piccolo