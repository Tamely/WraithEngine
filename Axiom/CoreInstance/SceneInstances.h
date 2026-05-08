#pragma once

#include "CoreInstance/Instance.h"

namespace Axiom {

class DataModel final : public Instance {
public:
  DataModel() : Instance("DataModel") {}
  GENERATED_BODY(DataModel)
};

class SceneFolder final : public Instance {
public:
  explicit SceneFolder(const std::string &Name = "Folder") : Instance(Name) {}
  GENERATED_BODY(SceneFolder)
};

class SceneMeshObject final : public Instance {
public:
  explicit SceneMeshObject(const std::string &Name = "Mesh") : Instance(Name) {}
  GENERATED_BODY(SceneMeshObject)
};

class SceneLight final : public Instance {
public:
  explicit SceneLight(const std::string &Name = "Light") : Instance(Name) {}
  GENERATED_BODY(SceneLight)
};

class SceneCamera final : public Instance {
public:
  explicit SceneCamera(const std::string &Name = "Camera") : Instance(Name) {}
  GENERATED_BODY(SceneCamera)
};

class SceneActor final : public Instance {
public:
  explicit SceneActor(const std::string &Name = "Actor") : Instance(Name) {}
  GENERATED_BODY(SceneActor)
};

} // namespace Axiom
