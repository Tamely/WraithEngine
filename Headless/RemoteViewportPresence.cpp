#include "RemoteViewportPresence.h"

#include "RemoteViewportServer.h"
#include "RemoteViewportWebRtcSessionManager.h"

#include <Session/EditorSession.h>

#include <chrono>
#include <thread>
#include <vector>

namespace Axiom {
namespace {
constexpr int AwayThresholdSeconds = 10;
constexpr int DisconnectThresholdSeconds = 30;
constexpr int PresenceCheckIntervalMs = 2000;
} // namespace

RemoteViewportPresence::RemoteViewportPresence(RemoteViewportServer &Server)
    : m_Server(Server) {}

void RemoteViewportPresence::RunLoop() {
  while (!m_Server.m_StopRequested.load()) {
    std::this_thread::sleep_for(
        std::chrono::milliseconds(PresenceCheckIntervalMs));
    if (m_Server.m_StopRequested.load()) {
      break;
    }

    const auto Now = std::chrono::steady_clock::now();
    std::vector<std::pair<SessionUserId, EditorUserPresenceState>> Transitions;
    for (const auto &[User, LastActivity] :
         m_Server.m_WebRtcSessions->CollectPresenceEntries()) {
      const auto Elapsed =
          std::chrono::duration_cast<std::chrono::seconds>(Now - LastActivity)
              .count();
      const EditorUserPresence *Presence =
          m_Server.m_Host.GetSessionModule().GetSession().FindPresence(User);
      if (Presence == nullptr) {
        continue;
      }
      if (Elapsed >= DisconnectThresholdSeconds &&
          Presence->State == EditorUserPresenceState::Away) {
        Transitions.emplace_back(User, EditorUserPresenceState::Disconnected);
      } else if (Elapsed >= AwayThresholdSeconds &&
                 Presence->State == EditorUserPresenceState::Connected) {
        Transitions.emplace_back(User, EditorUserPresenceState::Away);
      }
    }

    for (const auto &[User, State] : Transitions) {
      m_Server.m_Host.GetSessionModule().GetSession().SetPresenceState(User,
                                                                       State);
      if (State == EditorUserPresenceState::Disconnected) {
        m_Server.m_Host.GetSessionModule().GetSession().ReleaseAllLocksForUser(
            User);
      }
    }
  }
}
} // namespace Axiom
