#include "Window.h"

#include <utility>

namespace Axiom {
Window::Window(std::string Title, uint32_t Width, uint32_t Height)
    : m_Title(std::move(Title)), m_Width(Width), m_Height(Height) {}
} // namespace Axiom
