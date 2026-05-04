#pragma once

#include <string>

namespace Axiom {
class Layer {
public:
  explicit Layer(std::string Name = "Layer");
  virtual ~Layer() = default;

  virtual void OnAttach() {}
  virtual void OnDetach() {}
  virtual void OnUpdate() {}
  virtual void OnImGuiRender() {}

  const std::string &GetName() const { return m_Name; }

private:
  std::string m_Name;
};
} // namespace Axiom
