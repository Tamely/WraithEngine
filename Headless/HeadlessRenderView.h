#pragma once

#include <Renderer/RendererBackend.h>
#include <Session/SessionTypes.h>

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Axiom {
struct HeadlessRenderViewState {
  std::string ClientId;
  SessionUserId User;
  RendererViewMode ViewMode{RendererViewMode::Lit};
  bool ShowColliders{true};
  bool IsLocal{false};
};

class HeadlessRenderViewRegistry {
public:
  explicit HeadlessRenderViewRegistry(SessionUserId LocalUser = SessionUserId{1}) {
    EnsureLocalView(LocalUser);
  }

  HeadlessRenderViewState &EnsureLocalView(SessionUserId LocalUser) {
    m_LocalView.User = LocalUser;
    m_LocalView.IsLocal = true;
    if (m_FocusedClientId.has_value() &&
        !m_RemoteViewsByClientId.contains(*m_FocusedClientId)) {
      m_FocusedClientId.reset();
    }
    return m_LocalView;
  }

  HeadlessRenderViewState &UpsertRemoteView(std::string ClientId,
                                            SessionUserId User) {
    auto [It, Inserted] = m_RemoteViewsByClientId.try_emplace(std::move(ClientId));
    HeadlessRenderViewState &View = It->second;
    if (Inserted) {
      View.ClientId = It->first;
      View.ViewMode = RendererViewMode::Lit;
      View.ShowColliders = true;
    }
    View.User = User;
    View.IsLocal = false;
    return View;
  }

  bool RemoveRemoteView(std::string_view ClientId) {
    const auto It = m_RemoteViewsByClientId.find(std::string(ClientId));
    if (It == m_RemoteViewsByClientId.end()) {
      return false;
    }

    m_RemoteViewsByClientId.erase(It);
    if (m_FocusedClientId == ClientId) {
      m_FocusedClientId.reset();
    }
    return true;
  }

  bool FocusRemoteView(std::string_view ClientId) {
    if (!m_RemoteViewsByClientId.contains(std::string(ClientId))) {
      return false;
    }

    m_FocusedClientId = std::string(ClientId);
    return true;
  }

  void FocusLocalView() { m_FocusedClientId.reset(); }

  bool SetViewMode(SessionUserId User, RendererViewMode ViewMode) {
    if (m_LocalView.User.Value == User.Value) {
      m_LocalView.ViewMode = ViewMode;
      return true;
    }

    for (auto &[ClientId, View] : m_RemoteViewsByClientId) {
      (void)ClientId;
      if (View.User.Value == User.Value) {
        View.ViewMode = ViewMode;
        return true;
      }
    }
    return false;
  }

  bool SetRemoteViewMode(std::string_view ClientId, RendererViewMode ViewMode) {
    auto It = m_RemoteViewsByClientId.find(std::string(ClientId));
    if (It == m_RemoteViewsByClientId.end()) {
      return false;
    }

    It->second.ViewMode = ViewMode;
    return true;
  }

  bool SetShowColliders(SessionUserId User, bool ShowColliders) {
    if (m_LocalView.User.Value == User.Value) {
      m_LocalView.ShowColliders = ShowColliders;
      return true;
    }

    for (auto &[ClientId, View] : m_RemoteViewsByClientId) {
      (void)ClientId;
      if (View.User.Value == User.Value) {
        View.ShowColliders = ShowColliders;
        return true;
      }
    }
    return false;
  }

  bool SetRemoteShowColliders(std::string_view ClientId, bool ShowColliders) {
    auto It = m_RemoteViewsByClientId.find(std::string(ClientId));
    if (It == m_RemoteViewsByClientId.end()) {
      return false;
    }

    It->second.ShowColliders = ShowColliders;
    return true;
  }

  const HeadlessRenderViewState *FindRemoteView(
      std::string_view ClientId) const {
    const auto It = m_RemoteViewsByClientId.find(std::string(ClientId));
    return It != m_RemoteViewsByClientId.end() ? &It->second : nullptr;
  }

  const HeadlessRenderViewState *FindView(SessionUserId User) const {
    if (m_LocalView.User.Value == User.Value) {
      return &m_LocalView;
    }

    for (const auto &[ClientId, View] : m_RemoteViewsByClientId) {
      (void)ClientId;
      if (View.User.Value == User.Value) {
        return &View;
      }
    }
    return nullptr;
  }

  const HeadlessRenderViewState *GetFocusedView() const {
    if (m_FocusedClientId.has_value()) {
      const auto It = m_RemoteViewsByClientId.find(*m_FocusedClientId);
      if (It != m_RemoteViewsByClientId.end()) {
        return &It->second;
      }
    }
    return &m_LocalView;
  }

  size_t GetRemoteViewCount() const { return m_RemoteViewsByClientId.size(); }

  std::vector<HeadlessRenderViewState> BuildRemoteViewSnapshot() const {
    std::vector<HeadlessRenderViewState> Result;
    Result.reserve(m_RemoteViewsByClientId.size());
    for (const auto &[ClientId, View] : m_RemoteViewsByClientId) {
      (void)ClientId;
      Result.push_back(View);
    }
    return Result;
  }

private:
  HeadlessRenderViewState m_LocalView{
      .ClientId = "",
      .User = SessionUserId{1},
      .ViewMode = RendererViewMode::Lit,
      .ShowColliders = true,
      .IsLocal = true,
  };
  std::unordered_map<std::string, HeadlessRenderViewState> m_RemoteViewsByClientId;
  std::optional<std::string> m_FocusedClientId;
};
} // namespace Axiom
