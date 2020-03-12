#pragma once

#include <vector>
#include <memory>

namespace freeticks { 
inline namespace util {

template<typename T>
class  Pool {
    struct Node {
        Node *next = nullptr;
        T value;

        Node(T value) : value(value) {}
    };
    using SlabPtr = std::unique_ptr<Node[]>;
public:
    Pool() = default;

    // Copy.
    Pool(const Pool&) = delete;
    Pool& operator=(const Pool&) = delete;

    // Move.
    Pool(Pool&&) = delete;
    Pool& operator=(Pool&&) = delete;

    template<typename ...ArgsT>
    T* alloc(ArgsT... args) {

    }
    void dealloc(T* e) noexcept {
        assert(e);
        Node* node = reinterpret_cast<Node*>(reinterpret_cast<void*>(e) - sizeof(Node*));
        node->next = free_;
        free_ = node;
    }

  private:
    std::vector<SlabPtr> slabs_;
    /// Head of free-list.
    Node* free_{nullptr};
};

}} // namespace freeticks::util

