#include "wpch.h"
#include "OpenGL/OpenGLShader.h"

#include <fstream>
#include <filesystem>
#include <glad/glad.h>

#include <glm/gtc/type_ptr.hpp>

#include <shaderc/shaderc.hpp>

#include <spirv_cross/spirv_cross.hpp>
#include <spirv_cross/spirv_glsl.hpp>

#include "Core/Timer.h"

namespace Wraith {
	namespace Utils {
		static GLenum ShaderTypeFromString(const std::string& type) {
			if (type == "vertex") return GL_VERTEX_SHADER;
			if (type == "fragment" || type == "pixel") return GL_FRAGMENT_SHADER;

			W_CORE_ASSERT(false, "Unknown shader type!");
			return 0;
		}

		static shaderc_shader_kind GLShaderStageToShaderC(GLenum stage) {
			switch (stage) {
				case GL_VERTEX_SHADER:		return shaderc_glsl_vertex_shader;
				case GL_FRAGMENT_SHADER:	return shaderc_glsl_fragment_shader;
			}
			W_CORE_ASSERT(false);
			return (shaderc_shader_kind)0;
		}

		static const char* GLShaderStageToString(GLenum stage) {
			switch (stage) {
				case GL_VERTEX_SHADER:		return "GL_VERTEX_SHADER";
				case GL_FRAGMENT_SHADER:	return "GL_FRAGMENT_SHADER";
			}
			W_CORE_ASSERT(false);
			return nullptr;
		}

		static const char* GetCacheDirectory() {
			// TODO: Make sure the assets directory is valid
			return "Content/Cache/Shader/OpenGL";
		}

		static void CreateCacheDirectoryIfNeeded() {
			std::string cacheDirectory = GetCacheDirectory();
			if (!std::filesystem::exists(cacheDirectory)) {
				std::filesystem::create_directories(cacheDirectory);
			}
		}

		static const char* GLShaderStageCachedOpenGLFileExtension(uint32_t stage) {
			switch (stage) {
				case GL_VERTEX_SHADER:		return ".cached_opengl.vert";
				case GL_FRAGMENT_SHADER:	return ".cached_opengl.frag";
			}
			W_CORE_ASSERT(false);
			return "";
		}

		static const char* GLShaderStageCachedVulkanFileExtension(uint32_t stage) {
			switch (stage) {
				case GL_VERTEX_SHADER:		return ".cached_vulkan.vert";
				case GL_FRAGMENT_SHADER:	return ".cached_vulkan.frag";
			}
			W_CORE_ASSERT(false);
			return "";
		}
	}

	OpenGLShader::OpenGLShader(const std::string& filePath)
			: m_FilePath(filePath) {
		W_PROFILE_FUNCTION();

		Utils::CreateCacheDirectoryIfNeeded();

		std::string source = ReadFile(filePath);
		auto shaderSources = PreProcess(source);

		{
			Timer timer;
			CompileOrGetVulkanBinaries(shaderSources);
			CompileOrGetOpenGLBinaries();
			CreateProgram();
			W_CORE_WARN("Shader creation took {0} ms", timer.ElapsedMillis());
		}

		// Extract name from filepath
		auto lastSlash = filePath.find_last_of("/\\");
		lastSlash = lastSlash == std::string::npos ? 0 : lastSlash + 1;
		auto lastDot = filePath.rfind('.');
		auto count = lastDot == std::string::npos ? filePath.size() - lastSlash : lastDot - lastSlash;
		m_Name = filePath.substr(lastSlash, count);
	}

	OpenGLShader::OpenGLShader(const std::string& name, const std::string& vertexSrc, const std::string& fragmentSrc)
		: m_Name(name) {
		W_PROFILE_FUNCTION();

		std::unordered_map<GLenum, std::string> sources;
		sources[GL_VERTEX_SHADER] = vertexSrc;
		sources[GL_FRAGMENT_SHADER] = fragmentSrc;

		CompileOrGetVulkanBinaries(sources);
		CompileOrGetOpenGLBinaries();
		CreateProgram();
	}

	OpenGLShader::~OpenGLShader() {
		W_PROFILE_FUNCTION();

		glDeleteProgram(m_RendererID);
	}

	std::unordered_map<GLenum, std::string> OpenGLShader::PreProcess(const std::string& source) {
		W_PROFILE_FUNCTION();

		std::unordered_map<GLenum, std::string> shaderSources;

		const char* typeToken = "#type";
		size_t typeTokenLength = strlen(typeToken);
		size_t pos = source.find(typeToken, 0);
		while (pos != std::string::npos) {
			size_t eol = source.find_first_of("\r\n", pos);
			W_CORE_ASSERT(eol != std::string::npos, "Syntax error");
			size_t begin = pos + typeTokenLength + 1;
			std::string type = source.substr(begin, eol - begin);
			W_CORE_ASSERT(Utils::ShaderTypeFromString(type), "Invalid shader type specified");

			size_t nextLinePos = source.find_first_not_of("\r\n", eol);
			W_CORE_ASSERT(nextLinePos != std::string::npos, "Syntax error");
			pos = source.find(typeToken, nextLinePos);

			shaderSources[Utils::ShaderTypeFromString(type)] = (pos == std::string::npos) ? source.substr(nextLinePos) : source.substr(nextLinePos, pos - nextLinePos);
		}

		return shaderSources;
	}

	void OpenGLShader::CompileOrGetVulkanBinaries(const std::unordered_map<GLenum, std::string>& shaderSources) {
		GLuint program = glCreateProgram();

		shaderc::Compiler compiler;
		shaderc::CompileOptions options;
		options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_2);
		const bool optimize = true;
		if (optimize) {
			options.SetOptimizationLevel(shaderc_optimization_level_performance);
		}

		std::filesystem::path cacheDirectory = Utils::GetCacheDirectory();

		auto& shaderData = m_VulkanSPIRV;
		shaderData.clear();
		for (auto&& [stage, source] : shaderSources) {
			std::filesystem::path shaderFilePath = m_FilePath;
			std::filesystem::path cachedPath = cacheDirectory / (shaderFilePath.filename().string() + Utils::GLShaderStageCachedVulkanFileExtension(stage));

			std::ifstream in(cachedPath, std::ios::in | std::ios::binary);
			if (in.is_open()) {
				W_CORE_WARN("Loading cached Vulkan SPIRV for stage {0}", Utils::GLShaderStageToString(stage));

				in.seekg(0, std::ios::end);
				auto size = in.tellg();
				in.seekg(0, std::ios::beg);

				auto& data = shaderData[stage];
				data.Resize(size / sizeof(uint32_t));
				in.read((char*)data.GetData(), size);

				W_CORE_WARN("Loaded {0} uint32_t values from cache", data.Size());
				if (data.Size() > 0) {
					W_CORE_WARN("First few values: {0:x}, {1:x}, {2:x}",
						data.Size() > 0 ? data[0] : 0,
						data.Size() > 1 ? data[1] : 0,
						data.Size() > 2 ? data[2] : 0);
				}
			}
			else {
				W_CORE_WARN("Compiling fresh Vulkan SPIRV for stage {0}", Utils::GLShaderStageToString(stage));
				W_CORE_WARN("Source length: {0}", source.length());

				shaderc::SpvCompilationResult module = compiler.CompileGlslToSpv(source, Utils::GLShaderStageToShaderC(stage), m_FilePath.c_str(), options);
				if (module.GetCompilationStatus() != shaderc_compilation_status_success) {
					W_CORE_ERROR("Vulkan compilation failed: {0}", module.GetErrorMessage());
					W_CORE_ASSERT(false);
				}

				auto moduleSize = std::distance(module.cbegin(), module.cend());
				W_CORE_WARN("Compilation successful, SPIRV size: {0} uint32_t values", moduleSize);

				shaderData[stage] = Array<uint32_t>(module.cbegin(), module.cend());

				W_CORE_WARN("Array size after construction: {0}", shaderData[stage].Size());
				if (shaderData[stage].Size() > 0) {
					W_CORE_WARN("First few values: {0:x}, {1:x}, {2:x}",
						shaderData[stage][0],
						shaderData[stage].Size() > 1 ? shaderData[stage][1] : 0,
						shaderData[stage].Size() > 2 ? shaderData[stage][2] : 0);
				}

				// Save to cache...
				std::ofstream out(cachedPath, std::ios::out | std::ios::binary);
				if (out.is_open()) {
					auto& data = shaderData[stage];
					out.write((char*)data.GetData(), data.Size() * sizeof(uint32_t));
					out.flush();
					out.close();
					W_CORE_WARN("Saved {0} bytes to cache", data.Size() * sizeof(uint32_t));
				}
			}
		}

		W_CORE_WARN("About to reflect {0} shader stages", shaderData.size());
		for (auto&& [stage, data] : shaderData) {
			W_CORE_WARN("Stage {0} has {1} uint32_t values", Utils::GLShaderStageToString(stage), data.Size());
			Reflect(stage, data);
		}
	}

	void OpenGLShader::CompileOrGetOpenGLBinaries() {
		auto& shaderData = m_OpenGLSPIRV;

		shaderc::Compiler compiler;
		shaderc::CompileOptions options;
		options.SetTargetEnvironment(shaderc_target_env_opengl, shaderc_env_version_opengl_4_5);
		const bool optimize = false;
		if (optimize) {
			options.SetOptimizationLevel(shaderc_optimization_level_performance);
		}

		std::filesystem::path cacheDirectory = Utils::GetCacheDirectory();

		shaderData.clear();
		m_OpenGLSourceCode.clear();

		for (auto&& [stage, spirv] : m_VulkanSPIRV) {
			std::filesystem::path shaderFilePath = m_FilePath;
			std::filesystem::path cachedPath = cacheDirectory / (shaderFilePath.filename().string() + Utils::GLShaderStageCachedOpenGLFileExtension(stage));

			std::ifstream in(cachedPath, std::ios::in | std::ios::binary);
			if (in.is_open()) {
				W_CORE_WARN("Loading cached OpenGL SPIRV for stage {0}", Utils::GLShaderStageToString(stage));

				in.seekg(0, std::ios::end);
				auto size = in.tellg();
				in.seekg(0, std::ios::beg);

				if (size > 0 && size % sizeof(uint32_t) == 0) {
					// Create a temporary vector to load the data
					std::vector<uint32_t> tempData(size / sizeof(uint32_t));
					in.read((char*)tempData.data(), size);

					// Now create the Array from the vector data
					auto& data = shaderData[stage];
					data = Array<uint32_t>(tempData.begin(), tempData.end());

					W_CORE_WARN("Loaded {0} uint32_t values from OpenGL cache", data.Size());

					// Verify the data was loaded correctly
					if (data.Size() > 0) {
						W_CORE_WARN("First few OpenGL SPIRV values: {0:x}, {1:x}, {2:x}",
							data[0],
							data.Size() > 1 ? data[1] : 0,
							data.Size() > 2 ? data[2] : 0);
					}
				}
				else {
					W_CORE_ERROR("Invalid cached file size: {0} bytes", size);
					in.close();
					// Fall through to regenerate
					goto regenerate_opengl_spirv;
				}
			}
			else {
			regenerate_opengl_spirv:
				W_CORE_WARN("Converting SPIRV to GLSL for stage {0}", Utils::GLShaderStageToString(stage));

				// Convert Vulkan SPIRV to GLSL
				auto spirvVector = spirv.ToVector();
				W_CORE_WARN("ToVector() produced {0} values for SPIRV-Cross", spirvVector.size());

				spirv_cross::CompilerGLSL glslCompiler(spirvVector);
				m_OpenGLSourceCode[stage] = glslCompiler.compile();
				auto& source = m_OpenGLSourceCode[stage];

				W_CORE_WARN("Generated GLSL source length: {0}", source.length());

				// Compile GLSL to OpenGL SPIRV
				shaderc::SpvCompilationResult module = compiler.CompileGlslToSpv(source, Utils::GLShaderStageToShaderC(stage), m_FilePath.c_str());
				if (module.GetCompilationStatus() != shaderc_compilation_status_success) {
					W_CORE_ERROR("OpenGL GLSL compilation failed: {0}", module.GetErrorMessage());
					W_CORE_ASSERT(false);
				}

				// Create Array from compilation result
				shaderData[stage] = Array<uint32_t>(module.cbegin(), module.cend());
				W_CORE_WARN("OpenGL SPIRV compilation produced {0} values", shaderData[stage].Size());

				// Save to cache using a temporary vector for safety
				std::ofstream out(cachedPath, std::ios::out | std::ios::binary);
				if (out.is_open()) {
					auto& data = shaderData[stage];
					// Use GetData() but verify it's not null
					if (data.GetData() && data.Size() > 0) {
						out.write((char*)data.GetData(), data.Size() * sizeof(uint32_t));
						out.flush();
						out.close();
						W_CORE_WARN("Saved {0} bytes to OpenGL cache", data.Size() * sizeof(uint32_t));
					}
					else {
						W_CORE_ERROR("Cannot save to cache: data is null or empty");
					}
				}
			}
		}

		W_CORE_WARN("Final OpenGL SPIRV data has {0} stages", shaderData.size());
		for (auto&& [stage, data] : shaderData) {
			W_CORE_WARN("Stage {0}: {1} values", Utils::GLShaderStageToString(stage), data.Size());
		}
	}

	void OpenGLShader::CreateProgram() {
		// Check OpenGL context and capabilities first
		W_CORE_WARN("OpenGL Version: {0}", (char*)glGetString(GL_VERSION));
		W_CORE_WARN("OpenGL Vendor: {0}", (char*)glGetString(GL_VENDOR));
		W_CORE_WARN("OpenGL Renderer: {0}", (char*)glGetString(GL_RENDERER));

		// Check if SPIR-V is supported
		GLint numExtensions;
		glGetIntegerv(GL_NUM_EXTENSIONS, &numExtensions);
		bool spirvSupported = false;
		bool gl45Supported = false;

		for (int i = 0; i < numExtensions; i++) {
			const char* extension = (const char*)glGetStringi(GL_EXTENSIONS, i);
			if (strcmp(extension, "GL_ARB_gl_spirv") == 0) {
				spirvSupported = true;
			}
			if (strcmp(extension, "GL_ARB_ES2_compatibility") == 0) {
				gl45Supported = true;
			}
		}

		W_CORE_WARN("SPIR-V Support (GL_ARB_gl_spirv): {0}", spirvSupported ? "YES" : "NO");

		GLint majorVersion, minorVersion;
		glGetIntegerv(GL_MAJOR_VERSION, &majorVersion);
		glGetIntegerv(GL_MINOR_VERSION, &minorVersion);
		W_CORE_WARN("OpenGL Version: {0}.{1}", majorVersion, minorVersion);

		if (majorVersion < 4 || (majorVersion == 4 && minorVersion < 5)) {
			W_CORE_ERROR("OpenGL 4.5 or higher required for SPIR-V support! Current: {0}.{1}", majorVersion, minorVersion);
		}

		if (!spirvSupported) {
			W_CORE_ERROR("GL_ARB_gl_spirv extension not supported!");
		}

		// Clear any existing OpenGL errors before we start
		while (glGetError() != GL_NO_ERROR) {
			// Clear error queue
		}

		GLuint program = glCreateProgram();
		if (program == 0) {
			W_CORE_ERROR("glCreateProgram failed!");
			GLenum error = glGetError();
			W_CORE_ERROR("OpenGL Error after glCreateProgram: {0}", error);
			return;
		}

		W_CORE_WARN("Created program object: {0}", program);

		Array<GLuint> shaderIDs;

		W_CORE_WARN("Creating OpenGL program with {0} shader stages", m_OpenGLSPIRV.size());

		for (auto&& [stage, spirv] : m_OpenGLSPIRV) {
			W_CORE_WARN("Creating shader for stage {0} with {1} SPIRV bytes",
				Utils::GLShaderStageToString(stage),
				spirv.Size() * sizeof(uint32_t));

			// Clear errors before shader creation
			while (glGetError() != GL_NO_ERROR) {}

			GLuint shaderID = glCreateShader(stage);
			GLenum createError = glGetError();

			if (shaderID == 0) {
				W_CORE_ERROR("glCreateShader failed for stage {0}", Utils::GLShaderStageToString(stage));
				W_CORE_ERROR("OpenGL Error: {0}", createError);

				// Try to understand why it failed
				switch (createError) {
				case GL_INVALID_ENUM:
					W_CORE_ERROR("GL_INVALID_ENUM: Invalid shader type {0}", stage);
					break;
				case GL_OUT_OF_MEMORY:
					W_CORE_ERROR("GL_OUT_OF_MEMORY: Not enough memory to create shader");
					break;
				default:
					W_CORE_ERROR("Unknown OpenGL error: {0}", createError);
					break;
				}
				continue;
			}

			W_CORE_WARN("Created shader object: {0} for stage {1}", shaderID, Utils::GLShaderStageToString(stage));
			shaderIDs.Emplace(shaderID);

			// Verify SPIRV data before passing to OpenGL
			if (spirv.GetData() == nullptr) {
				W_CORE_ERROR("SPIRV data is null for stage {0}", Utils::GLShaderStageToString(stage));
				continue;
			}

			if (spirv.Size() == 0) {
				W_CORE_ERROR("SPIRV data is empty for stage {0}", Utils::GLShaderStageToString(stage));
				continue;
			}

			// Check SPIRV magic number (should be 0x07230203)
			if (spirv.Size() > 0 && spirv[0] != 0x07230203) {
				W_CORE_ERROR("Invalid SPIRV magic number: {0:x}, expected 0x07230203", spirv[0]);
			}
			else {
				W_CORE_WARN("SPIRV magic number correct: {0:x}", spirv[0]);
			}

			// Clear errors before shader binary call
			while (glGetError() != GL_NO_ERROR) {}

			glShaderBinary(1, &shaderID, GL_SHADER_BINARY_FORMAT_SPIR_V, spirv.GetData(), spirv.Size() * sizeof(uint32_t));
			GLenum binaryError = glGetError();

			if (binaryError != GL_NO_ERROR) {
				W_CORE_ERROR("glShaderBinary failed for stage {0}, error: {1}", Utils::GLShaderStageToString(stage), binaryError);

				switch (binaryError) {
				case GL_INVALID_OPERATION:
					W_CORE_ERROR("GL_INVALID_OPERATION: Shader object not created properly or binary format not supported");
					break;
				case GL_INVALID_VALUE:
					W_CORE_ERROR("GL_INVALID_VALUE: Invalid count, shaders, binaryFormat, or binary data");
					break;
				default:
					W_CORE_ERROR("Other OpenGL error: {0}", binaryError);
					break;
				}
			}

			// Check shader binary compilation status
			GLint binaryStatus;
			glGetShaderiv(shaderID, GL_COMPILE_STATUS, &binaryStatus);
			if (binaryStatus == GL_FALSE) {
				GLint logLength;
				glGetShaderiv(shaderID, GL_INFO_LOG_LENGTH, &logLength);
				if (logLength > 0) {
					std::vector<GLchar> log(logLength);
					glGetShaderInfoLog(shaderID, logLength, nullptr, log.data());
					W_CORE_ERROR("Shader binary failed for stage {0}: {1}", Utils::GLShaderStageToString(stage), log.data());
				}
			}

			// Clear errors before specialization
			while (glGetError() != GL_NO_ERROR) {}

			glSpecializeShader(shaderID, "main", 0, nullptr, nullptr);
			GLenum specError = glGetError();

			if (specError != GL_NO_ERROR) {
				W_CORE_ERROR("glSpecializeShader failed for stage {0}, error: {1}", Utils::GLShaderStageToString(stage), specError);
			}

			// Check specialization status
			glGetShaderiv(shaderID, GL_COMPILE_STATUS, &binaryStatus);
			if (binaryStatus == GL_FALSE) {
				GLint logLength;
				glGetShaderiv(shaderID, GL_INFO_LOG_LENGTH, &logLength);
				if (logLength > 0) {
					std::vector<GLchar> log(logLength);
					glGetShaderInfoLog(shaderID, logLength, nullptr, log.data());
					W_CORE_ERROR("Shader specialization failed for stage {0}: {1}", Utils::GLShaderStageToString(stage), log.data());
				}
			}
			else {
				W_CORE_WARN("Shader specialization successful for stage {0}", Utils::GLShaderStageToString(stage));
			}

			glAttachShader(program, shaderID);
			GLenum attachError = glGetError();
			if (attachError != GL_NO_ERROR) {
				W_CORE_ERROR("glAttachShader failed, error: {0}", attachError);
			}
		}

		W_CORE_WARN("Linking program with {0} shaders", shaderIDs.Size());

		// Clear errors before linking
		while (glGetError() != GL_NO_ERROR) {}

		glLinkProgram(program);
		GLenum linkError = glGetError();

		if (linkError != GL_NO_ERROR) {
			W_CORE_ERROR("glLinkProgram failed, error: {0}", linkError);
		}

		GLint isLinked;
		glGetProgramiv(program, GL_LINK_STATUS, &isLinked);
		if (isLinked == GL_FALSE) {
			GLint maxLength;
			glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);

			std::vector<GLchar> infoLog(maxLength);
			glGetProgramInfoLog(program, maxLength, &maxLength, infoLog.data());
			W_CORE_ERROR("Shader linking failed ({0}):\n{1}", m_FilePath, infoLog.data());

			glDeleteProgram(program);

			for (auto id : shaderIDs)
				glDeleteShader(id);
		}
		else {
			W_CORE_WARN("Program linking successful!");
		}

		for (auto id : shaderIDs) {
			glDetachShader(program, id);
			glDeleteShader(id);
		}

		m_RendererID = program;
	}

	void OpenGLShader::Reflect(GLenum stage, const Array<uint32_t>& shaderData) {
		spirv_cross::Compiler compiler(shaderData.ToVector());
		spirv_cross::ShaderResources resources = compiler.get_shader_resources();

		W_CORE_TRACE("OpenGLShader::Reflect - {0} {1}", Utils::GLShaderStageToString(stage), m_FilePath);
		W_CORE_TRACE("	{0} uniform buffers", resources.uniform_buffers.size());
		W_CORE_TRACE("	{0} resources", resources.sampled_images.size());

		W_CORE_TRACE("Uniform buffers:");
		for (const auto& resource : resources.uniform_buffers) {
			const auto& bufferType = compiler.get_type(resource.base_type_id);
			uint32_t bufferSize = compiler.get_declared_struct_size(bufferType);
			uint32_t binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
			int memberCount = bufferType.member_types.size();

			W_CORE_TRACE("  {0}", resource.name);
			W_CORE_TRACE("    Size = {0}", bufferSize);
			W_CORE_TRACE("    Binding = {0}", binding);
			W_CORE_TRACE("    Members = {0}", memberCount);
		}
	}

	std::string OpenGLShader::ReadFile(const std::string& filePath) {
		W_PROFILE_FUNCTION();

		std::string result;
		std::ifstream in(filePath, std::ios::in | std::ios::binary); // ifstream closes itself due to RAII
		if (in) {
			in.seekg(0, std::ios::end);
			size_t size = in.tellg();
			if (size != -1) {
				result.resize(size);
				in.seekg(0, std::ios::beg);
				in.read(&result[0], size);
			}
			else {
				W_CORE_ERROR("Could not read from file '{0}'", filePath);
			}
		}
		else {
			W_CORE_ERROR("Could not open file '{0}'", filePath);
		}

		return result;
	}

	void OpenGLShader::Bind() const {
		W_PROFILE_FUNCTION();

		glUseProgram(m_RendererID);
	}

	void OpenGLShader::Unbind() const {
		W_PROFILE_FUNCTION();

		glUseProgram(0);
	}

	void OpenGLShader::SetIntArray(const std::string& name, int* values, uint32_t count) {
		W_PROFILE_FUNCTION();

		UploadUniformIntArray(name, values, count);
	}

	void OpenGLShader::SetInt(const std::string& name, int value) {
		W_PROFILE_FUNCTION();

		UploadUniformInt(name, value);
	}

	void OpenGLShader::SetFloat(const std::string& name, float value) {
		W_PROFILE_FUNCTION();

		UploadUniformFloat(name, value);
	}

	void OpenGLShader::SetFloat2(const std::string& name, const glm::vec2& value) {
		W_PROFILE_FUNCTION();

		UploadUniformFloat2(name, value);
	}

	void OpenGLShader::SetFloat3(const std::string& name, const glm::vec3& value) {
		W_PROFILE_FUNCTION();

		UploadUniformFloat3(name, value);
	}

	void OpenGLShader::SetFloat4(const std::string& name, const glm::vec4& value) {
		W_PROFILE_FUNCTION();

		UploadUniformFloat4(name, value);
	}

	void OpenGLShader::SetMat4(const std::string& name, const glm::mat4& value) {
		W_PROFILE_FUNCTION();

		UploadUniformMat4(name, value);
	}

	void OpenGLShader::UploadUniformIntArray(const std::string& name, int* values, uint32_t count) {
		GLint location = glGetUniformLocation(m_RendererID, name.c_str());
		glUniform1iv(location, count, values);
	}

	void OpenGLShader::UploadUniformInt(const std::string& name, int value) {
		GLint location = glGetUniformLocation(m_RendererID, name.c_str());
		glUniform1i(location, value);
	}

	void OpenGLShader::UploadUniformFloat(const std::string& name, float value) {
		GLint location = glGetUniformLocation(m_RendererID, name.c_str());
		glUniform1f(location, value);
	}

	void OpenGLShader::UploadUniformFloat2(const std::string& name, const glm::vec2& values) {
		GLint location = glGetUniformLocation(m_RendererID, name.c_str());
		glUniform2f(location, values.x, values.y);
	}

	void OpenGLShader::UploadUniformFloat3(const std::string& name, const glm::vec3& values) {
		GLint location = glGetUniformLocation(m_RendererID, name.c_str());
		glUniform3f(location, values.x, values.y, values.z);
	}

	void OpenGLShader::UploadUniformFloat4(const std::string& name, const glm::vec4& values) {
		GLint location = glGetUniformLocation(m_RendererID, name.c_str());
		glUniform4f(location, values.x, values.y, values.z, values.w);
	}

	void OpenGLShader::UploadUniformMat3(const std::string& name, const glm::mat3& matrix) {
		GLint location = glGetUniformLocation(m_RendererID, name.c_str());
		glUniformMatrix3fv(location, 1, GL_FALSE, glm::value_ptr(matrix));
	}

	void OpenGLShader::UploadUniformMat4(const std::string& name, const glm::mat4& matrix) {
		GLint location = glGetUniformLocation(m_RendererID, name.c_str());
		glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(matrix));
	}
}
