#pragma once

#include "HeadlessSessionHost.h"
#include "WebRtcSession.h"

#include <Remote/SessionTransport.h>
#include <Renderer/RendererBackend.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace Axiom {
struct RemoteViewportServerOptions {
  std::string Host{"127.0.0.1"};
  uint16_t Port{8080};
  uint32_t Width{1600};
  uint32_t Height{900};
  int JpegQuality{75};
};

class RemoteViewportServer final : public ISessionTransportSubscriber {
public:
  RemoteViewportServer(HeadlessSessionHost &Host,
                       const RemoteViewportServerOptions &Options);
  ~RemoteViewportServer() override;

  bool Start(std::string &Error);
  void Stop();

  bool ShouldStop() const { return m_StopRequested.load(); }
  uint16_t GetPort() const { return m_Options.Port; }

  void OnSessionTransportConnected() override;
  void OnSessionTransportDisconnected() override;
  void OnSessionTransportEditorEvent(
      const PublishedEditorEvent &Event) override;
  void OnSessionTransportViewportFrame(const ViewportFrame &Frame) override;
  void OnSessionTransportEncodedVideoPacket(
      const EncodedVideoPacket &Packet) override;

private:
  struct LatestFrame {
    uint64_t FrameIndex{0};
    uint32_t Width{0};
    uint32_t Height{0};
    std::vector<unsigned char> JpegBytes;
  };

  struct LatestEncodedPacket {
    EncodedVideoPacket Packet;
    bool HasPacket{false};
  };

  struct WebSocketClient {
    uintptr_t SocketValue{static_cast<uintptr_t>(~0ull)};
    bool IsOpen{true};
  };

private:
  void AcceptLoop();
  void HandleClient(uintptr_t ClientSocketValue);
  void BroadcastTextMessage(std::string Message);
  void BroadcastBinaryFrame(const LatestFrame &Frame);
  void CloseAllClients();
  void RemoveWebSocketClient(uintptr_t ClientSocketValue);
  bool SendTextMessage(uintptr_t ClientSocketValue, std::string_view Message);
  bool SendBinaryMessage(uintptr_t ClientSocketValue, const void *Data,
                         size_t Size);
  bool HandleHttpRequest(uintptr_t ClientSocketValue);
  bool HandleGetRequest(uintptr_t ClientSocketValue, std::string_view Path);
  bool HandlePostRequest(uintptr_t ClientSocketValue, std::string_view Path,
                         std::string_view Body);
  bool HandleWebRtcOfferRequest(uintptr_t ClientSocketValue,
                                std::string_view Body);
  bool HandleWebRtcIceCandidateRequest(uintptr_t ClientSocketValue,
                                       std::string_view Body);
  bool HandleWebRtcCloseRequest(uintptr_t ClientSocketValue,
                                std::string_view Body);
  bool HandleWebSocketUpgrade(uintptr_t ClientSocketValue,
                              std::string_view HeaderBlock,
                              std::string_view Path);
  void RunWebSocketSession(uintptr_t ClientSocketValue);
  bool HandleWebSocketMessage(std::string_view Payload);

  void SetLatestFrame(const CapturedFrame &Frame);
  bool TryGetLatestFrame(LatestFrame &Frame) const;
  void SetLatestEncodedPacket(const EncodedVideoPacket &Packet);
  bool TryGetLatestEncodedPacket(LatestEncodedPacket &Packet) const;

private:
  HeadlessSessionHost &m_Host;
  RemoteViewportServerOptions m_Options;
  std::atomic<bool> m_StopRequested{false};
  uintptr_t m_ListenSocket{static_cast<uintptr_t>(~0ull)};
  std::thread m_AcceptThread;

  mutable std::mutex m_FrameMutex;
  LatestFrame m_LatestFrame;
  LatestEncodedPacket m_LatestEncodedPacket;

  mutable std::mutex m_ClientMutex;
  std::vector<WebSocketClient> m_WebSocketClients;
  mutable std::mutex m_SendMutex;
  std::unique_ptr<IWebRtcSession> m_WebRtcSession;
};

bool ParseRemoteViewportServerOptions(int argc, char **argv,
                                      RemoteViewportServerOptions &Options,
                                      std::string &Error);
} // namespace Axiom
