// Copyright (c) 2026 Jyotipriya Mondal
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

/**
 * @file Chainable.hpp
 * @brief C++23 Explicit Object Parameter ("Deducing This") static dispatch interface.
 *
 * Provides the wavex::Chainable base class for zero-overhead compile-time
 * polymorphism, method chaining, and static pipeline composition.
 */

#pragma once

#include <utility>
#include <string_view>
#include <concepts>

namespace wavex {

    /**
     * @class Chainable
     * @brief C++23 "Deducing This" CRTP replacement.
     *
     * Enables compile-time static dispatch without requiring the base class
     * to be a template parameterized on the derived type (e.g. `Derived : Chainable`
     * instead of `Derived : Chainable<Derived>`).
     *
     * Value category (lvalue, rvalue, const) is preserved automatically via
     * `std::forward<Self>(self)`.
     */
    class Chainable {
    protected:
        /// Non-virtual protected destructor prevents unsafe polymorphic deletion through Base*
        ~Chainable() = default;

    public:
        /**
         * @brief Statically dispatches to `self.handle_impl(args...)`.
         */
        template <typename Self, typename... Args>
        decltype(auto) handle(this Self&& self, Args&&... args) {
            return std::forward<Self>(self).handle_impl(std::forward<Args>(args)...);
        }

        /**
         * @brief Statically dispatches to `self.name_impl()`.
         */
        template <typename Self>
        decltype(auto) name(this Self&& self) {
            return std::forward<Self>(self).name_impl();
        }
    };

    /**
     * @concept ChainableHandler
     * @brief Concept ensuring a type satisfies the Chainable handle requirements for given arguments.
     */
    template <typename T, typename... Args>
    concept ChainableHandler = requires(T& handler, Args&&... args) {
        handler.handle(std::forward<Args>(args)...);
    };

} // namespace wavex
