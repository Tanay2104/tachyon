#ifndef INTRUSIVE_LIST_HPP
#define INTRUSIVE_LIST_HPP

#include <algorithm>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <iostream>
#include <iterator>
#include <stdexcept>

#define container_of(ptr, type, member)               \
  ({                                                  \
    const typeof(((type*)0)->member)* __mptr = (ptr); \
    (type*)((char*)__mptr - offsetof(type, member));  \
  })

// Concept for checking existance of intr_node
// TODO: learn concepts and requires properly
template <typename T, typename MemberType>
concept HasMemberValueOfType = requires(T t) {
  { t.intr_node } -> std::convertible_to<MemberType>;
};
struct IntrusiveListNode {  // Must be present in data as intr_node;
  IntrusiveListNode* next{};
  IntrusiveListNode* prev{};
};

template <typename T>
class intrusive_list {
 public:
  IntrusiveListNode root;
  size_t list_size{};

 public:
  intrusive_list() {
    static_assert(HasMemberValueOfType<T, IntrusiveListNode>,
                  "Struct must have a IntrusiveListNode of name intr_node");
    root.next = &root;
    root.prev = &root;
  }

  // Move assignment.
  auto operator=(intrusive_list&& other) noexcept {
    this->root.next = other.root.next;
    this->root.prev = other.root.prev;
    other.root.next->prev = &this->root;
    other.root.prev->next = &this->root;

    // this->root = other.root;
    this->list_size = other.list_size;
    other.list_size = 0;
  }

  // Move constructor..
  intrusive_list(intrusive_list&& other) noexcept {
    this->root.next = other.root.next;
    this->root.prev = other.root.prev;
    other.root.next->prev = &this->root;
    other.root.prev->next = &this->root;

    // this->root = other.root;
    this->list_size = other.list_size;
    other.list_size = 0;
  }
  auto size() -> size_t { return list_size; }

  // Add node between next and previous.
  void addNode(IntrusiveListNode* new_node, IntrusiveListNode* prev,
               IntrusiveListNode* next) {
    list_size++;
    new_node->next = next;
    new_node->prev = prev;
    next->prev = new_node;
    prev->next = new_node;
  }
  void remove(IntrusiveListNode* node) {
    node->next->prev = node->prev;
    node->prev->next = node->next;
    node->next = nullptr;
    node->prev = nullptr;
    list_size--;
  }
  // Remove an element from the list.
  void remove(T& data) { remove(&data.intr_node); }

  // Push an element to the back of the list.
  void push_back(IntrusiveListNode* node) { addNode(node, &root, root.next); }
  void push_back(T& data) { push_back(&data.intr_node); }

  // Push an element into front of the list.
  void push_front(IntrusiveListNode* node) { addNode(node, root.prev, &root); }
  void push_front(T& data) { push_front(&data.intr_node); }

  // Get object for a listnode.
  auto getData(IntrusiveListNode* node) -> T* {
    return container_of(node, T, intr_node);
  }
  // Get object at front.
  auto front() -> T& { return (*getData(root.prev)); }
  // Get object at back.
  auto back() -> T& { return *(getData(root.next)); }

  auto operator[](std::size_t index) -> T& {
    if (size() == 0 || index >= size()) {
      throw std::out_of_range("Index out of range");
    }
    IntrusiveListNode* node = root.prev;
    for (size_t i = 0; i < index; i++) {
      node = node->prev;
    }
    return *getData(node);
  }

  void pop_front(T& data) {
    if (size() == 0) {
      throw std::out_of_range("Cannot pop from empty list");
    }
    data = front();
    remove(root.prev);
  }
  void pop_back(T& data) {
    if (size() == 0) {
      throw std::out_of_range("Cannot pop from empty list");
    }
    data = back();
    remove(root.next);
  }
  template <typename Func>
  void for_each(Func func) {
    IntrusiveListNode* current = root.prev;
    while (current != &root) {
      // PREFETCH INSTRUCTION
      // Tells CPU to load the NEXT node into L1 cache
      // while we process the CURRENT node.
      if (current->prev) {
        __builtin_prefetch(current->prev, 0, 3);
      }

      // Get data and execute
      T* data = container_of(current, T, intr_node);
      func(*data);

      current = current->prev;
    }
  }
};

#endif
