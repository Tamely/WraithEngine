#include "RemoteViewportServer.h"
#include "WraithNetworkingModule.h"

#include "HeadlessCommandProtocol.h"

#include <iostream>
#include <memory>

int main(int argc, char **argv) {
  std::string Error;
  Axiom::RemoteViewportServerOptions Options{};
  if (!Axiom::ParseRemoteViewportServerOptions(argc, argv, Options, Error)) {
    std::cerr << Axiom::SerializeError(Error) << std::endl;
    return 1;
  }

  Axiom::HeadlessSessionHost Host({argv, argc}, Options.Width, Options.Height);
  if (!Host.LoadStartupSceneIntoSession()) {
    std::cerr << Axiom::SerializeError("Failed to load the startup scene.")
              << std::endl;
    return 1;
  }

  auto NetworkingModule =
      std::make_unique<Axiom::WraithNetworkingModule>(Host, Options);
  Axiom::WraithNetworkingModule *NetworkingModulePtr = NetworkingModule.get();
  if (!Host.GetModuleManager().RegisterModule(std::move(NetworkingModule))) {
    std::cerr
        << Axiom::SerializeError("Failed to register the WraithNetworking module.")
        << std::endl;
    return 1;
  }
  const auto NetworkingState = NetworkingModulePtr->GetStateSnapshot();
  if (!NetworkingModulePtr->IsInitialized()) {
    std::cerr << Axiom::SerializeError(
                     NetworkingState.LastError.empty()
                         ? "Failed to initialize the WraithNetworking module."
                         : NetworkingState.LastError)
              << std::endl;
    return 1;
  }

  std::cout << Axiom::SerializeReady(Options.Width, Options.Height)
            << std::endl;
  while (!NetworkingModulePtr->ShouldStop() && Host.Step()) {
  }

  std::cout << Axiom::SerializeShutdown() << std::endl;
  return 0;
}
