#pragma once

#include <Session/EditorSession.h>

namespace Axiom {
class EditorSessionValidationModule {
public:
  explicit EditorSessionValidationModule(EditorSession &Session);

  bool ValidateCommand(const QueuedEditorCommand &QueuedCommand,
                       std::string &FailureReason) const;

private:
  EditorSession &m_Session;
};
} // namespace Axiom
