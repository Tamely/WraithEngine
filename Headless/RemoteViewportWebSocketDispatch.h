#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace Axiom {
class RemoteViewportServer;

class RemoteViewportWebSocketDispatch {
public:
  explicit RemoteViewportWebSocketDispatch(RemoteViewportServer &Server);

  size_t GetActiveClientCount() const;
  uint64_t GetTotalWebSocketMessages() const;

  void OnClientOpen(uintptr_t ConnectionId, void *Socket);
  void OnClientClose(uintptr_t ConnectionId);
  void CloseAllClients();
  void BroadcastTextMessage(std::string Message);
  bool SendTextMessage(uintptr_t ClientSocketValue, std::string_view Message);
  bool SendBinaryMessage(uintptr_t ClientSocketValue, const void *Data,
                         size_t Size);
  bool HandleWebSocketMessage(uintptr_t ClientSocketValue,
                              std::string_view Payload);
  bool HandleClientWebRtcMessage(std::string_view ClientId,
                                 std::string_view Payload);

private:
  struct WebSocketClient {
    uintptr_t ConnectionId{static_cast<uintptr_t>(~0ull)};
    void *Socket{nullptr};
    bool IsOpen{true};
  };

  RemoteViewportServer &m_Server;
  mutable std::mutex m_WebSocketMutex;
  std::vector<WebSocketClient> m_WebSocketClients;
  mutable std::mutex m_SendMutex;
  std::atomic<uint64_t> m_TotalWebSocketMessages{0};
};
} // namespace Axiom
