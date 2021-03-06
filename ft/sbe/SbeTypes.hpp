#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include "ft/utils/StringUtils.hpp"

#include "toolbox/util/ByteTraits.hpp"

namespace ft::sbe {
#pragma pack(push, 1)

template<std::size_t Length>
class FixedString {
public:
    constexpr FixedString() = default;
    constexpr FixedString(const char* val) {
        std::strncpy(data_, val, Length);
    }
    constexpr std::size_t capacity() const { return Length; }
    constexpr std::size_t size() const { return std::char_traits<char>::length(data_);}
    constexpr std::string_view str() const { return std::string_view(data_, size()); }
    std::wstring wstr() const { return ft::to_wstring(str()); }
    constexpr const char* c_str() const { return data_; }

    friend auto& operator<<(std::ostream& os, const FixedString& self) {
        return os << self.str();
    }
private:
    char data_[Length+1] {};
};

template<typename SizeT>
class VarStringBase {
protected:
    SizeT size_{};
    char data_[0];    
};

template<typename SizeT, std::size_t OffsetI=sizeof(SizeT), std::size_t CapacityI=0>
class BasicVarString : public VarStringBase<SizeT> {
    using Base = VarStringBase<SizeT>;
    using Base::size_;
    using Base::data_;
public:
    constexpr BasicVarString() = default;
    constexpr BasicVarString(SizeT len) {
        size_ = len;
    }
    template<typename StringT>
    auto& operator=(const StringT& val) {
        size_ = val.size();
        std::memcpy(data(), val.data(), size_);
        //data()[size_] = 0;
        return *this;
    }
    constexpr std::size_t capacity() const { return size_; }
    constexpr std::size_t size() const { return size_;} // FIXME: std::strlen(data_) ?
    void resize(std::size_t val) { size_ = val; }
    constexpr std::string_view str() const { return std::string_view(data(), size()); }
    operator std::string_view() const { return str(); }
    std::wstring wstr() const { return ft::to_wstring(str()); }
    constexpr const char* c_str() const { return data(); }
    char* data() { return &data_[OffsetI-sizeof(SizeT)]; }
    const char* data() const { return &data_[OffsetI-sizeof(SizeT)]; }    
    constexpr std::size_t bytesize() const { return sizeof(SizeT) + capacity(); }

    friend auto& operator<<(std::ostream& os, const BasicVarString& self) {
        return os << self.str();
    }
};

using VarString = BasicVarString<std::uint16_t>;
#pragma pack(pop)
}