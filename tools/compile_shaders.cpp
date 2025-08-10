#include <cstdint>
#include <stddef.h>

#if __has_include(<filesystem>)
#include <filesystem>
namespace fs = std::filesystem;
#else
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#endif

#include <shaderc/shaderc.hpp>

#include <stdexcept>
#include <fstream>
#include <sstream>
#include <optional>
#include <unordered_map>
#include <iostream>

static const std::string s_StageDeclaration = "#stage ";
static const std::unordered_map<std::string, shaderc_shader_kind> s_StageMap = {
    { "vertex", shaderc_vertex_shader },
    { "fragment", shaderc_fragment_shader },
    { "geometry", shaderc_geometry_shader },
    { "compute", shaderc_compute_shader }
};

struct IncludeData {
    std::string Content, Path;
};

class Includer : public shaderc::CompileOptions::IncluderInterface {
    virtual shaderc_include_result* GetInclude(const char* requested_source,
                                               shaderc_include_type type,
                                               const char* requesting_source,
                                               size_t include_depth) override {
        fs::path included = requested_source;
        fs::path including = requesting_source;

        if (!included.is_absolute()) {
            fs::path directory = including.parent_path();
            included = directory / included;
        }

        std::stringstream content;
        std::ifstream file(included);

        if (file.is_open()) {
            std::string line;
            while (std::getline(file, line)) {
                content << line << std::endl;
            }
        }

        auto data = new IncludeData;
        data->Content = content.str();
        data->Path = file.is_open() ? included.string() : "";

        auto result = new shaderc_include_result;
        result->user_data = data;
        result->content = data->Content.c_str();
        result->content_length = data->Content.length();
        result->source_name = data->Path.c_str();
        result->source_name_length = data->Path.length();

        return result;
    }

    virtual void ReleaseInclude(shaderc_include_result* data) override {
        delete (IncludeData*)data->user_data;
        delete data;
    }
};

template <typename _Ty>
static bool CompileShader(const shaderc::Compiler& compiler, const shaderc::CompileOptions& options,
                          const fs::path& shader, const fs::path& outputDir, _Ty& outputStream) {
    std::ifstream input(shader);
    std::stringstream common;

    std::optional<shaderc_shader_kind> currentStage;
    std::unordered_map<shaderc_shader_kind, std::stringstream> stageSources;

    std::string line;
    while (std::getline(input, line)) {
        if (line.find(s_StageDeclaration) == 0) {
            auto stageName = line.substr(s_StageDeclaration.length());
            currentStage = s_StageMap.at(stageName);

            continue;
        }

        if (currentStage.has_value()) {
            auto stage = currentStage.value();
            stageSources[stage] << line << std::endl;
        } else {
            common << line << std::endl;
        }
    }

    if (stageSources.empty()) {
        throw std::runtime_error("No stages specified!");
    }

    auto shaderFilename = shader.filename();
    auto shaderDir = outputDir / shaderFilename;

    fs::create_directories(shaderDir);
    for (const auto& [stage, source] : stageSources) {
        std::string stageName;
        switch (stage) {
        case shaderc_vertex_shader:
            stageName = "Vertex";
            break;
        case shaderc_fragment_shader:
            stageName = "Fragment";
            break;
        case shaderc_geometry_shader:
            stageName = "Geometry";
            break;
        case shaderc_compute_shader:
            stageName = "Compute";
            break;
        default:
            throw std::runtime_error("Unknown shader stage");
        }

        std::string stageSource = common.str() + source.str();
        std::string path = shader.string();

        auto result = compiler.CompileGlslToSpv(stageSource, stage, path.c_str(), "main", options);
        if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
            auto message = result.GetErrorMessage();
            std::cerr << "Failed to compile shader " << path << ": " << message << std::endl;
            return false;
        }

        auto outputPath = shaderDir / (stageName + ".spv");
        std::vector<char> code((char*)result.begin(), (char*)result.end());
        std::ofstream output(outputPath, std::ios::out | std::ios::binary);
        output.write(code.data(), code.size());

        outputStream << outputPath.string() << std::endl;
    }

    return true;
}

int main(int argc, const char** argv) {
    fs::path outputDir = fs::absolute(argv[1]);

    shaderc::Compiler compiler;
    shaderc::CompileOptions options;

    options.SetGenerateDebugInfo();
    options.SetOptimizationLevel(shaderc_optimization_level_performance);
    options.SetTargetSpirv(shaderc_spirv_version_1_0);
    options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_0);
    options.SetIncluder(std::make_unique<Includer>());

    fs::create_directories(outputDir);
    std::ofstream outputList(outputDir / "outputs.txt");

    for (int i = 2; i < argc; i++) {
        fs::path shader = fs::absolute(argv[i]);
        if (!CompileShader(compiler, options, shader, outputDir, outputList)) {
            return 1;
        }
    }

    return 0;
}
