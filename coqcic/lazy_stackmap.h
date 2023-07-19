#ifndef COQCIC_LAZY_STACKMAP_H
#define COQCIC_LAZY_STACKMAP_H

#include <memory>
#include <optional>
#include <stdexcept>
#include <unordered_map>

namespace coqcic {

template<typename T>
struct lazy_stackmap_repr {
	std::unordered_map<T, std::size_t> items;
	std::shared_ptr<lazy_stackmap_repr<T>> next;
};

template<typename T>
class lazy_stackmap {
public:
	inline
	lazy_stackmap() noexcept = default;

	inline
	bool
	empty() const noexcept;

	inline
	lazy_stackmap<T>
	push(T t) const;

	inline
	std::optional<std::size_t>
	get_index(const T& key) const noexcept;

	inline
	std::unordered_map<T, std::size_t>
	flatten() const;

private:
	inline
	lazy_stackmap(std::shared_ptr<lazy_stackmap_repr<T>> repr, std::size_t bottom) noexcept
		: repr_(std::move(repr)), bottom_(bottom)
	{
	}

	std::shared_ptr<lazy_stackmap_repr<T>> repr_;
	std::size_t bottom_ = 0;
};

template<typename T>
inline
bool
lazy_stackmap<T>::empty() const noexcept
{
	return !repr_;
}

template<typename T>
inline
lazy_stackmap<T>
lazy_stackmap<T>::push(T key) const
{
	auto next = repr_;
	auto repr = std::make_shared<lazy_stackmap_repr<T>>();
	repr->items.emplace(key, bottom_ + 1);

	while (next && next->items.size() >= repr->items.size()) {
		for (const auto& item : next->items) {
			repr->items.insert(item);
		}
		next = next->next;
	}
	repr->next = std::move(next);

	return lazy_stackmap(std::move(repr), bottom_ + 1);
}

template<typename T>
inline
std::optional<std::size_t>
lazy_stackmap<T>::get_index(const T& key) const noexcept
{
	auto ptr = repr_.get();
	while (ptr) {
		auto i = ptr->items.find(key);
		if (i != ptr->items.end()) {
			return bottom_ - i->second;
		}
		ptr = ptr->next.get();
	}

	return std::nullopt;
}

template<typename T>
inline
std::unordered_map<T, std::size_t>
lazy_stackmap<T>::flatten() const
{
	std::unordered_map<T, std::size_t> result;

	auto ptr = repr_.get();
	while (ptr) {
		for (const auto& item : ptr->items) {
			result.emplace(item.first, bottom_ - item.second);
		}
		ptr = ptr->next;
	}

	return result;
}


}  // namespace coqcic

#endif  // COQCIC_SHARED_STACKMAP_H
