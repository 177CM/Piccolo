#include "runtime/function/framework/component/label/label_component.h"

namespace Piccolo
{
    // no need to tick
    void LabelComponent::tick(float delta_time) {}

    void LabelComponent::addLabel(std::string label)
    {
        Label temp;
        temp.m_label = label;
        m_labels.push_back(temp);
    }

    void LabelComponent::addLabels(std::vector<std::string> labels)
    {
        for (auto label : labels)
        {
            addLabel(label);
        }
    }

    bool LabelComponent::deleteLabel(std::string label)
    {
        for (auto iter = m_labels.begin(); iter != m_labels.end(); iter++)
        {
            if ((*iter).m_label == label)
            {
                m_labels.erase(iter);
                return true;
            }
        }
        return false;
    }
} // namespace Piccolo
