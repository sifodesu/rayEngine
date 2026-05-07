#include "door.h"

#include "collisionRect.h"
#include "object_m.h"
#include "triggerButton.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <utility>

std::unordered_map<std::string, bool> Door::persistentOpen_;

namespace {

constexpr float cableTileSize = 8.0f;
constexpr float cableAnchorTolerance = 1.0f;

struct Cell {
    int x;
    int y;
};

using CellKey = std::uint64_t;

CellKey cellKey(Cell cell) {
    return (static_cast<CellKey>(static_cast<std::uint32_t>(cell.x)) << 32) |
           static_cast<std::uint32_t>(cell.y);
}

Cell cellFromKey(CellKey key) {
    return Cell{
        static_cast<std::int32_t>(key >> 32),
        static_cast<std::int32_t>(key & 0xffffffffu)
    };
}

Vector2 rectCenter(Rectangle rect) {
    return Vector2{rect.x + rect.width * 0.5f, rect.y + rect.height * 0.5f};
}

Vector2 cellCenter(Cell cell) {
    return Vector2{
        static_cast<float>(cell.x) * cableTileSize + cableTileSize * 0.5f,
        static_cast<float>(cell.y) * cableTileSize + cableTileSize * 0.5f
    };
}

Rectangle cellRect(Cell cell) {
    return Rectangle{
        static_cast<float>(cell.x) * cableTileSize,
        static_cast<float>(cell.y) * cableTileSize,
        cableTileSize,
        cableTileSize
    };
}

float clampFloat(float value, float minValue, float maxValue) {
    return std::max(minValue, std::min(value, maxValue));
}

float distanceSq(Vector2 a, Vector2 b) {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    return dx * dx + dy * dy;
}

float distance(Vector2 a, Vector2 b) {
    return std::sqrt(distanceSq(a, b));
}

float distanceToRect(Vector2 point, Rectangle rect) {
    float dx = std::max({rect.x - point.x, 0.0f, point.x - (rect.x + rect.width)});
    float dy = std::max({rect.y - point.y, 0.0f, point.y - (rect.y + rect.height)});
    return std::sqrt(dx * dx + dy * dy);
}

bool rectsTouchOrOverlap(Rectangle a, Rectangle b, float tolerance) {
    return a.x <= b.x + b.width + tolerance &&
           a.x + a.width + tolerance >= b.x &&
           a.y <= b.y + b.height + tolerance &&
           a.y + a.height + tolerance >= b.y;
}

bool rectsOverlapInterior(Rectangle a, Rectangle b) {
    return a.x < b.x + b.width &&
           a.x + a.width > b.x &&
           a.y < b.y + b.height &&
           a.y + a.height > b.y;
}

float jitterForCell(Cell cell, std::uint32_t salt) {
    std::uint32_t h = static_cast<std::uint32_t>(cell.x) * 374761393u ^
                      static_cast<std::uint32_t>(cell.y) * 668265263u ^
                      salt * 2246822519u;
    h ^= h >> 13;
    h *= 1274126177u;
    h ^= h >> 16;
    return static_cast<float>(static_cast<int>(h % 5u) - 2);
}

Vector2 cablePointInCell(Cell cell, float seed) {
    std::uint32_t salt = static_cast<std::uint32_t>(std::round(seed * 17.0f));
    Vector2 center = cellCenter(cell);
    return Vector2{
        center.x + jitterForCell(cell, salt),
        center.y + jitterForCell(cell, salt ^ 0x9e3779b9u)
    };
}

Vector2 cableAttachPointInCell(Cell cell, Rectangle targetRect, float seed) {
    Rectangle tile = cellRect(cell);
    Vector2 tileCenter = cellCenter(cell);
    Vector2 targetCenter = rectCenter(targetRect);
    std::uint32_t salt = static_cast<std::uint32_t>(std::round(seed * 29.0f));

    Vector2 point = cablePointInCell(cell, seed);
    float dx = targetCenter.x - tileCenter.x;
    float dy = targetCenter.y - tileCenter.y;

    if (std::abs(dx) >= std::abs(dy)) {
        point.x = dx >= 0.0f ? tile.x + tile.width : tile.x;
        point.y = clampFloat(targetCenter.y + jitterForCell(cell, salt),
                             tile.y + 1.0f,
                             tile.y + tile.height - 1.0f);
    } else {
        point.x = clampFloat(targetCenter.x + jitterForCell(cell, salt),
                             tile.x + 1.0f,
                             tile.x + tile.width - 1.0f);
        point.y = dy >= 0.0f ? tile.y + tile.height : tile.y;
    }

    return point;
}

void insertSolidCells(Rectangle rect, std::unordered_set<CellKey>& cells) {
    const float epsilon = 0.01f;
    int minX = static_cast<int>(std::floor(rect.x / cableTileSize));
    int maxX = static_cast<int>(std::floor((rect.x + rect.width - epsilon) / cableTileSize));
    int minY = static_cast<int>(std::floor(rect.y / cableTileSize));
    int maxY = static_cast<int>(std::floor((rect.y + rect.height - epsilon) / cableTileSize));

    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            cells.insert(cellKey(Cell{x, y}));
        }
    }
}

std::unordered_set<CellKey> collectSolidTileCells() {
    std::unordered_set<CellKey> cells;
    cells.reserve(Object_m::level_tiles_.size());

    for (const auto& [id, tile] : Object_m::level_tiles_) {
        (void)id;
        if (!tile) continue;

        CollisionRect* body = tile->getCollisionBody();
        if (!body || !body->isSolid()) continue;
        insertSolidCells(body->getSurface(), cells);
    }

    return cells;
}

std::vector<CellKey> findAnchorCells(Rectangle rect,
                                     const std::unordered_set<CellKey>& solidCells,
                                     bool allowInteriorOverlap) {
    std::vector<std::pair<float, CellKey>> candidates;
    if (solidCells.empty()) return {};

    Vector2 center = rectCenter(rect);
    for (CellKey key : solidCells) {
        Cell cell = cellFromKey(key);
        Rectangle tile = cellRect(cell);
        if (rectsTouchOrOverlap(rect, tile, cableAnchorTolerance) &&
            (allowInteriorOverlap || !rectsOverlapInterior(rect, tile))) {
            candidates.emplace_back(distanceSq(center, cellCenter(cell)), key);
        }
    }

    std::sort(candidates.begin(), candidates.end(), [](const auto& a, const auto& b) {
        if (a.first != b.first) return a.first < b.first;
        return a.second < b.second;
    });

    constexpr std::size_t maxAnchors = 32;
    std::vector<CellKey> anchors;
    anchors.reserve(std::min(maxAnchors, candidates.size()));
    for (const auto& candidate : candidates) {
        anchors.push_back(candidate.second);
        if (anchors.size() >= maxAnchors) break;
    }
    return anchors;
}

bool buildSolidTilePath(Rectangle fromRect,
                        Rectangle toRect,
                        const std::unordered_set<CellKey>& solidCells,
                        float seed,
                        std::vector<Vector2>& out) {
    std::vector<CellKey> startKeys = findAnchorCells(fromRect, solidCells, true);
    std::vector<CellKey> goalKeys = findAnchorCells(toRect, solidCells, false);
    if (startKeys.empty() || goalKeys.empty()) {
        return false;
    }

    std::unordered_set<CellKey> goalSet(goalKeys.begin(), goalKeys.end());

    struct SearchNode {
        float cost;
        CellKey key;
    };
    auto fartherFirst = [](const SearchNode& a, const SearchNode& b) {
        if (a.cost != b.cost) return a.cost > b.cost;
        return a.key > b.key;
    };
    std::priority_queue<SearchNode, std::vector<SearchNode>, decltype(fartherFirst)> pending(fartherFirst);
    std::unordered_map<CellKey, CellKey> previous;
    std::unordered_map<CellKey, float> bestCost;
    previous.reserve(solidCells.size());
    bestCost.reserve(solidCells.size());

    for (CellKey startKey : startKeys) {
        float startCost = distanceToRect(cellCenter(cellFromKey(startKey)), fromRect);
        auto it = bestCost.find(startKey);
        if (it != bestCost.end() && it->second <= startCost) continue;
        bestCost[startKey] = startCost;
        previous[startKey] = startKey;
        pending.push(SearchNode{startCost, startKey});
    }

    constexpr std::array<Cell, 4> dirs{
        Cell{1, 0},
        Cell{-1, 0},
        Cell{0, 1},
        Cell{0, -1}
    };

    CellKey bestGoal = 0;
    float bestGoalCost = std::numeric_limits<float>::infinity();
    while (!pending.empty()) {
        SearchNode currentNode = pending.top();
        CellKey currentKey = currentNode.key;
        Cell current = cellFromKey(currentKey);
        pending.pop();

        auto costIt = bestCost.find(currentKey);
        if (costIt == bestCost.end() || currentNode.cost > costIt->second + 0.001f) continue;
        if (currentNode.cost > bestGoalCost) break;

        if (goalSet.find(currentKey) != goalSet.end()) {
            float finalCost = currentNode.cost + distanceToRect(cellCenter(current), toRect);
            if (finalCost < bestGoalCost) {
                bestGoalCost = finalCost;
                bestGoal = currentKey;
            }
            continue;
        }

        for (Cell dir : dirs) {
            Cell next{current.x + dir.x, current.y + dir.y};
            CellKey nextKey = cellKey(next);
            if (solidCells.find(nextKey) == solidCells.end()) {
                continue;
            }

            float nextCost = currentNode.cost + cableTileSize;
            auto nextCostIt = bestCost.find(nextKey);
            if (nextCostIt != bestCost.end() && nextCostIt->second <= nextCost) {
                continue;
            }

            bestCost[nextKey] = nextCost;
            previous[nextKey] = currentKey;
            pending.push(SearchNode{nextCost, nextKey});
        }
    }

    if (!std::isfinite(bestGoalCost)) {
        return false;
    }

    std::vector<CellKey> reversedCells;
    for (CellKey key = bestGoal;; key = previous[key]) {
        reversedCells.push_back(key);
        if (key == previous[key]) break;
    }
    std::reverse(reversedCells.begin(), reversedCells.end());

    out.clear();
    out.reserve(reversedCells.size() + 2);
    out.push_back(cableAttachPointInCell(cellFromKey(reversedCells.front()), fromRect, seed));
    for (CellKey key : reversedCells) {
        out.push_back(cablePointInCell(cellFromKey(key), seed));
    }
    out.push_back(cableAttachPointInCell(cellFromKey(reversedCells.back()), toRect, seed + 7.0f));
    return true;
}

void drawCablePulse(const std::vector<Vector2>& points, float seed) {
    if (points.size() < 2) return;

    float totalLength = 0.0f;
    for (std::size_t i = 1; i < points.size(); ++i) {
        totalLength += distance(points[i - 1], points[i]);
    }
    if (totalLength <= 0.01f) return;

    float target = std::fmod(static_cast<float>(GetTime()) * 36.0f + seed, totalLength);
    for (std::size_t i = 1; i < points.size(); ++i) {
        Vector2 a = points[i - 1];
        Vector2 b = points[i];
        float segmentLength = distance(a, b);
        if (target > segmentLength) {
            target -= segmentLength;
            continue;
        }

        float t = segmentLength > 0.0f ? target / segmentLength : 0.0f;
        Vector2 pos{
            a.x + (b.x - a.x) * t,
            a.y + (b.y - a.y) * t
        };
        DrawPixelV(pos, Color{240, 255, 255, 255});
        return;
    }
}

void drawNeonCable(const std::vector<Vector2>& points, bool powered, float seed) {
    if (points.size() < 2) return;

    float pulse = 0.5f + 0.5f * std::sin(static_cast<float>(GetTime()) * 8.0f + seed);
    unsigned char outerAlpha = static_cast<unsigned char>(powered ? 60.0f + pulse * 45.0f : 28.0f);
    unsigned char midAlpha = static_cast<unsigned char>(powered ? 110.0f + pulse * 55.0f : 62.0f);
    unsigned char coreAlpha = static_cast<unsigned char>(powered ? 230.0f + pulse * 25.0f : 130.0f);

    BeginBlendMode(BLEND_ADDITIVE);
    for (std::size_t i = 1; i < points.size(); ++i) {
        DrawLineEx(points[i - 1], points[i], 1.0f, Color{0, 210, 255, outerAlpha});
    }
    for (std::size_t i = 1; i < points.size(); ++i) {
        DrawLineEx(points[i - 1], points[i], 1.0f, Color{0, 245, 255, midAlpha});
    }
    if (powered) {
        drawCablePulse(points, seed);
    }
    EndBlendMode();

    for (std::size_t i = 1; i < points.size(); ++i) {
        DrawLineEx(points[i - 1], points[i], 1.0f, Color{190, 255, 255, coreAlpha});
    }
}

} // namespace

Door::Door(const SpawnData& data)
    : BasicEnt(data)
    , buttonIds_(data.ldtk.targetIds.value_or(std::vector<std::string>{})) {
    registryId_ = data.ldtk.linkId.value_or(data.ldtk.iid.value_or(std::to_string(id_)));
    cablePaths_.reserve(buttonIds_.size());
    for (std::size_t i = 0; i < buttonIds_.size(); ++i) {
        cablePaths_.push_back(CablePath{
            buttonIds_[i],
            {},
            static_cast<float>((i + 1) * 13),
            false,
            false
        });
    }
    if (body_) body_->setSolid(true);
    auto it = persistentOpen_.find(registryId_);
    if (it != persistentOpen_.end() && it->second) {
        setOpen(true);
    }
}

void Door::routine() {
    BasicEnt::routine();
    if (!open_ && shouldOpen()) {
        setOpen(true);
    }
}

void Door::draw() {
    if (!body_) return;

    Rectangle rect = body_->getSurface();
    drawCables(rect);

    if (open_) {
        DrawRectangleLinesEx(rect, 1.0f, Color{112, 116, 104, 90});
        drawIndicators(rect);
        return;
    }

    DrawRectangleRec(rect, Color{24, 27, 24, 255});
    DrawRectangleLinesEx(rect, 1.0f, Color{128, 128, 108, 255});

    float midX = rect.x + rect.width * 0.5f;
    DrawLineV(Vector2{midX, rect.y + 1.0f}, Vector2{midX, rect.y + rect.height - 1.0f}, Color{72, 76, 68, 255});

    int slatCount = std::max(1, static_cast<int>(rect.height / 8.0f));
    for (int i = 1; i < slatCount; ++i) {
        float y = rect.y + i * 8.0f;
        if (y >= rect.y + rect.height) break;
        DrawLineV(Vector2{rect.x + 1.0f, y}, Vector2{rect.x + rect.width - 1.0f, y}, Color{72, 76, 68, 255});
    }

    drawIndicators(rect);
}

bool Door::shouldOpen() const {
    if (buttonIds_.empty()) return false;

    for (const std::string& buttonId : buttonIds_) {
        if (!TriggerButton::isButtonActivated(buttonId)) {
            return false;
        }
    }
    return true;
}

void Door::setOpen(bool open) {
    if (open_ == open) return;

    open_ = open;
    if (open_ && !registryId_.empty()) {
        persistentOpen_[registryId_] = true;
    }
    if (body_) {
        body_->setSolid(!open_);
    }
}

void Door::clearPersistentState() {
    persistentOpen_.clear();
}

bool Door::isIndicatorLit(std::size_t index) const {
    if (open_) return true;
    if (index >= buttonIds_.size()) return false;
    return TriggerButton::isButtonActivated(buttonIds_[index]);
}

void Door::ensureCableCache(Rectangle rect) {
    bool needsBuild = false;
    for (const CablePath& cable : cablePaths_) {
        if (!cable.attempted) {
            needsBuild = true;
            break;
        }
    }
    if (!needsBuild) return;

    std::unordered_set<CellKey> solidCells = collectSolidTileCells();
    if (solidCells.empty()) return;

    for (CablePath& cable : cablePaths_) {
        if (cable.attempted) continue;

        Rectangle buttonRect{};
        if (!TriggerButton::getButtonRect(cable.buttonId, buttonRect)) {
            continue;
        }

        cable.attempted = true;
        cable.hasPath = buildSolidTilePath(buttonRect, rect, solidCells, cable.seed, cable.points);
    }
}

void Door::drawCables(Rectangle rect) {
    ensureCableCache(rect);

    for (const CablePath& cable : cablePaths_) {
        if (!cable.hasPath) continue;

        bool powered = open_ || TriggerButton::isButtonActivated(cable.buttonId);
        drawNeonCable(cable.points, powered, cable.seed);
    }
}

void Door::drawIndicators(Rectangle rect) const {
    if (buttonIds_.empty()) return;

    float count = static_cast<float>(buttonIds_.size());
    float slotHeight = std::max(1.0f, (rect.height - 2.0f) / count);
    float lightSize = std::clamp(std::min(rect.width - 2.0f, slotHeight - 1.0f), 1.0f, 3.0f);
    float x = rect.x + rect.width - lightSize - 1.0f;

    for (std::size_t i = 0; i < buttonIds_.size(); ++i) {
        float y = rect.y + 1.0f + static_cast<float>(i) * slotHeight + std::max((slotHeight - lightSize) * 0.5f, 0.0f);
        Rectangle light{x, y, lightSize, lightSize};
        bool lit = isIndicatorLit(i);
        DrawRectangleRec(light, lit ? Color{255, 220, 76, 255} : Color{42, 46, 40, 255});
        DrawRectangleLinesEx(light, 1.0f, lit ? Color{255, 248, 180, 255} : Color{92, 96, 84, 255});
    }
}
