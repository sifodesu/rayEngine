#pragma once

#include <cassert>
#include <algorithm>
#include <memory>
#include <type_traits>
#include <vector>
#include <functional>
#include <iostream>
#include "Box.h"
#include "raylib.h"

using namespace quadtree;

namespace quadtree {
    struct quadNode {
        int id;
        Rectangle rect;
        bool operator==(const quadNode& rhs) const {
            return (id == rhs.id) && ((abs(rect.width - rhs.rect.width) < 0.000000001) && (abs(rect.height - rhs.rect.height) < 0.000000001)
                && (abs(rect.x - rhs.rect.x) < 0.000000001) && (abs(rect.y - rhs.rect.y) < 0.000000001));
        }
    };
    // Box<float> titi(quadNode node) {
    //     return Box<float>(node.rect.x, node.rect.y, node.rect.width, node.rect.height);
    // }

    class Quadtree {
        static_assert(std::is_convertible_v<std::invoke_result_t<Box<float>(quadNode node), const quadNode&>, Box<float>>,
            "Box<float>(quadNode node) must be a callable of signature Box<float>(const quadNode&)");
        static_assert(std::is_convertible_v<std::invoke_result_t<std::equal_to<quadNode>, const quadNode&, const quadNode&>, bool>,
            "std::equal_to<quadNode> must be a callable of signature bool(const quadNode&, const quadNode&)");
        static_assert(std::is_arithmetic_v<float>);

    public:
        Quadtree(const Box<float>& box = Box(-(float)INT_MAX / 2, -(float)INT_MAX / 2,
            (float)INT_MAX, (float)INT_MAX),
            const std::equal_to<quadNode>& equal = std::equal_to<quadNode>()) :
            mBox(box), mRoot(std::make_unique<Node>()), mEqual(equal) {
            auto titi = [](quadNode node) {
                return Box<float>(node.rect.x, node.rect.y, node.rect.width, node.rect.height);
            };
            mGetBox = titi;
        }

        void add(const quadNode& value) {
            add(mRoot.get(), 0, mBox, value);
        }

        void remove(const quadNode& value) {
            remove(mRoot.get(), nullptr, mBox, value);
        }

        std::vector<quadNode> query(const Box<float>& box) const {
            auto values = std::vector<quadNode>();
            query(mRoot.get(), mBox, box, values);
            return values;
        }

        std::vector<std::pair<quadNode, quadNode>> findAllIntersections() const {
            auto intersections = std::vector<std::pair<quadNode, quadNode>>();
            findAllIntersections(mRoot.get(), intersections);
            return intersections;
        }

    private:
        static constexpr auto quadNodehreshold = std::size_t(8);
        static constexpr auto MaxDepth = std::size_t(64);

        struct Node {
            std::array<std::unique_ptr<Node>, 4> children;
            std::vector<quadNode> values;
        };

        Box<float> mBox;
        std::unique_ptr<Node> mRoot;
        std::function<Box<float>(quadNode node)> mGetBox;
        std::equal_to<quadNode> mEqual;

        bool isLeaf(const Node* node) const {
            return !static_cast<bool>(node->children[0]);
        }

        Box<float> computeBox(const Box<float>& box, int i) const {
            auto origin = box.getTopLeft();
            auto childSize = box.getSize() / static_cast<float>(2);
            switch (i) {
                // North West
            case 0:
            return Box<float>(origin, childSize);
            // Norst East
            case 1:
            return Box<float>(quadtreeVector2::Vector2<float>(origin.x + childSize.x, origin.y), childSize);
            // South West
            case 2:
            return Box<float>(quadtreeVector2::Vector2<float>(origin.x, origin.y + childSize.y), childSize);
            // South East
            case 3:
            return Box<float>(origin + childSize, childSize);
            default:
            assert(false && "Invalid child index");
            return Box<float>();
            }
        }

        int getQuadrant(const Box<float>& nodeBox, const Box<float>& valueBox) const {
            auto center = nodeBox.getCenter();
            // West
            if (valueBox.getRight() < center.x) {
                // North West
                if (valueBox.getBottom() < center.y)
                    return 0;
                // South West
                else if (valueBox.top >= center.y)
                    return 2;
                // Not contained in any quadrant
                else
                    return -1;
            }
            // East
            else if (valueBox.left >= center.x) {
                // North East
                if (valueBox.getBottom() < center.y)
                    return 1;
                // South East
                else if (valueBox.top >= center.y)
                    return 3;
                // Not contained in any quadrant
                else
                    return -1;
            }
            // Not contained in any quadrant
            else
                return -1;
        }

        void add(Node* node, std::size_t depth, const Box<float>& box, const quadNode& value) {
            assert(node != nullptr);
            if (!box.contains(mGetBox(value))){
                std::cout << "Out of bounds box" << std::endl;
                return;
            }
            if (isLeaf(node)) {
                // Insert the value in this node if possible
                if (depth >= MaxDepth || node->values.size() < quadNodehreshold)
                    node->values.push_back(value);
                // Otherwise, we split and we try again
                else {
                    split(node, box);
                    add(node, depth, box, value);
                }
            }
            else {
                auto i = getQuadrant(box, mGetBox(value));
                // Add the value in a child if the value is entirely contained in it
                if (i != -1)
                    add(node->children[static_cast<std::size_t>(i)].get(), depth + 1, computeBox(box, i), value);
                // Otherwise, we add the value in the current node
                else
                    node->values.push_back(value);
            }
        }

        void split(Node* node, const Box<float>& box) {
            assert(node != nullptr);
            assert(isLeaf(node) && "Only leaves can be split");
            // Create children
            for (auto& child : node->children)
                child = std::make_unique<Node>();
            // Assign values to children
            auto newValues = std::vector<quadNode>(); // New values for this node
            for (const auto& value : node->values) {
                auto i = getQuadrant(box, mGetBox(value));
                if (i != -1)
                    node->children[static_cast<std::size_t>(i)]->values.push_back(value);
                else
                    newValues.push_back(value);
            }
            node->values = std::move(newValues);
        }

        void remove(Node* node, Node* parent, const Box<float>& box, const quadNode& value) {
            assert(node != nullptr);
            if (!box.contains(mGetBox(value))) {
                std::cout << "Out of bounds box" << std::endl;
                return;
            }
            if (isLeaf(node)) {
                // Remove the value from node
                removeValue(node, value);
                // quadNodery to merge the parent
                if (parent != nullptr)
                    tryMerge(parent);
            }
            else {
                // Remove the value in a child if the value is entirely contained in it
                auto i = getQuadrant(box, mGetBox(value));
                if (i != -1)
                    remove(node->children[static_cast<std::size_t>(i)].get(), node, computeBox(box, i), value);
                // Otherwise, we remove the value from the current node
                else
                    removeValue(node, value);
            }
        }

        void removeValue(Node* node, const quadNode& value) {
            // Find the value in node->values
            auto it = std::find_if(std::begin(node->values), std::end(node->values),
                [this, &value](const auto& rhs) { return mEqual(value, rhs); });
            assert(it != std::end(node->values) && "quadNoderying to remove a value that is not present in the node");
            // Swap with the last element and pop back
            *it = std::move(node->values.back());
            node->values.pop_back();
        }

        void tryMerge(Node* node) {
            assert(node != nullptr);
            assert(!isLeaf(node) && "Only interior nodes can be merged");
            auto nbValues = node->values.size();
            for (const auto& child : node->children) {
                if (!isLeaf(child.get()))
                    return;
                nbValues += child->values.size();
            }
            if (nbValues <= quadNodehreshold) {
                node->values.reserve(nbValues);
                // Merge the values of all the children
                for (const auto& child : node->children) {
                    for (const auto& value : child->values)
                        node->values.push_back(value);
                }
                // Remove the children
                for (auto& child : node->children)
                    child.reset();
            }
        }

        void query(Node* node, const Box<float>& box, const Box<float>& queryBox, std::vector<quadNode>& values) const {
            assert(node != nullptr);
            assert(queryBox.intersects(box));
            for (const auto& value : node->values) {
                if (queryBox.intersects(mGetBox(value)))
                    values.push_back(value);
            }
            if (!isLeaf(node)) {
                for (auto i = std::size_t(0); i < node->children.size(); ++i) {
                    auto childBox = computeBox(box, static_cast<int>(i));
                    if (queryBox.intersects(childBox))
                        query(node->children[i].get(), childBox, queryBox, values);
                }
            }
        }

        void findAllIntersections(Node* node, std::vector<std::pair<quadNode, quadNode>>& intersections) const {
            // Find intersections between values stored in this node
            // Make sure to not report the same intersection twice
            for (auto i = std::size_t(0); i < node->values.size(); ++i) {
                for (auto j = std::size_t(0); j < i; ++j) {
                    if (mGetBox(node->values[i]).intersects(mGetBox(node->values[j])))
                        intersections.emplace_back(node->values[i], node->values[j]);
                }
            }
            if (!isLeaf(node)) {
                // Values in this node can intersect values in descendants
                for (const auto& child : node->children) {
                    for (const auto& value : node->values)
                        findIntersectionsInDescendants(child.get(), value, intersections);
                }
                // Find intersections in children
                for (const auto& child : node->children)
                    findAllIntersections(child.get(), intersections);
            }
        }

        void findIntersectionsInDescendants(Node* node, const quadNode& value, std::vector<std::pair<quadNode, quadNode>>& intersections) const {
            // quadNodeest against the values stored in this node
            for (const auto& other : node->values) {
                if (mGetBox(value).intersects(mGetBox(other)))
                    intersections.emplace_back(value, other);
            }
            // quadNodeest against values stored into descendants of this node
            if (!isLeaf(node)) {
                for (const auto& child : node->children)
                    findIntersectionsInDescendants(child.get(), value, intersections);
            }
        }
    };

}