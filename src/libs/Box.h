#pragma once

#include "Vector2.h"
#include "raylib.h"

namespace quadtree {

    template<typename T>
    class Box {
    public:
        T left;
        T top;
        T width; // Must be positive
        T height; // Must be positive

        constexpr Box(Rectangle rect) noexcept :
            left(rect.x), top(rect.y), width(rect.width), height(rect.height) {
        }

        constexpr Box(T Left = 0, T Top = 0, T Width = 0, T Height = 0) noexcept :
            left(Left), top(Top), width(Width), height(Height) {

        }

        constexpr Box(const quadtreeVector2::Vector2<T>& position, const quadtreeVector2::Vector2<T>& size) noexcept :
            left(position.x), top(position.y), width(size.x), height(size.y) {

        }

        constexpr T getRight() const noexcept {
            return left + width;
        }

        constexpr T getBottom() const noexcept {
            return top + height;
        }

        constexpr quadtreeVector2::Vector2<T> getTopLeft() const noexcept {
            return quadtreeVector2::Vector2<T>(left, top);
        }

        constexpr quadtreeVector2::Vector2<T> getCenter() const noexcept {
            return quadtreeVector2::Vector2<T>(left + width / 2, top + height / 2);
        }

        constexpr quadtreeVector2::Vector2<T> getSize() const noexcept {
            return quadtreeVector2::Vector2<T>(width, height);
        }

        constexpr bool contains(const Box<T>& box) const noexcept {
            return left <= box.left && box.getRight() <= getRight() &&
                top <= box.top && box.getBottom() <= getBottom();
        }

        constexpr bool intersects(const Box<T>& box) const noexcept {
            return !(left >= box.getRight() || getRight() <= box.left ||
                top >= box.getBottom() || getBottom() <= box.top);
        }
    };

}