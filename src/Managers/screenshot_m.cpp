#include "screenshot_m.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>
#include <rlgl.h>

#include "definitions.h"

namespace {

using json = nlohmann::json;

constexpr int DEFAULT_CAPTURE_WIDTH = 3840;
constexpr int DEFAULT_CAPTURE_HEIGHT = 2160;
constexpr int MAX_CAPTURE_DIMENSION = 16384;
constexpr double SETTLE_SECONDS = 1.0;
constexpr int MINIMUM_FRAMES = 8;
constexpr int MAX_ATTEMPTS = 3;
constexpr const char* RAW_FILE = "00-game-raw.png";
constexpr const char* CRT_FILE = "01-game-crt.png";

struct Request {
    std::filesystem::path path;
    Screenshot_m::Stage stage{Screenshot_m::Stage::Final};
};

struct State {
    bool comparisonEnabled{false};
    bool configurationValid{true};
    bool comparisonQueued{false};
    bool comparisonFinished{false};
    bool comparisonFailed{false};
    bool parameterSnapshotCopied{false};
    std::filesystem::path comparisonDirectory{
        std::filesystem::path("screenshots") / "crt-comparison-latest"};
    int captureWidth{DEFAULT_CAPTURE_WIDTH};
    int captureHeight{DEFAULT_CAPTURE_HEIGHT};
    int renderedFrames{0};
    int attempts{0};
    double startedAt{0.0};
    std::vector<Request> requests;
    std::map<std::filesystem::path, bool> results;
    std::filesystem::path lastPath;
    bool lastSucceeded{false};
};

State& state() {
    static State instance;
    return instance;
}

const char* stageName(Screenshot_m::Stage stage) {
    switch (stage) {
        case Screenshot_m::Stage::BeforeDisplayShaders:
            return "before_display_shaders";
        case Screenshot_m::Stage::Final:
            return "final";
    }
    return "unknown";
}

std::string sanitizeLabel(std::string label) {
    for (char& character : label) {
        const unsigned char value = static_cast<unsigned char>(character);
        if (!std::isalnum(value) && character != '-' && character != '_') {
            character = '_';
        }
    }
    return label.empty() ? "game" : label;
}

std::filesystem::path timestampedPath(const std::string& label) {
    const auto now = std::chrono::system_clock::now();
    const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    const std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm local{};
#if defined(_WIN32)
    localtime_s(&local, &time);
#else
    localtime_r(&time, &local);
#endif

    std::ostringstream name;
    name << std::put_time(&local, "%Y%m%d-%H%M%S") << '-'
         << std::setfill('0') << std::setw(3) << milliseconds.count() << '-'
         << sanitizeLabel(label) << ".png";
    return std::filesystem::path("screenshots") / name.str();
}

void queueRequest(std::filesystem::path path, Screenshot_m::Stage stage) {
    if (path.extension() != ".png") path.replace_extension(".png");
    state().requests.push_back({std::move(path), stage});
}

bool parseCaptureSize(const std::string& dimensions, int& width, int& height) {
    const std::size_t separator = dimensions.find_first_of("xX");
    if (separator == std::string::npos) return false;

    try {
        std::size_t widthCharacters = 0;
        std::size_t heightCharacters = 0;
        const int parsedWidth = std::stoi(
            dimensions.substr(0, separator), &widthCharacters);
        const int parsedHeight = std::stoi(
            dimensions.substr(separator + 1), &heightCharacters);
        if (widthCharacters != separator ||
            heightCharacters != dimensions.size() - separator - 1 ||
            parsedWidth <= 0 || parsedHeight <= 0 ||
            parsedWidth > MAX_CAPTURE_DIMENSION ||
            parsedHeight > MAX_CAPTURE_DIMENSION) {
            return false;
        }
        width = parsedWidth;
        height = parsedHeight;
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool isRegularFile(const std::filesystem::path& path) {
    std::error_code error;
    return std::filesystem::is_regular_file(path, error) && !error;
}

bool requestSucceeded(const std::filesystem::path& path) {
    const auto result = state().results.find(path);
    return result != state().results.end() && result->second;
}

bool writeManifest(bool complete) {
    const State& current = state();
    const json manifest = {
        {"schema", "rayengine.crt-comparison.v1"},
        {"complete", complete && current.parameterSnapshotCopied},
        {"overlayFree", true},
        {"sameSourceFrame", true},
        {"captureWidth", current.captureWidth},
        {"captureHeight", current.captureHeight},
        {"sourceWidth", NATIVE_RES_WIDTH},
        {"sourceHeight", NATIVE_RES_HEIGHT},
        {"settleSeconds", SETTLE_SECONDS},
        {"minimumFrames", MINIMUM_FRAMES},
        {"parameterSnapshot", current.parameterSnapshotCopied
            ? json("crt_params.json") : json(nullptr)},
        {"entries", json::array({
            {
                {"file", RAW_FILE},
                {"stage", "before_display_shaders"},
                {"crtEnabled", false},
                {"captured", isRegularFile(
                    current.comparisonDirectory / RAW_FILE)}
            },
            {
                {"file", CRT_FILE},
                {"stage", "final"},
                {"crtEnabled", true},
                {"captured", isRegularFile(
                    current.comparisonDirectory / CRT_FILE)}
            }
        })}
    };

    std::ofstream file(
        current.comparisonDirectory / "manifest.json", std::ios::trunc);
    if (!file.good()) return false;
    file << manifest.dump(2);
    return file.good();
}

void removePreviousComparisonFiles() {
    static constexpr std::array<const char*, 6> FILES = {
        RAW_FILE,
        CRT_FILE,
        "00-game-raw.json",
        "01-game-crt.json",
        "crt_params.json",
        "manifest.json",
    };

    std::error_code error;
    for (const char* file : FILES) {
        error.clear();
        std::filesystem::remove(state().comparisonDirectory / file, error);
    }
}

bool exportRequest(
    const Request& request, Texture2D output, bool crtApplied) {
    State& current = state();
    const std::filesystem::path& path = request.path;
    std::error_code error;
    if (!path.parent_path().empty()) {
        std::filesystem::create_directories(path.parent_path(), error);
    }

    current.lastPath = path;
    current.lastSucceeded = false;
    current.results[path] = false;
    if (error) {
        TraceLog(LOG_WARNING, "Screenshot: cannot create '%s' (%s)",
            path.parent_path().string().c_str(), error.message().c_str());
        return false;
    }

    Image framebuffer = LoadImageFromTexture(output);
    if (framebuffer.data) {
        ImageFlipVertical(&framebuffer);
        ImageFormat(&framebuffer, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    }
    const bool imageExported = framebuffer.data &&
        ExportImage(framebuffer, path.string().c_str());
    if (framebuffer.data) UnloadImage(framebuffer);

    const auto capturedAt = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    const char* capturePoint =
        request.stage == Screenshot_m::Stage::BeforeDisplayShaders
            ? "post-processing before display shaders, FPS and ImGui"
            : "final post-processing before FPS and ImGui";
    const bool hasParameterSnapshot = isRegularFile(
        path.parent_path() / "crt_params.json");
    const json metadata = {
        {"schema", "rayengine.screenshot.v1"},
        {"image", path.filename().string()},
        {"capturedAtUnixMs", capturedAt},
        {"width", output.width},
        {"height", output.height},
        {"previewWidth", GetScreenWidth()},
        {"previewHeight", GetScreenHeight()},
        {"captureStage", stageName(request.stage)},
        {"capturePoint", capturePoint},
        {"crtEnabled", crtApplied},
        {"overlayFree", true},
        {"renderLayout", {
            {"sourceWidth", NATIVE_RES_WIDTH},
            {"sourceHeight", NATIVE_RES_HEIGHT},
            {"outputOverride", current.comparisonEnabled}
        }},
        {"parameterSnapshot", hasParameterSnapshot
            ? json("crt_params.json") : json(nullptr)}
    };

    std::filesystem::path metadataPath = path;
    metadataPath.replace_extension(".json");
    std::ofstream metadataFile(metadataPath, std::ios::trunc);
    if (metadataFile.good()) metadataFile << metadata.dump(2);
    const bool metadataExported = metadataFile.good();

    current.lastSucceeded = imageExported && metadataExported &&
        isRegularFile(path);
    current.results[path] = current.lastSucceeded;
    if (!current.lastSucceeded) {
        TraceLog(LOG_WARNING, "Screenshot: failed to write '%s'",
            path.string().c_str());
    } else {
        TraceLog(LOG_INFO, "Screenshot: saved '%s'", path.string().c_str());
    }
    return current.lastSucceeded;
}

} // namespace

void Screenshot_m::configure(int argc, char** argv) {
    State& current = state();
    current = State{};

    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv && argv[index] ? argv[index] : "";
        if (argument == "--capture-crt-comparison") {
            current.comparisonEnabled = true;
            if (index + 1 < argc && argv[index + 1] &&
                std::string(argv[index + 1]).rfind("--", 0) != 0) {
                current.comparisonDirectory = argv[++index];
            }
            continue;
        }

        if (argument != "--capture-size") continue;
        if (index + 1 >= argc || !argv[index + 1]) {
            std::cerr << "Missing value after --capture-size; expected "
                         "WIDTHxHEIGHT\n";
            current.configurationValid = false;
            continue;
        }

        const std::string dimensions = argv[++index];
        if (!parseCaptureSize(
                dimensions, current.captureWidth, current.captureHeight)) {
            std::cerr << "Invalid --capture-size '" << dimensions
                      << "'; expected WIDTHxHEIGHT (maximum "
                      << MAX_CAPTURE_DIMENSION << 'x'
                      << MAX_CAPTURE_DIMENSION << ")\n";
            current.configurationValid = false;
        }
    }
}

void Screenshot_m::initialize() {
    State& current = state();
    if (!current.comparisonEnabled) return;
    if (!current.configurationValid) {
        current.comparisonFailed = true;
        current.comparisonFinished = true;
        return;
    }

    std::error_code error;
    std::filesystem::create_directories(
        current.comparisonDirectory, error);
    if (error) {
        std::cerr << "CRT comparison: cannot create "
                  << current.comparisonDirectory << ": "
                  << error.message() << '\n';
        current.comparisonFailed = true;
        current.comparisonFinished = true;
        return;
    }

    removePreviousComparisonFiles();
    error.clear();
    current.parameterSnapshotCopied = std::filesystem::copy_file(
        "crt_params.json",
        current.comparisonDirectory / "crt_params.json",
        std::filesystem::copy_options::overwrite_existing,
        error);
    if (!current.parameterSnapshotCopied || error) {
        std::cerr << "CRT comparison: parameter snapshot failed";
        if (error) std::cerr << " (" << error.message() << ')';
        std::cerr << '\n';
    }

    current.startedAt = GetTime();
    std::cout << "CRT comparison: "
              << std::filesystem::absolute(current.comparisonDirectory)
              << " (" << current.captureWidth << 'x'
              << current.captureHeight << ")\n";
}

void Screenshot_m::shutdown() {
    state().requests.clear();
}

Screenshot_m::OutputSize Screenshot_m::outputSize(
    int fallbackWidth, int fallbackHeight) {
    const State& current = state();
    if (current.comparisonEnabled && current.configurationValid) {
        return {current.captureWidth, current.captureHeight};
    }
    return {std::max(fallbackWidth, 1), std::max(fallbackHeight, 1)};
}

void Screenshot_m::request(const std::string& label) {
    queueRequest(timestampedPath(label), Stage::Final);
}

const std::filesystem::path& Screenshot_m::lastPath() {
    return state().lastPath;
}

bool Screenshot_m::lastSucceeded() {
    return state().lastSucceeded;
}

void Screenshot_m::beginFrame() {
    State& current = state();
    if (!current.comparisonEnabled || current.comparisonFinished ||
        current.comparisonQueued) {
        return;
    }
    if (GetTime() - current.startedAt < SETTLE_SECONDS ||
        current.renderedFrames < MINIMUM_FRAMES) {
        return;
    }

    queueRequest(
        current.comparisonDirectory / RAW_FILE,
        Stage::BeforeDisplayShaders);
    queueRequest(current.comparisonDirectory / CRT_FILE, Stage::Final);
    current.comparisonQueued = true;
}

void Screenshot_m::capture(Stage stage, Texture2D output, bool crtApplied) {
    State& current = state();
    const bool hasRequests = std::any_of(
        current.requests.begin(), current.requests.end(),
        [stage](const Request& request) { return request.stage == stage; });
    if (!hasRequests) return;

    rlDrawRenderBatchActive();
    std::vector<Request> remaining;
    remaining.reserve(current.requests.size());
    for (Request& request : current.requests) {
        if (request.stage != stage) {
            remaining.push_back(std::move(request));
            continue;
        }
        exportRequest(request, output, crtApplied);
    }
    current.requests = std::move(remaining);
}

bool Screenshot_m::endFrame() {
    State& current = state();
    if (!current.comparisonEnabled) return false;
    ++current.renderedFrames;
    if (current.comparisonFinished || !current.comparisonQueued) {
        return current.comparisonFinished;
    }

    const std::filesystem::path rawPath =
        current.comparisonDirectory / RAW_FILE;
    const std::filesystem::path crtPath =
        current.comparisonDirectory / CRT_FILE;
    const bool imagesSucceeded = requestSucceeded(rawPath) &&
        requestSucceeded(crtPath);

    if (imagesSucceeded) {
        const bool manifestSucceeded = writeManifest(true);
        current.comparisonFailed = !current.parameterSnapshotCopied ||
            !manifestSucceeded;
        current.comparisonFinished = true;
        if (current.comparisonFailed) {
            std::cerr << "CRT comparison images were written, but its "
                         "metadata is incomplete\n";
        } else {
            std::cout << "CRT comparison complete: "
                      << std::filesystem::absolute(
                          current.comparisonDirectory)
                      << '\n';
        }
        return true;
    }

    ++current.attempts;
    current.comparisonQueued = false;
    if (current.attempts < MAX_ATTEMPTS) return false;

    std::cerr << "CRT comparison failed after " << MAX_ATTEMPTS
              << " attempts\n";
    writeManifest(false);
    current.comparisonFailed = true;
    current.comparisonFinished = true;
    return true;
}

bool Screenshot_m::shouldClose() {
    const State& current = state();
    return current.comparisonEnabled && current.comparisonFinished;
}

int Screenshot_m::exitCode() {
    return state().comparisonFailed ? 1 : 0;
}
