#include "Core/Layer.h"

#include <utility>

namespace Axiom {
Layer::Layer(std::string Name) : m_Name(std::move(Name)) {}
} // namespace Axiom
