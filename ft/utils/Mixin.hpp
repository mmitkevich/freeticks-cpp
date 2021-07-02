#pragma once

/// CRTP boilerplate
#define FT_SELF(SelfT) \
    constexpr SelfT* self() { return static_cast<SelfT*>(this); } \
    constexpr const SelfT* self() const { return static_cast<const SelfT*>(this); } 

#define FT_IFACE(SelfT) \
    FT_SELF(SelfT); \
    using Ref = BasicRef<SelfT>; \
    template<class Self, class Base = SelfT> \
    class Impl;

#define FT_IMPL(SelfT) \
    FT_SELF(SelfT); \
    auto* impl() { return self()->impl(); } \
    const auto* impl() const {return self()->impl(); } \
    friend SelfT;

namespace ft{ inline namespace core {
class MixinBase {};
}};