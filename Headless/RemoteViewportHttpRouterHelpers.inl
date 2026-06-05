namespace {
constexpr std::string_view ClientIdHeaderName = "X-Axiom-Client-Id";

std::string BuildHttpResponse(std::string_view Status,
                              std::string_view ContentType,
                              std::string_view Body,
                              std::string_view ExtraHeaders = {}) {
  std::ostringstream Stream;
  Stream << "HTTP/1.1 " << Status << "\r\n"
         << "Content-Type: " << ContentType << "\r\n"
         << "Content-Length: " << Body.size() << "\r\n"
         << "Cache-Control: no-store\r\n"
         << "Connection: close\r\n"
         << "Access-Control-Allow-Origin: *\r\n";
  if (!ExtraHeaders.empty()) {
    Stream << ExtraHeaders;
  }
  Stream << "\r\n" << Body;
  return Stream.str();
}

std::string Trim(std::string_view Value) {
  while (!Value.empty() &&
         std::isspace(static_cast<unsigned char>(Value.front())) != 0) {
    Value.remove_prefix(1);
  }
  while (!Value.empty() &&
         std::isspace(static_cast<unsigned char>(Value.back())) != 0) {
    Value.remove_suffix(1);
  }
  return std::string(Value);
}

bool EqualsCaseInsensitive(std::string_view Left, std::string_view Right) {
  if (Left.size() != Right.size()) {
    return false;
  }
  for (size_t Index = 0; Index < Left.size(); ++Index) {
    if (std::tolower(static_cast<unsigned char>(Left[Index])) !=
        std::tolower(static_cast<unsigned char>(Right[Index]))) {
      return false;
    }
  }
  return true;
}

struct ParsedHttpResponse {
  std::string Status;
  std::vector<std::pair<std::string, std::string>> Headers;
  std::string Body;
};

std::optional<ParsedHttpResponse> ParseHttpResponseText(std::string_view Response) {
  const size_t HeaderEnd = Response.find("\r\n\r\n");
  if (HeaderEnd == std::string_view::npos) {
    return std::nullopt;
  }
  const std::string_view HeaderBlock = Response.substr(0, HeaderEnd);
  const size_t StatusLineEnd = HeaderBlock.find("\r\n");
  if (StatusLineEnd == std::string_view::npos) {
    return std::nullopt;
  }
  const std::string_view StatusLine = HeaderBlock.substr(0, StatusLineEnd);
  const size_t FirstSpace = StatusLine.find(' ');
  if (FirstSpace == std::string_view::npos || FirstSpace + 1 >= StatusLine.size()) {
    return std::nullopt;
  }

  ParsedHttpResponse Parsed{};
  Parsed.Status = std::string(StatusLine.substr(FirstSpace + 1));
  Parsed.Body = std::string(Response.substr(HeaderEnd + 4));

  size_t LineStart = StatusLineEnd + 2;
  while (LineStart < HeaderBlock.size()) {
    const size_t LineEnd = HeaderBlock.find("\r\n", LineStart);
    const std::string_view Line = HeaderBlock.substr(
        LineStart, (LineEnd == std::string_view::npos ? HeaderBlock.size() : LineEnd) -
                       LineStart);
    const size_t Colon = Line.find(':');
    if (Colon != std::string_view::npos) {
      Parsed.Headers.emplace_back(std::string(Trim(Line.substr(0, Colon))),
                                  std::string(Trim(Line.substr(Colon + 1))));
    }
    if (LineEnd == std::string_view::npos) {
      break;
    }
    LineStart = LineEnd + 2;
  }

  return Parsed;
}

std::string_view StripQuery(std::string_view Path) {
  const size_t Query = Path.find('?');
  return Query == std::string_view::npos ? Path : Path.substr(0, Query);
}

std::string UrlDecode(std::string_view Input) {
  std::string Out;
  Out.reserve(Input.size());
  for (size_t I = 0; I < Input.size(); ++I) {
    if (Input[I] == '%' && I + 2 < Input.size()) {
      const char Hi = Input[I + 1];
      const char Lo = Input[I + 2];
      auto HexVal = [](char C) -> int {
        if (C >= '0' && C <= '9') return C - '0';
        if (C >= 'a' && C <= 'f') return C - 'a' + 10;
        if (C >= 'A' && C <= 'F') return C - 'A' + 10;
        return -1;
      };
      const int H = HexVal(Hi);
      const int L = HexVal(Lo);
      if (H >= 0 && L >= 0) {
        Out += static_cast<char>(H * 16 + L);
        I += 2;
        continue;
      }
    } else if (Input[I] == '+') {
      Out += ' ';
      continue;
    }
    Out += Input[I];
  }
  return Out;
}

std::optional<std::string> GetQueryParam(std::string_view Path,
                                         std::string_view Key) {
  const size_t Q = Path.find('?');
  if (Q == std::string_view::npos) {
    return std::nullopt;
  }
  std::string_view Query = Path.substr(Q + 1);
  while (!Query.empty()) {
    const size_t Amp = Query.find('&');
    std::string_view Pair =
        Amp == std::string_view::npos ? Query : Query.substr(0, Amp);
    const size_t Eq = Pair.find('=');
    if (Eq != std::string_view::npos && Pair.substr(0, Eq) == Key) {
      return UrlDecode(Pair.substr(Eq + 1));
    }
    if (Amp == std::string_view::npos) {
      break;
    }
    Query.remove_prefix(Amp + 1);
  }
  return std::nullopt;
}

std::optional<std::string> FindHeaderValue(std::string_view HeaderBlock,
                                           std::string_view HeaderName) {
  size_t LineStart = 0;
  while (LineStart < HeaderBlock.size()) {
    const size_t LineEnd = HeaderBlock.find("\r\n", LineStart);
    const std::string_view Line =
        HeaderBlock.substr(LineStart, LineEnd == std::string_view::npos
                                          ? std::string_view::npos
                                          : LineEnd - LineStart);
    const size_t Colon = Line.find(':');
    if (Colon != std::string_view::npos &&
        EqualsCaseInsensitive(Trim(Line.substr(0, Colon)), HeaderName)) {
      return Trim(Line.substr(Colon + 1));
    }
    if (LineEnd == std::string_view::npos) {
      break;
    }
    LineStart = LineEnd + 2;
  }
  return std::nullopt;
}

std::string JsonResponse(std::string_view Status, std::string_view Payload) {
  return BuildHttpResponse(Status, "application/json; charset=utf-8", Payload);
}

using JsonWriter = rapidjson::Writer<rapidjson::StringBuffer>;

void WriteJsonString(JsonWriter &Writer, std::string_view Value) {
  Writer.String(Value.data(), static_cast<rapidjson::SizeType>(Value.size()));
}

template <typename Fn> std::string BuildJson(Fn &&FnWriter) {
  rapidjson::StringBuffer Buffer;
  JsonWriter Writer(Buffer);
  FnWriter(Writer);
  return std::string(Buffer.GetString(), Buffer.GetSize());
}

std::string SerializeTypeOnlyJson(std::string_view Type) {
  return BuildJson([&](JsonWriter &Writer) {
    Writer.StartObject();
    Writer.Key("type");
    WriteJsonString(Writer, Type);
    Writer.EndObject();
  });
}

std::optional<std::string> ExtractJsonStringField(std::string_view Body,
                                                  std::string_view FieldName) {
  std::string MutableBody(Body);
  rapidjson::Document Document;
  Document.ParseInsitu<rapidjson::kParseStopWhenDoneFlag>(MutableBody.data());
  if (Document.HasParseError() || !Document.IsObject()) {
    return std::nullopt;
  }
  const auto It = Document.FindMember(std::string(FieldName).c_str());
  if (It == Document.MemberEnd() || !It->value.IsString()) {
    return std::nullopt;
  }
  return std::string(It->value.GetString(), It->value.GetStringLength());
}

void WriteProjectJson(JsonWriter &Writer,
                      const Project::ProjectDescriptor &Project) {
  Writer.StartObject();
  Writer.Key("projectId");
  WriteJsonString(Writer, Project.Manifest.ProjectId);
  Writer.Key("name");
  WriteJsonString(Writer, Project.Manifest.Name);
  Writer.Key("slug");
  WriteJsonString(Writer, Project.Manifest.Slug);
  Writer.Key("rootPath");
  WriteJsonString(Writer, Project.Root.RootPath.string());
  Writer.Key("contentDir");
  WriteJsonString(Writer, Project.Root.ContentDir.string());
  Writer.Key("scriptsDir");
  WriteJsonString(Writer, Project.ScriptWorkspace.ScriptsDir.string());
  Writer.Key("scriptProjectPath");
  WriteJsonString(Writer, Project.ScriptWorkspace.ScriptProjectPath.string());
  Writer.Key("scriptSolutionPath");
  WriteJsonString(Writer, Project.ScriptWorkspace.ScriptSolutionPath.string());
  Writer.Key("scriptAssemblyName");
  WriteJsonString(Writer, Project.ScriptWorkspace.AssemblyName);
  Writer.Key("scriptRootNamespace");
  WriteJsonString(Writer, Project.ScriptWorkspace.RootNamespace);
  Writer.Key("starterScriptPath");
  WriteJsonString(Writer, Project.ScriptWorkspace.StarterScriptPath.string());
  Writer.Key("starterScriptClassName");
  WriteJsonString(Writer, Project.ScriptWorkspace.StarterScriptClassName);
  Writer.Key("starterScriptQualifiedClassName");
  WriteJsonString(Writer,
                  Project.ScriptWorkspace.StarterScriptQualifiedClassName);
  Writer.Key("cookedDir");
  WriteJsonString(Writer, Project.Output.CookedDir.string());
  Writer.Key("cookManifestPath");
  WriteJsonString(Writer, Project.Output.CookManifestPath.string());
  Writer.Key("buildDir");
  WriteJsonString(Writer, Project.Output.BuildDir.string());
  Writer.Key("packageDir");
  WriteJsonString(Writer, Project.Output.PackageDir.string());
  Writer.Key("packagedContentDir");
  WriteJsonString(Writer, Project.Output.PackagedContentDir.string());
  Writer.Key("packagedCookedDir");
  WriteJsonString(Writer, Project.Output.PackagedCookedDir.string());
  Writer.Key("packagedSceneAssetPath");
  WriteJsonString(Writer, Project.Output.PackagedSceneAssetPath.string());
  Writer.Key("stagedRuntimeBinaryPath");
  WriteJsonString(Writer, Project.Output.StagedRuntimeBinaryPath.string());
  Writer.Key("packageManifestPath");
  WriteJsonString(Writer, Project.Output.PackageManifestPath.string());
  Writer.Key("engineContentDir");
  WriteJsonString(
      Writer, (std::filesystem::path(AXIOM_CONTENT_DIR) / "Engine").string());
  Writer.Key("sceneFilePath");
  WriteJsonString(Writer, Project.Root.SceneFilePath.string());
  Writer.EndObject();
}

std::string SerializeProjectList(
    const std::vector<Project::ProjectDescriptor> &Projects,
    const std::optional<Project::ProjectDescriptor> &ActiveProject) {
  return BuildJson([&](JsonWriter &Writer) {
    Writer.StartObject();
    Writer.Key("type");
    Writer.String("projects");
    Writer.Key("activeProjectSlug");
    if (ActiveProject.has_value()) {
      WriteJsonString(Writer, ActiveProject->Manifest.Slug);
    } else {
      Writer.Null();
    }
    Writer.Key("projects");
    Writer.StartArray();
    for (const auto &Project : Projects) {
      WriteProjectJson(Writer, Project);
    }
    Writer.EndArray();
    Writer.EndObject();
  });
}

std::string SerializeCurrentProject(
    const std::optional<Project::ProjectDescriptor> &ActiveProject) {
  return BuildJson([&](JsonWriter &Writer) {
    Writer.StartObject();
    Writer.Key("type");
    Writer.String("current_project");
    Writer.Key("project");
    if (ActiveProject.has_value()) {
      WriteProjectJson(Writer, *ActiveProject);
    } else {
      Writer.Null();
    }
    Writer.EndObject();
  });
}

std::string SerializeProjectCookResult(
    const Project::ProjectDescriptor &Project,
    const Project::ProjectCookResult &Result) {
  return BuildJson([&](JsonWriter &Writer) {
    Writer.StartObject();
    Writer.Key("type");
    Writer.String("project_cooked");
    Writer.Key("project");
    WriteProjectJson(Writer, Project);
    Writer.Key("cookedSourceAssetCount");
    Writer.Uint64(Result.CookedSourceAssetCount);
    Writer.Key("manifestEntryCount");
    Writer.Uint64(Result.ManifestEntryCount);
    Writer.Key("cookManifestPath");
    WriteJsonString(Writer, Result.Output.CookManifestPath.string());
    Writer.EndObject();
  });
}

std::string SerializeProjectPackageResult(
    const Project::ProjectDescriptor &Project,
    const Project::ProjectPackageResult &Result) {
  return BuildJson([&](JsonWriter &Writer) {
    Writer.StartObject();
    Writer.Key("type");
    Writer.String("project_packaged");
    Writer.Key("project");
    WriteProjectJson(Writer, Project);
    Writer.Key("cookedSourceAssetCount");
    Writer.Uint64(Result.Cook.CookedSourceAssetCount);
    Writer.Key("manifestEntryCount");
    Writer.Uint64(Result.Cook.ManifestEntryCount);
    Writer.Key("packagedFileCount");
    Writer.Uint64(Result.PackagedFileCount);
    Writer.Key("includedSceneAsset");
    Writer.Bool(Result.IncludedSceneAsset);
    Writer.Key("includedEngineContent");
    Writer.Bool(Result.IncludedEngineContent);
    Writer.Key("includedRuntimeBinary");
    Writer.Bool(Result.IncludedRuntimeBinary);
    Writer.Key("sceneAssetPath");
    WriteJsonString(Writer, Result.SceneAssetPath.string());
    Writer.Key("runtimeBinaryPath");
    WriteJsonString(Writer, Result.RuntimeBinaryPath.string());
    Writer.Key("packagedContentPath");
    WriteJsonString(Writer, Result.Cook.Output.PackagedContentDir.string());
    Writer.Key("packageDir");
    WriteJsonString(Writer, Result.Cook.Output.PackageDir.string());
    Writer.Key("packageManifestPath");
    WriteJsonString(Writer, Result.Cook.Output.PackageManifestPath.string());
    Writer.EndObject();
  });
}

bool IsValidScriptRelativePath(std::filesystem::path RelativePath) {
  RelativePath = RelativePath.lexically_normal();
  if (RelativePath.empty() || RelativePath.is_absolute()) {
    return false;
  }
  if (RelativePath.filename().empty() || RelativePath.extension() != ".cs") {
    return false;
  }
  for (const auto &Part : RelativePath) {
    const auto Token = Part.string();
    if (Token.empty() || Token == "." || Token == "..") {
      return false;
    }
  }
  return true;
}

std::string SerializeScriptListJson(const std::vector<std::string> &Files) {
  return BuildJson([&](JsonWriter &Writer) {
    Writer.StartObject();
    Writer.Key("type");
    Writer.String("scripts_list");
    Writer.Key("files");
    Writer.StartArray();
    for (const auto &File : Files) {
      WriteJsonString(Writer, File);
    }
    Writer.EndArray();
    Writer.EndObject();
  });
}

std::string SerializeScriptFileJson(std::string_view RelativePath,
                                    std::string_view Content) {
  return BuildJson([&](JsonWriter &Writer) {
    Writer.StartObject();
    Writer.Key("type");
    Writer.String("script_file");
    Writer.Key("path");
    WriteJsonString(Writer, RelativePath);
    Writer.Key("content");
    WriteJsonString(Writer, Content);
    Writer.EndObject();
  });
}

std::string SerializeScriptMutationJson(std::string_view MutationType,
                                        std::string_view RelativePath) {
  return BuildJson([&](JsonWriter &Writer) {
    Writer.StartObject();
    Writer.Key("type");
    WriteJsonString(Writer, MutationType);
    Writer.Key("path");
    WriteJsonString(Writer, RelativePath);
    Writer.EndObject();
  });
}

std::string SerializeScriptClassesJson(
    const std::vector<std::pair<std::string, std::string>> &Classes) {
  return BuildJson([&](JsonWriter &Writer) {
    Writer.StartObject();
    Writer.Key("type");
    Writer.String("script_classes");
    Writer.Key("classes");
    Writer.StartArray();
    for (const auto &Entry : Classes) {
      Writer.StartObject();
      Writer.Key("className");
      WriteJsonString(Writer, Entry.first);
      Writer.Key("path");
      WriteJsonString(Writer, Entry.second);
      Writer.EndObject();
    }
    Writer.EndArray();
    Writer.EndObject();
  });
}

std::vector<uint8_t> MakeThumbnailJpeg(const std::filesystem::path &Path,
                                       int MaxDim = 128) {
  int W = 0, H = 0, Channels = 0;
  stbi_uc *Pixels =
      stbi_load(Path.string().c_str(), &W, &H, &Channels, STBI_rgb);
  if (!Pixels || W <= 0 || H <= 0) {
    return {};
  }

  int ThumbW = W;
  int ThumbH = H;
  if (W > MaxDim || H > MaxDim) {
    if (W >= H) {
      ThumbW = MaxDim;
      ThumbH = std::max(1, H * MaxDim / W);
    } else {
      ThumbH = MaxDim;
      ThumbW = std::max(1, W * MaxDim / H);
    }
  }

  std::vector<uint8_t> Scaled(static_cast<size_t>(ThumbW * ThumbH * 3));
  for (int Y = 0; Y < ThumbH; ++Y) {
    for (int X = 0; X < ThumbW; ++X) {
      const int SrcX = X * W / ThumbW;
      const int SrcY = Y * H / ThumbH;
      const stbi_uc *Src = Pixels + (SrcY * W + SrcX) * 3;
      uint8_t *Dst = Scaled.data() + (Y * ThumbW + X) * 3;
      Dst[0] = Src[0];
      Dst[1] = Src[1];
      Dst[2] = Src[2];
    }
  }
  stbi_image_free(Pixels);

  std::vector<uint8_t> JpegBytes;
  stbi_write_jpg_to_func(
      [](void *Ctx, void *Data, int Size) {
        auto *Out = static_cast<std::vector<uint8_t> *>(Ctx);
        const uint8_t *Bytes = static_cast<const uint8_t *>(Data);
        Out->insert(Out->end(), Bytes, Bytes + Size);
      },
      &JpegBytes, ThumbW, ThumbH, 3, Scaled.data(), 85);
  return JpegBytes;
}
} // namespace
