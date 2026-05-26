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
  bool NeedsRender{true};
  uint32_t ActiveBurstTicksRemaining{0};
};

class HeadlessRenderViewRegistry {
public:
  static constexpr uint32_t RecentlyActiveBurstTicks = 3u;
  static constexpr uint32_t IdleRenderIntervalTicks = 4u;

  explicit HeadlessRenderViewRegistry(SessionUserId LocalUser = SessionUserId{1}) {
    EnsureLocalView(LocalUser);
  }

  HeadlessRenderViewState &EnsureLocalView(SessionUserId LocalUser) {
    m_LocalView.User = LocalUser;
    m_LocalView.IsLocal = true;
    m_LocalView.NeedsRender = true;
    m_LocalView.ActiveBurstTicksRemaining = RecentlyActiveBurstTicks;
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
    MarkViewActive(View, true);
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
    auto It = m_RemoteViewsByClientId.find(std::string(ClientId));
    if (It == m_RemoteViewsByClientId.end()) {
      return false;
    }

    m_FocusedClientId = std::string(ClientId);
    MarkViewActive(It->second, false);
    return true;
  }

  void FocusLocalView() {
    m_FocusedClientId.reset();
    MarkViewActive(m_LocalView, false);
  }

  bool SetViewMode(SessionUserId User, RendererViewMode ViewMode) {
    if (m_LocalView.User.Value == User.Value) {
      m_LocalView.ViewMode = ViewMode;
      MarkViewActive(m_LocalView, true);
      return true;
    }

    for (auto &[ClientId, View] : m_RemoteViewsByClientId) {
      (void)ClientId;
      if (View.User.Value == User.Value) {
        View.ViewMode = ViewMode;
        MarkViewActive(View, true);
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
    MarkViewActive(It->second, true);
    return true;
  }

  bool SetShowColliders(SessionUserId User, bool ShowColliders) {
    if (m_LocalView.User.Value == User.Value) {
      m_LocalView.ShowColliders = ShowColliders;
      MarkViewActive(m_LocalView, true);
      return true;
    }

    for (auto &[ClientId, View] : m_RemoteViewsByClientId) {
      (void)ClientId;
      if (View.User.Value == User.Value) {
        View.ShowColliders = ShowColliders;
        MarkViewActive(View, true);
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
    MarkViewActive(It->second, true);
    return true;
  }

  void MarkAllRemoteViewsDirty() {
    for (auto &[ClientId, View] : m_RemoteViewsByClientId) {
      (void)ClientId;
      MarkViewActive(View, true);
    }
  }

  bool MarkRemoteViewActive(std::string_view ClientId, bool NeedsRender = true) {
    auto It = m_RemoteViewsByClientId.find(std::string(ClientId));
    if (It == m_RemoteViewsByClientId.end()) {
      return false;
    }

    MarkViewActive(It->second, NeedsRender);
    return true;
  }

  bool MarkViewActive(SessionUserId User, bool NeedsRender = true) {
    if (m_LocalView.User.Value == User.Value) {
      MarkViewActive(m_LocalView, NeedsRender);
      return true;
    }

    for (auto &[ClientId, View] : m_RemoteViewsByClientId) {
      (void)ClientId;
      if (View.User.Value == User.Value) {
        MarkViewActive(View, NeedsRender);
        return true;
      }
    }
    return false;
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

  void AdvanceRenderSchedulingTick() {
    ++m_SchedulingTick;
    for (auto &[ClientId, View] : m_RemoteViewsByClientId) {
      (void)ClientId;
      if (View.ActiveBurstTicksRemaining > 0u) {
        --View.ActiveBurstTicksRemaining;
      }
    }
    if (m_LocalView.ActiveBurstTicksRemaining > 0u) {
      --m_LocalView.ActiveBurstTicksRemaining;
    }
  }

  uint64_t GetSchedulingTick() const { return m_SchedulingTick; }

  void MarkViewRendered(SessionUserId User) {
    if (m_LocalView.User.Value == User.Value) {
      m_LocalView.NeedsRender = false;
      return;
    }

    for (auto &[ClientId, View] : m_RemoteViewsByClientId) {
      (void)ClientId;
      if (View.User.Value == User.Value) {
        View.NeedsRender = false;
        return;
      }
    }
  }

private:
  static void MarkViewActive(HeadlessRenderViewState &View, bool NeedsRender) {
    View.NeedsRender = View.NeedsRender || NeedsRender;
    View.ActiveBurstTicksRemaining =
        std::max(View.ActiveBurstTicksRemaining, RecentlyActiveBurstTicks);
  }

  HeadlessRenderViewState m_LocalView{
      .ClientId = "",
      .User = SessionUserId{1},
      .ViewMode = RendererViewMode::Lit,
      .ShowColliders = true,
      .IsLocal = true,
      .NeedsRender = true,
      .ActiveBurstTicksRemaining = RecentlyActiveBurstTicks,
  };
  std::unordered_map<std::string, HeadlessRenderViewState> m_RemoteViewsByClientId;
  std::optional<std::string> m_FocusedClientId;
  uint64_t m_SchedulingTick{0};
};
} // namespace Axiom
