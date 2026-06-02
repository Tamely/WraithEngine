#include "Core/HeadlessWindow.h"

namespace Axiom {
HeadlessWindow::HeadlessWindow(const std::string &Title, uint32_t Width,
                               uint32_t Height)
    : Window(Title, Width, Height) {}

void HeadlessWindow::PollEvents() {}

bool HeadlessWindow::IsKeyPressed(int Key) const {
  (void)Key;
  return false;
}

bool HeadlessWindow::IsMouseButtonPressed(int Button) const {
  (void)Button;
  return false;
}

void HeadlessWindow::GetCursorPosition(double &X, double &Y) const {
  X = 0.0;
  Y = 0.0;
}

void HeadlessWindow::SetCursorMode(CursorMode Mode) { m_CursorMode = Mode; }

CursorMode HeadlessWindow::GetCursorMode() const { return m_CursorMode; }

bool HeadlessWindow::ShouldClose() const { return m_ShouldClose; }

bool HeadlessWindow::IsMinimized() const { return false; }

void HeadlessWindow::RequestClose() { m_ShouldClose = true; }

void *HeadlessWindow::GetNativeHandle() const { return nullptr; }

bool HeadlessWindow::SupportsPresentationBackend(
    PresentationBackendType Backend) const {
  (void)Backend;
  return false;
}

PresentationSurfaceResult
HeadlessWindow::CreatePresentationSurface(PresentationBackendType Backend,
                                          void *Instance, void *Surface) const {
  (void)Backend;
  (void)Instance;
  (void)Surface;
  return PresentationSurfaceResult::InitializationFailed;
}
} // namespace Axiom
