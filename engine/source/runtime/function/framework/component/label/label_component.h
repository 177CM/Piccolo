#pragma once

#include "runtime/function/framework/component/component.h"
#include "runtime/resource/res_type/components/label.h"

#include <string>
#include <vector>

namespace Piccolo
{

    REFLECTION_TYPE(LabelComponent)
    CLASS(LabelComponent : public Component, Fields)
    {
        REFLECTION_BODY(LabelComponent)
    public:
        LabelComponent() {};

        void addLabel(std::string label);
        void addLabels(std::vector<std::string> labels);
        bool deleteLabel(std::string label);

        void tick(float delta_time) override;

    private:
        std::vector<Label> m_labels;
    };
} // namespace Piccolo
