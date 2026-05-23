#include "PackagedRuntimeHost.h"

#include <Assets/CookedAssetRuntime.h>
#include <Core/Log.h>

#include <filesystem>
#include <iostream>
#include <optional>
#include <string>

namespace {

struct PackagedRuntimeOptions {
  std::filesystem::path PackageRoot;
  uint32_t Width{1600};
  uint32_t Height{900};
};

std::optional<PackagedRuntimeOptions>
ParsePackagedRuntimeOptions(int argc, char **argv, std::string &Error) {
  PackagedRuntimeOptions Options;
  if (argc > 0 && argv != nullptr && argv[0] != nullptr) {
    std::error_code Ec;
    Options.PackageRoot =
        std::filesystem::weakly_canonical(std::filesystem::path(argv[0]), Ec)
            .parent_path();
    if (Ec) {
      Options.PackageRoot = std::filesystem::absolute(std::filesystem::path(argv[0]))
                                .parent_path();
    }
  } else {
    Options.PackageRoot = std::filesystem::current_path();
  }

  for (int Index = 1; Index < argc; ++Index) {
    const std::string_view Argument = argv[Index];
    if (Argument == "--package-root") {
      if (Index + 1 >= argc) {
        Error = "Missing value for --package-root.";
        return std::nullopt;
      }
      Options.PackageRoot = argv[++Index];
      continue;
    }
    Error = "Unknown argument: " + std::string(Argument);
    return std::nullopt;
  }

  return Options;
}

} // namespace

int main(int argc, char **argv) {
  std::string Error;
  const auto Options = ParsePackagedRuntimeOptions(argc, argv, Error);
  if (!Options.has_value()) {
    std::cerr << Error << std::endl;
    return 1;
  }

  const std::filesystem::path ContentRoot = Options->PackageRoot / "Content";
  std::string FailureReason;
  const auto Descriptor =
      Axiom::Assets::ResolvePackagedContentDescriptor(ContentRoot, &FailureReason);
  if (!Descriptor.has_value()) {
    std::cerr << "Invalid package root '" << Options->PackageRoot.string()
              << "': " << FailureReason << std::endl;
    return 1;
  }
  if (!std::filesystem::exists(Descriptor->SceneAssetPath)) {
    std::cerr << "Packaged scene asset is missing: "
              << Descriptor->SceneAssetPath.string() << std::endl;
    return 1;
  }
  if (!std::filesystem::exists(Descriptor->CookManifestPath)) {
    std::cerr << "Packaged asset cook manifest is missing: "
              << Descriptor->CookManifestPath.string() << std::endl;
    return 1;
  }
  if (!std::filesystem::exists(Descriptor->EngineContentDir)) {
    std::cerr << "Packaged engine content directory is missing: "
              << Descriptor->EngineContentDir.string() << std::endl;
    return 1;
  }

  Axiom::PackagedRuntimeHost Host({argv, argc}, Options->Width, Options->Height);
  if (!Host.LoadPackagedProject(ContentRoot, &FailureReason)) {
    std::cerr << FailureReason << std::endl;
    return 1;
  }

  Host.Run();
  return 0;
}
