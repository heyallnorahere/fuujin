#include <cstdint>
#include <stddef.h>

#if __has_include(<filesystem>)
#include <filesystem>
namespace fs = std::filesystem;
#else
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#endif

#include <optional>
#include <string>
#include <vector>
#include <unordered_map>
#include <fstream>

struct CommandLine {
    std::vector<std::string> Arguments;
    std::unordered_map<std::string, std::vector<std::string>> Options;
};

void ParseCommandLine(int argc, const char** argv, CommandLine& cli) {
    cli.Arguments.clear();
    cli.Options.clear();

    for (int i = 1; i < argc; i++) {
        std::string argument = argv[i];
        if (argument.empty()) {
            continue;
        }

        if (argument[0] == '-') {
            if (i == argc - 1) {
                throw std::runtime_error("Passed an option without any value!");
            }

            std::string value = argv[++i];
            cli.Options[argument].push_back(value);
            continue;
        }

        cli.Arguments.push_back(argument);
    }
}

struct Input {
    fs::path Output;
    std::vector<fs::path> InputFiles;

    std::optional<std::string> PCH;
    std::optional<std::string> Namespace;
    std::optional<std::string> FieldName;
    std::optional<fs::path> RelativeTo;
    std::optional<fs::path> InputList;
};

void ParseInput(const CommandLine& cli, Input& input) {
    input.Output = fs::absolute(cli.Arguments[0]);

    auto it = cli.Options.find("--pch");
    if (it != cli.Options.end()) {
        input.PCH = it->second[0];
    }

    it = cli.Options.find("--namespace");
    if (it != cli.Options.end()) {
        input.Namespace = it->second[0];
    }

    it = cli.Options.find("--relative");
    if (it != cli.Options.end()) {
        input.RelativeTo = fs::absolute(it->second[0]);
    }

    it = cli.Options.find("--field");
    if (it != cli.Options.end()) {
        input.FieldName = it->second[0];
    }

    it = cli.Options.find("--list");
    if (it != cli.Options.end()) {
        input.InputList = fs::absolute(it->second[0]);
    }

    if (cli.Options.contains("--input")) {
        const auto& inputFiles = cli.Options.at("--input");
        for (const auto& arg : inputFiles) {
            fs::path inputPath = arg;
            input.InputFiles.push_back(fs::absolute(inputPath));
        }
    }
}

static const std::vector<std::string> s_RequiredIncludes = {
    "cstdint",
    "vector",
    "unordered_map",
    "string",
};

int main(int argc, const char** argv) {
    CommandLine cli;
    ParseCommandLine(argc, argv, cli);

    Input input;
    ParseInput(cli, input);

    std::ofstream output(input.Output);
    if (input.PCH.has_value()) {
        output << "#include \"" << input.PCH.value() << "\"" << std::endl;
    } else {
        for (const auto& header : s_RequiredIncludes) {
            output << "#include <" << header << ">" << std::endl;
        }
    }

    output << std::endl;

    bool hasNamespace = false;
    if (input.Namespace.has_value()) {
        output << "namespace " << input.Namespace.value() << " {" << std::endl;
        hasNamespace = true;
    }

    if (hasNamespace) {
        output << '\t';
    }

    output << "const std::unordered_map<std::string, std::vector<uint8_t>> "
           << input.FieldName.value_or("g_EmbeddedData") << " = {" << std::endl;

    std::vector<fs::path> inputPaths;
    if (input.InputList.has_value()) {
        std::ifstream inputList(input.InputList.value());
        std::string line;

        while (std::getline(inputList, line)) {
            if (line.empty()) {
                continue;
            }

            inputPaths.push_back(fs::absolute(line));
        }
    } else {
        inputPaths = input.InputFiles;
    }

    for (const auto& path : inputPaths) {
        std::ifstream inputFile(path, std::ios::in | std::ios::binary);
        if (!inputFile.is_open()) {
            throw std::runtime_error("Failed to read data file \"" + path.string() + "\"");
        }

        inputFile.seekg(0, std::ios::end);
        auto length = inputFile.tellg();
        inputFile.seekg(0, std::ios::beg);

        std::vector<uint8_t> fileData((size_t)length);
        inputFile.read((char*)fileData.data(), (std::streamsize)fileData.size());
        inputFile.close();

        auto assetPath =
            input.RelativeTo.has_value() ? fs::relative(path, input.RelativeTo.value()) : path;

        static constexpr char systemSeparator = (char)fs::path::preferred_separator;
        static const std::string preferredSeparator = "/";

        auto key = assetPath.string();
        if (systemSeparator != preferredSeparator[0]) {
            size_t index = std::string::npos;
            while (true) {
                index = key.find(systemSeparator, index + 1);
                if (index != std::string::npos) {
                    key.replace(index, 1, preferredSeparator);
                } else {
                    break;
                }
            }
        }

        if (hasNamespace) {
            output << '\t';
        }

        output << "\t{ " << std::quoted(key) << ", { ";
        for (uint8_t byte : fileData) {
            output << "0x";
            output << std::hex << std::setfill('0') << std::setw(2) << (uint32_t)byte;
            output << ", ";
        }

        output << "} }," << std::endl;
    }

    if (hasNamespace) {
        output << '\t';
    }

    output << "};" << std::endl;
    if (hasNamespace) {
        output << "}" << std::endl;
    }

    output.clear();
}