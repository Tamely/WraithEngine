#include "RemoteViewportServer.h"

#include "HeadlessCommandProtocol.h"

#include <iostream>

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

  Axiom::RemoteViewportServer Server(Host, Options);
  if (!Server.Start(Error)) {
    std::cerr << Axiom::SerializeError(Error) << std::endl;
    return 1;
  }

  std::cout << Axiom::SerializeReady(Options.Width, Options.Height)
            << std::endl;
  while (!Server.ShouldStop() && Host.Step()) {
  }

  Server.Stop();
  std::cout << Axiom::SerializeShutdown() << std::endl;
  return 0;
}
