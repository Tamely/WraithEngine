#pragma once

#include <Session/EditorSession.h>
#include <Session/SessionTypes.h>

#if AXIOM_SCRIPTING_ENABLED
#include <Coral/String.hpp>
#include <Coral/Core.hpp>
#endif

namespace Axiom::InternalCalls {

/// Store the session pointer, credentials, and trust profile used by all
/// internal call implementations. Must be called before UploadInternalCalls().
void Bind(EditorSession &Session, SessionId Id, SessionUserId UserId,
          bool IsRestricted);

#if AXIOM_SCRIPTING_ENABLED
// Function pointer targets — registered with Coral via AddInternalCall.
// Signatures must match the C# delegate* unmanaged<> fields in GameObject.cs /
// ScriptSecurity.cs.
void GameObject_GetName(Coral::String ObjectId, Coral::String *OutName);
void GameObject_GetTransform(Coral::String ObjectId,
                              EditorTransformDetails *OutTransform);
void GameObject_SetTransform(Coral::String ObjectId,
                              const EditorTransformDetails *InTransform);
Coral::Bool32 GameObject_GetVisible(Coral::String ObjectId);
Coral::Bool32 ScriptSecurity_IsRestricted();
#endif

} // namespace Axiom::InternalCalls
