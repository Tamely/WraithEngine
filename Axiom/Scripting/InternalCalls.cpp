#include "InternalCalls.h"

#include <Core/Log.h>
#include <Session/EditorCommand.h>

#if AXIOM_SCRIPTING_ENABLED
#include <Coral/String.hpp>
#include <Coral/Core.hpp>
#endif

namespace {
Axiom::EditorSession *s_Session = nullptr;
Axiom::SessionId s_SessionId{1};
Axiom::SessionUserId s_UserId{1};
} // namespace

namespace Axiom::InternalCalls {

void Bind(EditorSession &Session, SessionId Id, SessionUserId UserId) {
  s_Session = &Session;
  s_SessionId = Id;
  s_UserId = UserId;
}

// ---------------------------------------------------------------------------
// Internal call implementations
// These are called via Coral's function-pointer dispatch from managed C# code.
// Naming convention used in AddInternalCall:
//   "WraithEngine.<Class>+s_<Field>,WraithEngine.Managed"
// ---------------------------------------------------------------------------

#if AXIOM_SCRIPTING_ENABLED

void GameObject_GetName(Coral::String ObjectId, Coral::String *OutName) {
  if (!s_Session || !OutName) {
    if (OutName)
      *OutName = Coral::String{};
    return;
  }
  std::string id = ObjectId;
  const auto *Details = s_Session->FindObjectDetails(id);
  *OutName = Details ? Coral::String::New(Details->DisplayName)
                     : Coral::String::New(id);
}

void GameObject_GetTransform(Coral::String ObjectId,
                              EditorTransformDetails *OutTransform) {
  if (!s_Session || !OutTransform)
    return;
  std::string id = ObjectId;
  const auto *Details = s_Session->FindObjectDetails(id);
  if (Details && Details->Transform.has_value()) {
    *OutTransform = *Details->Transform;
  } else {
    *OutTransform = EditorTransformDetails{};
  }
}

void GameObject_SetTransform(Coral::String ObjectId,
                              const EditorTransformDetails *InTransform) {
  if (!s_Session || !InTransform)
    return;
  std::string id = ObjectId;
  CommandContext Ctx{s_SessionId, s_UserId};
  SetTransformCommand Cmd{.ObjectId = std::move(id),
                          .Location = InTransform->Location,
                          .RotationDegrees = InTransform->RotationDegrees,
                          .Scale = InTransform->Scale};
  s_Session->Submit(Ctx, EditorCommand{Cmd});
}

Coral::Bool32 GameObject_GetVisible(Coral::String ObjectId) {
  if (!s_Session)
    return 1u;
  std::string id = ObjectId;
  const auto *Details = s_Session->FindObjectDetails(id);
  return Details ? static_cast<Coral::Bool32>(Details->Visible ? 1u : 0u) : 1u;
}

#endif // AXIOM_SCRIPTING_ENABLED

} // namespace Axiom::InternalCalls
