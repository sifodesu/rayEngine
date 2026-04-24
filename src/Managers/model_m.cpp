#include "model_m.h"

#include <algorithm>
#include <cctype>
#include <filesystem>

std::map<std::string, Model> Model_m::models_;
std::map<std::string, std::string> Model_m::basenameToKey_;

void Model_m::load(std::string path) {
    const std::filesystem::path root(path);
    std::error_code ec;
    if (!std::filesystem::exists(root, ec)) return;

    for (const auto& entry : std::filesystem::recursive_directory_iterator(root, ec)) {
        if (ec) break;

        if (!std::filesystem::is_regular_file(entry, ec)) continue;

        std::string ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });

        if (ext != ".obj" && ext != ".gltf" && ext != ".glb" && ext != ".iqm") continue;

        std::string key = std::filesystem::relative(entry.path(), root, ec).generic_string();
        if (ec || key.empty()) {
            key = entry.path().filename().string();
            ec.clear();
        }

        Model loaded = LoadModel(entry.path().string().c_str());

        if (auto it = models_.find(key); it != models_.end()) {
            UnloadModel(it->second);
            it->second = loaded;
        } else {
            models_.emplace(key, loaded);
        }

        const std::string base = entry.path().filename().string();
        auto aliasIt = basenameToKey_.find(base);
        if (aliasIt == basenameToKey_.end()) {
            basenameToKey_[base] = key;
        } else if (!aliasIt->second.empty() && aliasIt->second != key) {
            // Mark basename as ambiguous when multiple assets share it.
            aliasIt->second.clear();
        }
    }
}

void Model_m::unload() {
    for (auto& [name, model] : models_) {
        UnloadModel(model);
    }
    models_.clear();
    basenameToKey_.clear();
}

Model* Model_m::getModel(const std::string& filename) {
    auto normalize = [](std::string v) {
        std::replace(v.begin(), v.end(), '\\', '/');
        return v;
    };

    const std::string normalized = normalize(filename);
    if (auto it = models_.find(normalized); it != models_.end()) return &(it->second);

    auto aliasIt = basenameToKey_.find(std::filesystem::path(filename).filename().string());
    if (aliasIt == basenameToKey_.end() || aliasIt->second.empty()) return nullptr;

    auto modelIt = models_.find(aliasIt->second);
    if (modelIt == models_.end()) return nullptr;
    return &(modelIt->second);
}
