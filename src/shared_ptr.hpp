#pragma once 

#include <utility>
#include <ostream>
#include <iostream>
#include <type_traits>
#include <memory>
#include <cstddef>
#include <atomic>

namespace stel {

template <typename T>
struct default_delete {
	void operator ()(T* ptr) const {
		delete ptr;
	}
};

template <typename T>
struct default_delete<T[]> {
	void operator ()(T* ptr) const {
		delete [] ptr;
	}
};

// Control block for reference counting
class control_block_base {
public:
	control_block_base() 
		: shared_count_(1), weak_count_(1) { }
	virtual ~control_block_base() = default;

	void add_shared_ref() {
		shared_count_.fetch_add(1, std::memory_order_relaxed);
	}

	void release_shared() {
		if (shared_count_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
			destroy_object();
			release_weak();
		}
	}

	void add_weak_ref() {
		weak_count_.fetch_add(1, std::memory_order_relaxed);
	}

	void release_weak() {
		if (shared_count_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
			delete this;
		}
	}

	long shared_count() const {
		return shared_count_.load(std::memory_order_relaxed);
	}

	long weak_count() const {
		return weak_count_.load(std::memory_order_relaxed);
	}

	bool expired() const {
		return shared_count() == 0;
	}

protected:
	virtual void destroy_object() = 0;
private:
	std::atomic<long> shared_count_;
	std::atomic<long> weak_count_;
};

// Control block for pointers managed by shared_ptr
template <typename T, typename Deleter>
class control_block_ptr : public control_block_base {
public:
	control_block_ptr(T* ptr, Deleter deleter) 
		: ptr_(ptr), deleter_(deleter) { }
protected:
	void destroy_object() override {
		deleter_(ptr_);
		ptr_ = nullptr;
	}
private:
	T* ptr_;
	Deleter deleter_;
};

// Control block for pointers created by make_shared
template <typename T>
class control_block_obj : public control_block_base {
public:
	template <typename... Args>
	control_block_obj(Args&&... args) {
		new (get_object_ptr()) T(std::forward<Args>(args)...);
	}
	T* get_object_ptr() {
		return reinterpret_cast<T*>(&storage_);
	}
protected:
	void destroy_object() override {
		get_object_ptr()->~T();
	}
private:
	typename std::aligned_storage<sizeof(T), alignof(T)>::type storage_;
};

template <typename T>
class shared_ptr {
public:
	using element_type = T;

	shared_ptr() noexcept : ptr_(nullptr), control_(nullptr) { }
	shared_ptr(std::nullptr_t) noexcept : ptr_(nullptr), control_(nullptr) { }
	explicit shared_ptr(T* ptr) : ptr_(ptr), control_(nullptr) {
		if (ptr) {
			try {
				control_ = new control_block_ptr<T, default_delete<T>>(
						ptr_, default_delete<T>());
			} catch (...) {
				delete ptr_;
				throw;
			}
		}
	}

	template <typename Deleter>
	explicit shared_ptr(T* ptr, Deleter deleter) : ptr_(ptr), control_(nullptr) 
	{
		if (ptr) {
			try {
				control_ = new control_block_ptr<T, Deleter>(ptr_, std::move(deleter));
			} catch (...) {
				deleter(ptr_);
				throw;
			}
		}
	}

	shared_ptr(const shared_ptr& other) noexcept 
		: ptr_(other.ptr_), control_(other.control_) 
	{
		if (control_) {
			control_->add_shared_ref();
		}
	}

	shared_ptr(shared_ptr&& other) noexcept 
		: ptr_(std::exchange(other.ptr_, nullptr)), control_(std::exchange(other.control_, nullptr))
	{
	}
	
	// Copy constructor from compatible type
	template <typename U>
	shared_ptr(const shared_ptr<U>& other) noexcept 
		: ptr_(other.ptr_), control_(other.control_) 
	{
		static_assert(std::is_convertible<U*, T>::value, "U* must be convertible to T*");
		if (control_) {
			control_->add_shared_ref();
		}
	}

	// TODO: constructor from weak_ptr
	//
	~shared_ptr() {
		if (control_) {
			control_->release_shared();
		}
	}

	shared_ptr& operator =(const shared_ptr& other) noexcept {
		shared_ptr(other).swap(*this);
		return *this;
	}

	shared_ptr& operator =(shared_ptr&& other) noexcept {
		shared_ptr(std::move(other)).swap(*this);
		return *this;
	}

	template <typename U>
	shared_ptr& operator =(const shared_ptr<U>& other) noexcept {
		static_assert(std::is_convertible<U*, T>::value, "U* must be convertible to T*");
		shared_ptr(other).swap(*this);
		return *this;
	}

	void reset() noexcept {
		shared_ptr().swap(*this);
	}

	template <typename U>
	void reset(U* ptr) {
		shared_ptr(ptr).swap(*this);
	}

	template <typename U, typename Deleter>
	void reset(U* ptr, Deleter deleter) {
		shared_ptr(ptr, deleter).swap(*this);
	}

	void swap(shared_ptr& other) noexcept {
		using std::swap;
		std::swap(ptr_, other.ptr_);
		std::swap(control_, other.control_);
	}

	T* get() const noexcept{
		return ptr_;
	}

	T* operator ->() const noexcept {
		return ptr_;
	}

	T& operator *() const noexcept {
		return *ptr_;
	}

	T& operator[](std::size_t idx) const {
		return ptr_[idx];
	}

	long use_count() const noexcept {
		return control_ ? control_->shared_count() : 0;
	}

	bool unique() const noexcept {
		return use_count() == 1;
	}

	explicit operator bool() const noexcept {
		return ptr_ != nullptr;
	}


	// TODO: Comparison operators
	// Constructor for make shared
	template <typename U>
	shared_ptr(control_block_obj<U>* control, U* ptr) noexcept 
		: ptr_(ptr), control_(control)
	{
	}
private:
	template<typename U> friend class shared_ptr;
	


	T* ptr_;
	control_block_base* control_;
};

template <typename T, typename... Args>
shared_ptr<T> make_shared(Args&&... args) {
	auto* control = new control_block_obj<T>(std::forward<Args>(args)...);
	try {
		return shared_ptr<T>(control, control->get_object_ptr());
	} catch (...) {
		delete control;
		throw;
	}
}

}
