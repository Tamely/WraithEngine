#include "InternalCalls.h"

#include <Core/Log.h>
#include <Session/EditorCommand.h>

namespace {
Axiom::EditorSession *s_Session = nullptr;
Axiom::SessionId s_SessionId{1};
Axiom::SessionUserId s_UserId{1};
bool s_IsRestricted = false;
} // namespace

namespace Axiom::InternalCalls {

void Bind(EditorSession &Session, SessionId Id, SessionUserId UserId,
          bool IsRestricted) {
  s_Session = &Session;
  s_SessionId = Id;
  s_UserId = UserId;
  s_IsRestricted = IsRestricted;
}

void GameObject_GetName(Coral::String ObjectId, Coral::String *OutName) {
  if (!s_Session || !OutName) {
    if (OutName) {
      *OutName = Coral::String{};
    }
    return;
  }
  std::string Id = ObjectId;
  const auto *Details = s_Session->FindObjectDetails(Id);
  *OutName = Details ? Coral::String::New(Details->DisplayName)
                     : Coral::String::New(Id);
}

void GameObject_GetTransform(Coral::String ObjectId,
                             EditorTransformDetails *OutTransform) {
  if (!s_Session || !OutTransform) {
    return;
  }
  std::string Id = ObjectId;
  const auto *Details = s_Session->FindObjectDetails(Id);
  if (Details && Details->Transform.has_value()) {
    *OutTransform = *Details->Transform;
  } else {
    *OutTransform = EditorTransformDetails{};
  }
}

void GameObject_SetTransform(Coral::String ObjectId,
                             const EditorTransformDetails *InTransform) {
  if (!s_Session || !InTransform) {
    return;
  }
  std::string Id = ObjectId;
  CommandContext Ctx{s_SessionId, s_UserId, 0, 0.0f, true};
  SetTransformCommand Cmd{.ObjectId = std::move(Id),
                          .Location = InTransform->Location,
                          .RotationDegrees = InTransform->RotationDegrees,
                          .Scale = InTransform->Scale};
  s_Session->Submit(Ctx, EditorCommand{Cmd});
}

Coral::Bool32 GameObject_GetVisible(Coral::String ObjectId) {
  if (!s_Session) {
    return 1u;
  }
  std::string Id = ObjectId;
  const auto *Details = s_Session->FindObjectDetails(Id);
  return Details ? static_cast<Coral::Bool32>(Details->Visible ? 1u : 0u) : 1u;
}

Coral::Bool32 ScriptSecurity_IsRestricted() {
  return static_cast<Coral::Bool32>(s_IsRestricted ? 1u : 0u);
}

} // namespace Axiom::InternalCalls
