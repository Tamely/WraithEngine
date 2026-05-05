#pragma once

#include "Renderer/VideoEncoding.h"
#include "Renderer/ViewportFrameOutput.h"
#include "Session/EditorCommand.h"
#include "Session/EditorEvent.h"

namespace Axiom {
class ISessionTransportSubscriber {
public:
  virtual ~ISessionTransportSubscriber() = default;

  virtual void OnSessionTransportConnected() {}
  virtual void OnSessionTransportDisconnected() {}
  virtual void OnSessionTransportEditorEvent(
      const PublishedEditorEvent &Event) = 0;
  virtual void OnSessionTransportViewportFrame(const ViewportFrame &Frame) = 0;
  virtual void OnSessionTransportEncodedVideoPacket(
      const EncodedVideoPacket &Packet) {}
};

class ISessionTransport {
public:
  virtual ~ISessionTransport() = default;

  virtual void Submit(const CommandContext &Context,
                      const EditorCommand &Command) = 0;
  virtual void Connect(ISessionTransportSubscriber *Subscriber) = 0;
  virtual void Disconnect(ISessionTransportSubscriber *Subscriber) = 0;
};
} // namespace Axiom
