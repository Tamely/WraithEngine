bool RemoteViewportHttpRouter::HandleCreateProjectRequest(
    uintptr_t ClientSocketValue, std::string_view Body) {
  const auto ProjectName = ExtractJsonStringField(Body, "name");
  if (!ProjectName.has_value() || ProjectName->empty()) {
    SendJsonError(ClientSocketValue, "400 Bad Request",
                  "Missing required 'name' field.");
    return false;
  }
  std::string FailureReason;
  const auto Created = Project::CreateProjectScaffold(
      Project::GetDefaultProjectsRoot(), *ProjectName, &FailureReason);
  if (!Created.has_value()) {
    SendJsonError(ClientSocketValue, "400 Bad Request", FailureReason);
    return false;
  }
  SetActiveProject(*Created);
  FailureReason.clear();
  if (!LoadActiveProjectIntoSession(&FailureReason)) {
    SendJsonError(ClientSocketValue, "500 Internal Server Error", FailureReason);
    return false;
  }
  SendJsonResponse(ClientSocketValue, "201 Created",
                   SerializeCurrentProject(Created));
  return false;
}

bool RemoteViewportHttpRouter::HandleOpenProjectRequest(
    uintptr_t ClientSocketValue, std::string_view Body) {
  const auto ProjectSlug = ExtractJsonStringField(Body, "slug");
  if (!ProjectSlug.has_value() || ProjectSlug->empty()) {
    SendJsonError(ClientSocketValue, "400 Bad Request",
                  "Missing required 'slug' field.");
    return false;
  }
  const auto Opened = SetActiveProjectBySlug(*ProjectSlug);
  if (!Opened.has_value()) {
    SendJsonError(ClientSocketValue, "404 Not Found",
                  "Project was not found in the managed projects directory.");
    return false;
  }
  std::string FailureReason;
  if (!LoadActiveProjectIntoSession(&FailureReason)) {
    SendJsonError(ClientSocketValue, "500 Internal Server Error", FailureReason);
    return false;
  }
  SendJsonResponse(ClientSocketValue, "200 OK",
                   SerializeCurrentProject(Opened));
  return false;
}

bool RemoteViewportHttpRouter::HandleCookProjectRequest(
    uintptr_t ClientSocketValue) {
  const auto ActiveProject = GetActiveProject();
  if (!ActiveProject.has_value()) {
    SendJsonError(ClientSocketValue, "400 Bad Request",
                  "No active project is selected.");
    return false;
  }
  std::string FailureReason;
  const auto Result = Project::CookProjectContent(*ActiveProject, &FailureReason);
  if (!Result.has_value()) {
    SendJsonError(ClientSocketValue, "500 Internal Server Error", FailureReason);
    return false;
  }
  SendJsonResponse(ClientSocketValue, "200 OK",
                   SerializeProjectCookResult(*ActiveProject, *Result));
  return false;
}

bool RemoteViewportHttpRouter::HandlePackageProjectRequest(
    uintptr_t ClientSocketValue) {
  const auto ActiveProject = GetActiveProject();
  if (!ActiveProject.has_value()) {
    SendJsonError(ClientSocketValue, "400 Bad Request",
                  "No active project is selected.");
    return false;
  }
  std::string FailureReason;
  const auto Result =
      Project::PackageProjectContent(*ActiveProject, &FailureReason);
  if (!Result.has_value()) {
    SendJsonError(ClientSocketValue, "500 Internal Server Error", FailureReason);
    return false;
  }
  SendJsonResponse(ClientSocketValue, "200 OK",
                   SerializeProjectPackageResult(*ActiveProject, *Result));
  return false;
}

bool RemoteViewportHttpRouter::HandleListScriptsRequest(
    uintptr_t ClientSocketValue) {
  SendJsonResponse(ClientSocketValue, "200 OK",
                   SerializeScriptListJson(ListScriptFiles()));
  return false;
}

bool RemoteViewportHttpRouter::HandleListScriptClassesRequest(
    uintptr_t ClientSocketValue) {
  SendJsonResponse(ClientSocketValue, "200 OK",
                   SerializeScriptClassesJson(ListScriptClasses()));
  return false;
}

bool RemoteViewportHttpRouter::HandleReadScriptFileRequest(
    uintptr_t ClientSocketValue, std::string_view Path) {
  const auto RelativePath = GetQueryParam(Path, "path");
  if (!RelativePath.has_value() || RelativePath->empty()) {
    SendJsonError(ClientSocketValue, "400 Bad Request",
                  "Missing required 'path' query parameter.");
    return false;
  }
  const auto FilePath = ResolveActiveScriptPath(*RelativePath);
  if (!FilePath.has_value() || !std::filesystem::exists(*FilePath)) {
    SendJsonError(ClientSocketValue, "404 Not Found",
                  "Script file was not found.");
    return false;
  }
  std::ifstream File(*FilePath);
  if (!File.is_open()) {
    SendJsonError(ClientSocketValue, "500 Internal Server Error",
                  "Failed to open script file.");
    return false;
  }
  const std::string Content((std::istreambuf_iterator<char>(File)),
                            std::istreambuf_iterator<char>());
  SendJsonResponse(ClientSocketValue, "200 OK",
                   SerializeScriptFileJson(*RelativePath, Content));
  return false;
}

bool RemoteViewportHttpRouter::HandleCreateScriptFileRequest(
    uintptr_t ClientSocketValue, std::string_view Body) {
  const auto RelativePath = ExtractJsonStringField(Body, "path");
  if (!RelativePath.has_value() || RelativePath->empty()) {
    SendJsonError(ClientSocketValue, "400 Bad Request",
                  "Missing required 'path' field.");
    return false;
  }
  const auto FilePath = ResolveActiveScriptPath(*RelativePath, true);
  if (!FilePath.has_value()) {
    SendJsonError(ClientSocketValue, "400 Bad Request",
                  "Script path must stay inside the active project's Scripts directory and end in .cs.");
    return false;
  }
  if (std::filesystem::exists(*FilePath)) {
    SendJsonError(ClientSocketValue, "409 Conflict",
                  "A script file with that path already exists.");
    return false;
  }
  std::error_code Error;
  std::filesystem::create_directories(FilePath->parent_path(), Error);
  if (Error) {
    SendJsonError(ClientSocketValue, "500 Internal Server Error",
                  "Failed to create the script directory.");
    return false;
  }
  std::ofstream File(*FilePath);
  if (!File.is_open()) {
    SendJsonError(ClientSocketValue, "500 Internal Server Error",
                  "Failed to create script file.");
    return false;
  }

  const auto ActiveProject = GetActiveProject();
  const std::string Namespace = ActiveProject.has_value()
                                    ? ActiveProject->ScriptWorkspace.RootNamespace
                                    : "Project.Scripts";
  const std::string ClassName = FilePath->stem().string();
  std::ostringstream Template;
  Template << "using WraithEngine;\n\n"
           << "namespace " << Namespace << ";\n\n"
           << "public class " << ClassName << " : Script\n"
           << "{\n"
           << "    public override void OnCreate()\n"
           << "    {\n"
           << "    }\n\n"
           << "    public override void OnTick(float dt)\n"
           << "    {\n"
           << "    }\n"
           << "}\n";
  File << Template.str();
  SendJsonResponse(ClientSocketValue, "201 Created",
                   SerializeScriptMutationJson("script_created", *RelativePath));
  return false;
}

bool RemoteViewportHttpRouter::HandleSaveScriptFileRequest(
    uintptr_t ClientSocketValue, std::string_view Body) {
  const auto RelativePath = ExtractJsonStringField(Body, "path");
  const auto Content = ExtractJsonStringField(Body, "content");
  if (!RelativePath.has_value() || !Content.has_value()) {
    SendJsonError(ClientSocketValue, "400 Bad Request",
                  "Missing required 'path' or 'content' field.");
    return false;
  }
  const auto FilePath = ResolveActiveScriptPath(*RelativePath, true);
  if (!FilePath.has_value()) {
    SendJsonError(ClientSocketValue, "400 Bad Request",
                  "Script path must stay inside the active project's Scripts directory and end in .cs.");
    return false;
  }
  std::error_code Error;
  std::filesystem::create_directories(FilePath->parent_path(), Error);
  if (Error) {
    SendJsonError(ClientSocketValue, "500 Internal Server Error",
                  "Failed to create the script directory.");
    return false;
  }
  std::ofstream File(*FilePath);
  if (!File.is_open()) {
    SendJsonError(ClientSocketValue, "500 Internal Server Error",
                  "Failed to save script file.");
    return false;
  }
  File << *Content;
  SendJsonResponse(ClientSocketValue, "200 OK",
                   SerializeScriptMutationJson("script_saved", *RelativePath));
  return false;
}

bool RemoteViewportHttpRouter::HandleRenameScriptFileRequest(
    uintptr_t ClientSocketValue, std::string_view Body) {
  const auto RelativePath = ExtractJsonStringField(Body, "path");
  const auto NewRelativePath = ExtractJsonStringField(Body, "newPath");
  if (!RelativePath.has_value() || !NewRelativePath.has_value() ||
      RelativePath->empty() || NewRelativePath->empty()) {
    SendJsonError(ClientSocketValue, "400 Bad Request",
                  "Missing required 'path' or 'newPath' field.");
    return false;
  }
  const auto OldPath = ResolveActiveScriptPath(*RelativePath);
  const auto NewPath = ResolveActiveScriptPath(*NewRelativePath, true);
  if (!OldPath.has_value() || !NewPath.has_value() ||
      !std::filesystem::exists(*OldPath)) {
    SendJsonError(ClientSocketValue, "400 Bad Request",
                  "Script rename must stay inside the active project's Scripts directory and target an existing .cs file.");
    return false;
  }
  if (std::filesystem::exists(*NewPath)) {
    SendJsonError(ClientSocketValue, "409 Conflict",
                  "A script file with the destination path already exists.");
    return false;
  }
  std::error_code Error;
  std::filesystem::create_directories(NewPath->parent_path(), Error);
  if (Error) {
    SendJsonError(ClientSocketValue, "500 Internal Server Error",
                  "Failed to create the destination script directory.");
    return false;
  }
  std::filesystem::rename(*OldPath, *NewPath, Error);
  if (Error) {
    SendJsonError(ClientSocketValue, "500 Internal Server Error",
                  "Failed to rename script file.");
    return false;
  }
  SendJsonResponse(ClientSocketValue, "200 OK",
                   SerializeScriptMutationJson("script_renamed",
                                               *NewRelativePath));
  return false;
}

bool RemoteViewportHttpRouter::HandleDeleteScriptFileRequest(
    uintptr_t ClientSocketValue, std::string_view Body) {
  const auto RelativePath = ExtractJsonStringField(Body, "path");
  if (!RelativePath.has_value() || RelativePath->empty()) {
    SendJsonError(ClientSocketValue, "400 Bad Request",
                  "Missing required 'path' field.");
    return false;
  }
  const auto FilePath = ResolveActiveScriptPath(*RelativePath);
  if (!FilePath.has_value() || !std::filesystem::exists(*FilePath)) {
    SendJsonError(ClientSocketValue, "404 Not Found",
                  "Script file was not found.");
    return false;
  }
  std::error_code Error;
  const bool Removed = std::filesystem::remove(*FilePath, Error);
  if (Error || !Removed) {
    SendJsonError(ClientSocketValue, "500 Internal Server Error",
                  "Failed to delete script file.");
    return false;
  }
  SendJsonResponse(ClientSocketValue, "200 OK",
                   SerializeScriptMutationJson("script_deleted", *RelativePath));
  return false;
}

bool RemoteViewportHttpRouter::HandleAssetUploadRequest(
    uintptr_t ClientSocketValue, std::string_view Path,
    std::string_view HeaderBlock, std::string_view Body) {
  const auto ContentType = FindHeaderValue(HeaderBlock, "Content-Type");
  if (!ContentType.has_value() ||
      ContentType->find("multipart/form-data") == std::string::npos) {
    SendJsonError(ClientSocketValue, "400 Bad Request",
                  "Expected multipart/form-data Content-Type.");
    return false;
  }
  const auto BoundaryPos = ContentType->find("boundary=");
  if (BoundaryPos == std::string::npos) {
    SendJsonError(ClientSocketValue, "400 Bad Request", "Missing boundary.");
    return false;
  }
  const std::string Boundary = "--" + ContentType->substr(BoundaryPos + 9);
  const std::string TargetDir = GetQueryParam(Path, "dir").value_or("");
  const auto ContentRoot = GetActiveContentDir();

  std::filesystem::path DestDir;
  if (TargetDir.empty()) {
    DestDir = ContentRoot;
  } else {
    const std::filesystem::path TargetDirPath{TargetDir};
    bool Unsafe = false;
    for (const auto &Part : TargetDirPath) {
      if (Part.string().find("..") != std::string::npos) {
        Unsafe = true;
        break;
      }
    }
    if (Unsafe) {
      SendJsonError(ClientSocketValue, "400 Bad Request",
                    "Invalid target directory.");
      return false;
    }
    if (TargetDirPath.begin() != TargetDirPath.end() &&
        (*TargetDirPath.begin()).string() == "Engine") {
      SendJsonError(ClientSocketValue, "400 Bad Request",
                    "Engine content is read-only and cannot be modified by project uploads.");
      return false;
    }
    DestDir = ContentRoot / TargetDirPath;
  }

  static constexpr std::string_view kContentDisposition = "Content-Disposition:";
  std::vector<std::string> Saved;
  std::string_view Remaining{Body};
  while (true) {
    const auto BPos = Remaining.find(Boundary);
    if (BPos == std::string_view::npos) {
      break;
    }
    Remaining.remove_prefix(BPos + Boundary.size());
    if (Remaining.starts_with("--")) {
      break;
    }
    if (Remaining.starts_with("\r\n")) {
      Remaining.remove_prefix(2);
    }
    const auto HeaderEnd = Remaining.find("\r\n\r\n");
    if (HeaderEnd == std::string_view::npos) {
      break;
    }
    const std::string_view PartHeaders = Remaining.substr(0, HeaderEnd);
    Remaining.remove_prefix(HeaderEnd + 4);

    const auto CDPos = PartHeaders.find(kContentDisposition);
    if (CDPos == std::string_view::npos) {
      continue;
    }
    const auto FnPos = PartHeaders.find("filename=\"", CDPos);
    if (FnPos == std::string_view::npos) {
      continue;
    }
    const auto FnStart = FnPos + 10;
    const auto FnEnd = PartHeaders.find('"', FnStart);
    if (FnEnd == std::string_view::npos) {
      continue;
    }
    const std::string Filename{PartHeaders.substr(FnStart, FnEnd - FnStart)};
    if (Filename.empty()) {
      continue;
    }

    const auto BodyEnd = Remaining.find(Boundary);
    if (BodyEnd == std::string_view::npos) {
      break;
    }
    const size_t PartBodyLen = BodyEnd >= 2 ? BodyEnd - 2 : BodyEnd;
    const std::string_view PartBody = Remaining.substr(0, PartBodyLen);

    const std::filesystem::path FilePath{Filename};
    const std::string Ext = [&] {
      auto E = FilePath.extension().string();
      for (auto &C : E) {
        C = static_cast<char>(std::tolower(static_cast<unsigned char>(C)));
      }
      return E;
    }();
    static constexpr std::string_view kAllowed[] = {
        ".glb", ".gltf", ".fbx", ".obj", ".png", ".jpg", ".jpeg", ".hdr"};
    bool Allowed = false;
    for (const auto &A : kAllowed) {
      if (Ext == A) {
        Allowed = true;
        break;
      }
    }
    if (!Allowed) {
      std::cerr << "[AssetUpload] rejected '" << Filename
                << "': unsupported extension\n";
      continue;
    }

    std::error_code Ec;
    std::filesystem::create_directories(DestDir, Ec);
    const std::filesystem::path OutPath = DestDir / FilePath.filename();
    std::ofstream OutFile(OutPath, std::ios::binary);
    if (!OutFile.is_open()) {
      std::cerr << "[AssetUpload] could not open '" << OutPath.string()
                << "' for writing\n";
      continue;
    }
    OutFile.write(PartBody.data(), static_cast<std::streamsize>(PartBody.size()));
    OutFile.close();
    std::cerr << "[AssetUpload] saved '" << OutPath.string() << "'\n";

    const auto Rel = std::filesystem::relative(OutPath, ContentRoot, Ec);
    if (!Ec) {
      Saved.push_back(Rel.string());
    }
  }

  m_Server.m_WebSocketDispatch->BroadcastTextMessage(
      SerializeAssetList(CollectVisibleAssets()));

  const std::string Payload = BuildJson([&](JsonWriter &Writer) {
    Writer.StartObject();
    Writer.Key("type");
    Writer.String("assets_uploaded");
    Writer.Key("files");
    Writer.StartArray();
    for (const auto &SavedPath : Saved) {
      WriteJsonString(Writer, SavedPath);
    }
    Writer.EndArray();
    Writer.EndObject();
  });
  SendJsonResponse(ClientSocketValue, "200 OK", Payload);
  return false;
}
