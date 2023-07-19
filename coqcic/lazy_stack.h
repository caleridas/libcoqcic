#ifndef COQCIC_LAZY_STACK_H
#define COQCIC_LAZY_STACK_H

// "Functional" stack data structure
//
// Provides a functional-style stack data structure: Each stack
// state consists of objects at different logical depths. "Push"
// and "pop" operations on the stack leave the old state unmodified
// and create new states instead.

#include <memory>
#include <stdexcept>
#include <vector>

namespace coqcic {

template<typename T>
struct lazy_stack_repr {
	std::vector<T> items;
	std::shared_ptr<lazy_stack_repr<T>> next;
};

template<typename T>
class lazy_stack {
public:
	inline
	lazy_stack() noexcept = default;

	inline
	std::size_t
	size() const noexcept;

	inline
	bool
	empty() const noexcept;

	inline
	lazy_stack<T>
	push(T t) const;

	inline
	lazy_stack<T>
	pop() const;

	inline
	const T&
	at(std::size_t index) const;

	inline
	const T&
	get(std::size_t index, const T& fallback) const noexcept;

	void
	set(std::size_t index, T value);

private:
	inline
	lazy_stack(std::shared_ptr<lazy_stack_repr<T>> repr) noexcept : repr_(std::move(repr)) {}

	std::shared_ptr<lazy_stack_repr<T>> repr_;
};

template<typename T>
inline
std::size_t
lazy_stack<T>::size() const noexcept
{
	std::size_t count = 0;
	auto ptr = repr_.get();
	while (ptr) {
		count += ptr->items.size();
		ptr = ptr->next.get();
	}
	return count;
}

template<typename T>
inline
bool
lazy_stack<T>::empty() const noexcept
{
	return !repr_;
}

template<typename T>
inline
lazy_stack<T>
lazy_stack<T>::push(T t) const
{
	auto next = repr_;
	auto repr = std::make_shared<lazy_stack_repr<T>>();
	repr->items.push_back(t);
	while (next && next->items.size() == repr->items.size()) {
		repr->items.insert(repr->items.end(), next->items.begin(), next->items.end());
		next = next->next;
	}
	repr->next = std::move(next);

	return lazy_stack(std::move(repr));
}

template<typename T>
inline
lazy_stack<T>
lazy_stack<T>::pop() const
{
	auto next = repr_->next;
	std::shared_ptr<lazy_stack_repr<T>> repr;
	std::shared_ptr<lazy_stack_repr<T>>* current = &repr;
	auto i = repr_->items.begin() + 1;
	auto end = repr_->items.end();
	std::size_t pow = 1;

	while (i != end) {
		std::size_t size = end - i;
		while ((size & pow) == 0) {
			pow = pow << 1;
		}
		auto tmp = std::make_shared<lazy_stack_repr<T>>();
		tmp->items.insert(tmp->items.begin(), i, i + pow);
		i += pow;
		*current = tmp;
		current = &tmp->next;
	}

	*current = next;

	return lazy_stack(std::move(repr));
}

template<typename T>
inline
const T&
lazy_stack<T>::at(std::size_t index) const
{
	auto ptr = repr_.get();
	while (ptr) {
		if (index < ptr->items.size()) {
			return ptr->items[index];
		} else {
			index -= ptr->items.size();
			ptr = ptr->next.get();
		}
	}

	throw std::runtime_error("Index out of range");
}

template<typename T>
inline
const T&
lazy_stack<T>::get(std::size_t index, const T& fallback) const noexcept
{
	auto ptr = repr_.get();
	while (ptr) {
		if (index < ptr->items.size()) {
			return ptr->items[index];
		} else {
			index -= ptr->items.size();
			ptr = ptr->next.get();
		}
	}

	return fallback;
}

template<typename T>
inline
void
lazy_stack<T>::set(std::size_t index, T value)
{
	auto pptr = &repr_;
	while (*pptr) {
		if (!pptr->unique()) {
			auto ptr = std::make_shared<lazy_stack_repr<T>>(**pptr);
			*pptr = std::move(ptr);
		}
		if (index < (*pptr)->items.size()) {
			(*pptr)->items[index] = std::move(value);
			return;
		} else {
			index -= (*pptr)->items.size();
			pptr = &(*pptr)->next;
		}
	}

	throw std::runtime_error("Index out of range");
}

}  // namespace coqcic

#endif  // COQCIC_SHARED_STACK_H
