#pragma once

namespace Axiom {
class RemoteViewportServer;

class RemoteViewportPresence {
public:
  explicit RemoteViewportPresence(RemoteViewportServer &Server);

  void RunLoop();

private:
  RemoteViewportServer &m_Server;
};
} // namespace Axiom
