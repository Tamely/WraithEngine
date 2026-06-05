#pragma once

#include "CoreInstance/Instance.h"

namespace Axiom {

class DataModel final : public Instance {
public:
  explicit DataModel(const std::string &Name = "DataModel") : Instance(Name) {}
  AX_INSTANCE_BODY(DataModel)
};

class SceneFolder final : public Instance {
public:
  explicit SceneFolder(const std::string &Name = "Folder") : Instance(Name) {}
  AX_INSTANCE_BODY(SceneFolder)
};

class SceneMeshObject final : public Instance {
public:
  explicit SceneMeshObject(const std::string &Name = "Mesh") : Instance(Name) {}
  AX_INSTANCE_BODY(SceneMeshObject)
};

class SceneLight final : public Instance {
public:
  explicit SceneLight(const std::string &Name = "Light") : Instance(Name) {}
  AX_INSTANCE_BODY(SceneLight)
};

class SceneCamera final : public Instance {
public:
  explicit SceneCamera(const std::string &Name = "Camera") : Instance(Name) {}
  AX_INSTANCE_BODY(SceneCamera)
};

class SceneActor final : public Instance {
public:
  explicit SceneActor(const std::string &Name = "Actor") : Instance(Name) {}
  AX_INSTANCE_BODY(SceneActor)
};

} // namespace Axiom
