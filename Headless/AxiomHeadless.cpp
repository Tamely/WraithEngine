#include <Core/Application.h>
#include <Remote/AxiomSessionEndpoint.h>
#include <Renderer/Renderer.h>

#include <filesystem>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <cstring>
#include <vector>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include "HeadlessCommandProtocol.h"
#include "HeadlessSessionLayer.h"

namespace {
class HeadlessEndpointSubscriber final
    : public Axiom::ISessionTransportSubscriber {
public:
  explicit HeadlessEndpointSubscriber(std::ostream &Output) : m_Output(Output) {}

  void OnSessionTransportEditorEvent(
      const Axiom::PublishedEditorEvent &Event) override {
    m_Output << Axiom::SerializeEvent(Event) << std::endl;
  }

  void OnSessionTransportViewportFrame(
      const Axiom::ViewportFrame &Frame) override {
    m_LastFrame = ConvertFrame(Frame);
  }

  void DiscardLatestFrame() { m_LastFrame.reset(); }

  std::optional<Axiom::CapturedFrame> TakeLatestFrame() {
    std::optional<Axiom::CapturedFrame> Result = std::move(m_LastFrame);
    m_LastFrame.reset();
    return Result;
  }

private:
  static std::optional<Axiom::CapturedFrame>
  ConvertFrame(const Axiom::ViewportFrame &Frame) {
    if (Frame.Format != Axiom::ViewportFrameFormat::R8G8B8A8Unorm) {
      return std::nullopt;
    }

    Axiom::CapturedFrame Captured{};
    Captured.FrameIndex = Frame.FrameIndex;
    Captured.Width = Frame.Width;
    Captured.Height = Frame.Height;
    Captured.Pixels.resize(Frame.Pixels.size());
    std::memcpy(Captured.Pixels.data(), Frame.Pixels.data(), Frame.Pixels.size());
    return Captured;
  }

  std::ostream &m_Output;
  std::optional<Axiom::CapturedFrame> m_LastFrame;
};

class HeadlessApplication final : public Axiom::Application {
public:
  HeadlessApplication(const Axiom::ApplicationArgs &Args, uint32_t Width,
                      uint32_t Height)
      : Axiom::Application({.Title = "Axiom Headless",
                            .Width = Width,
                            .Height = Height,
                            .Mode = Axiom::RuntimeMode::HeadlessEditorSession},
                           Args) {
    m_Layer = new Axiom::HeadlessSessionLayer();
    PushLayer(m_Layer);
    m_Endpoint =
        std::make_unique<Axiom::AxiomSessionEndpoint>(m_Layer->GetSession());
    SetViewportFrameOutput(m_Endpoint.get());
  }

  Axiom::HeadlessSessionLayer &GetHeadlessLayer() { return *m_Layer; }
  Axiom::AxiomSessionEndpoint &GetEndpoint() { return *m_Endpoint; }

private:
  Axiom::HeadlessSessionLayer *m_Layer{nullptr};
  std::unique_ptr<Axiom::AxiomSessionEndpoint> m_Endpoint;
};

bool WritePng(const std::filesystem::path &Path,
              const Axiom::CapturedFrame &Frame) {
  return stbi_write_png(Path.string().c_str(), static_cast<int>(Frame.Width),
                        static_cast<int>(Frame.Height), 4, Frame.Pixels.data(),
                        static_cast<int>(Frame.Width * 4u)) != 0;
}

std::filesystem::path NextFramePath(const std::filesystem::path &OutputDirectory,
                                    uint64_t FrameIndex) {
  std::ostringstream Name;
  Name << "frame_" << std::setw(6) << std::setfill('0') << FrameIndex << ".png";
  return OutputDirectory / Name.str();
}
} // namespace

int main(int argc, char **argv) {
  std::string Error;
  const auto Options = Axiom::ParseHeadlessOptions(argc, argv, Error);
  if (!Options.has_value()) {
    std::cerr << Axiom::SerializeError(Error) << std::endl;
    return 1;
  }

  std::error_code FileSystemError;
  if (std::filesystem::exists(Options->OutputDirectory, FileSystemError) &&
      !std::filesystem::is_directory(Options->OutputDirectory, FileSystemError)) {
    std::cerr << Axiom::SerializeError(
                     "The output path exists but is not a directory.")
              << std::endl;
    return 1;
  }
  std::filesystem::create_directories(Options->OutputDirectory, FileSystemError);
  if (FileSystemError) {
    std::cerr << Axiom::SerializeError("Failed to create output directory.") << std::endl;
    return 1;
  }

  HeadlessApplication App({argv, argc}, Options->Width, Options->Height);
  auto &Layer = App.GetHeadlessLayer();
  HeadlessEndpointSubscriber Subscriber(std::cout);
  App.GetEndpoint().Connect(&Subscriber);

  std::cout << Axiom::SerializeReady(Options->Width, Options->Height)
            << std::endl;

  bool SceneLoaded = false;
  uint64_t EmittedFrameCount = 0;
  for (std::string Line; std::getline(std::cin, Line);) {
    if (Line.empty()) {
      continue;
    }

    Error.clear();
    const auto Command = Axiom::ParseHeadlessCommand(Line, Error);
    if (!Command.has_value()) {
      std::cout << Axiom::SerializeError(Error) << std::endl;
      continue;
    }

    switch (Command->Type) {
    case Axiom::HeadlessCommandType::LoadStartupScene:
      if (SceneLoaded) {
        std::cout << Axiom::SerializeError(
                         "Startup scene has already been loaded.")
                  << std::endl;
        break;
      }
      if (!Layer.LoadStartupSceneIntoSession()) {
        std::cout << Axiom::SerializeError(
                         "Failed to load the startup scene.")
                  << std::endl;
        break;
      }
      SceneLoaded = true;
      break;
    case Axiom::HeadlessCommandType::SetLookActive:
    case Axiom::HeadlessCommandType::UpdateViewportCamera:
      Layer.Submit(Command->EditorPayload);
      App.Step();
      Subscriber.DiscardLatestFrame();
      break;
    case Axiom::HeadlessCommandType::RenderFrame: {
      if (!SceneLoaded) {
        std::cout << Axiom::SerializeError(
                         "Cannot render before `load_startup_scene`.")
                  << std::endl;
        break;
      }
      App.Step();
      const auto Frame = Subscriber.TakeLatestFrame();
      if (!Frame.has_value()) {
        std::cout << Axiom::SerializeError("Renderer did not produce a frame.")
                  << std::endl;
        break;
      }

      ++EmittedFrameCount;
      const auto FramePath =
          NextFramePath(Options->OutputDirectory, EmittedFrameCount);
      if (!WritePng(FramePath, *Frame)) {
        std::cout << Axiom::SerializeError("Failed to write frame PNG.")
                  << std::endl;
        break;
      }
      std::cout << Axiom::SerializeFrame(FramePath, *Frame) << std::endl;
      break;
    }
    case Axiom::HeadlessCommandType::Quit:
      App.RequestClose();
      std::cout << Axiom::SerializeShutdown() << std::endl;
      return 0;
    }
  }

  App.GetEndpoint().Disconnect(&Subscriber);
  std::cout << Axiom::SerializeShutdown() << std::endl;
  return 0;
}
