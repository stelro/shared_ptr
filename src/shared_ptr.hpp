#pragma once 

#include <utility>
#include <ostream>
#include <iostream>
#include <type_traits>
#include <memory>
#include <cstddef>
#include <atomic>

namespace stel {

class bad_weak_ptr : public std::exception {
public:
	const char* what() const noexcept override {
		return "bad_weak_ptr";
	}

};

template <typename T>
struct default_deleter {
	void operator ()(T* ptr) {
		delete ptr;
	}
};

template <typename T>
struct default_deleter<T[]> {
	void operator ()(T* ptr) {
		delete [] ptr;
	}
};

template <typename T>
class shared_ptr;

template <typename T>
class weak_ptr;

class control_block_base {
public:
	control_block_base() : shared_count_(1), weak_count_(1) { }
	virtual ~control_block_base() = default;

	void add_shared() {
		shared_count_.fetch_add(1, std::memory_order_relaxed);
	}

	bool try_add_shared_ref() {
		long current = shared_count_.load(std::memory_order_relaxed);
		while (current > 0) {
			if (shared_count_.compare_exchange_weak(current, current + 1,
						std::memory_order_acq_rel,
						std::memory_order_relaxed)) {
				return true;
			}
		}
		return false;
	}

	void add_weak() {
		weak_count_.fetch_add(1, std::memory_order_relaxed);
	}

	void release_shared() {
		if (shared_count_.fetch_sub(1, std::memory_order_relaxed) == 1) {
			destroy_object();
			release_weak();
		}
	}

	void release_weak() {
		if (weak_count_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
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

private:
	virtual void destroy_object() = 0;
	std::atomic<long> shared_count_;
	std::atomic<long> weak_count_;

};

template <typename T, typename Deleter>
class control_block_ptr : public control_block_base {
public:
	control_block_ptr(T* ptr, Deleter deleter) : ptr_(ptr), deleter_(deleter) { }
protected:
	void destroy_object() override {
		deleter_(ptr_);
		ptr_ = nullptr;
	}
private:
	T* ptr_;
	Deleter deleter_;
};

template <typename T>
class control_block_obj : public control_block_base {
public:
	template <typename... Args>
	control_block_obj(Args&&... args) {
		new (get_object_ptr()) T(std::forward<Args>(args)...);
	}
	T* get_object_ptr() {
		return reinterpret_cast<T*>(storage_);
	}
protected:
	void destroy_object() override {
		get_object_ptr()->~T();
	}
private:
	alignas(T) unsigned char storage_[sizeof(T)];
};

template <typename T>
class shared_ptr {
public:

	shared_ptr() : ptr_(nullptr), control_(nullptr) { }
	shared_ptr(std::nullptr_t) : ptr_(nullptr), control_(nullptr) { }

	shared_ptr(T* ptr) 
		: ptr_(ptr), control_(nullptr) 
	{ 
		try {
			control_ = new control_block_ptr<T, default_deleter<T>>(
					ptr_, default_deleter<T>());
		} catch (...) {
			delete ptr;
		}
	}

	template <typename Deleter>
	shared_ptr(T* ptr, Deleter deleter) 
		: ptr_(ptr), control_(nullptr) 
	{ 
		try {
			control_ = new control_block_ptr<T, Deleter>(ptr_, deleter);
		} catch (...) {
			deleter(ptr);
		}
	}

	// Constructor from a weak_ptr
	explicit shared_ptr(const weak_ptr<T>& weak) 
		: ptr_(nullptr), control_(nullptr)
	{
		if (weak.control_ && weak.control_->try_add_shared_ref()) {
			ptr_ = weak.ptr_;
			control_ = weak.control_;
		} else {
			throw bad_weak_ptr();
		}
	}

	// constructor for the make_shared
	template <typename U>
	shared_ptr(control_block_base* control, U* ptr) noexcept 
		: ptr_(ptr), control_(control) { }

	shared_ptr(const shared_ptr& other) 
		: ptr_(other.ptr_), control_(other.control_) 
	{
		if (control_) {
			control_->add_shared();
		}
	}

	shared_ptr& operator =(const shared_ptr& other) {
		shared_ptr(other).swap(*this);
		return *this;
	}

	shared_ptr(shared_ptr&& other) 
		: ptr_(std::exchange(other.ptr_, nullptr)), control_(std::exchange(other.control_, nullptr)) 
	{
	}

	shared_ptr& operator =(shared_ptr&& other) {
		shared_ptr(std::move(other)).swap(*this);
		return *this;
	}

	~shared_ptr() noexcept {
		if (control_) {
			control_->release_shared();
		}
	}

	void reset() noexcept {
		shared_ptr().swap(*this);
	}

	T& operator *() const { return *ptr_; }
	T* operator ->() const { return ptr_; }
	T* get() const { return ptr_; }
	
	operator bool() const { return ptr_ != nullptr; }
	
	long use_count() const noexcept {
		return control_ ? control_->shared_count() : 0;
	}

	bool unique() const noexcept {
		if (control_) 
			return control_->shared_count() == 1;
		return false;
	}

	void swap(shared_ptr& other) noexcept {
		using std::swap;
		std::swap(other.ptr_, ptr_);
		std::swap(other.control_, control_);
	}

private:
	template<typename U> friend class shared_ptr;
	template<typename U> friend class weak_ptr;

	// Constructor for weak_ptr::lock() - assumes control block ref already incremented
	shared_ptr(T* ptr, control_block_base* control) noexcept 
		: ptr_(ptr), control_(control)
	{ }

	T* ptr_;
	control_block_base* control_;
};

template <typename T>
class weak_ptr {
public:
	using element_type = T;
	weak_ptr() noexcept : ptr_(nullptr), control_(nullptr) { }

	weak_ptr(const weak_ptr& other) noexcept 
		: ptr_(other.ptr_), control_(other.control_)
	{
		if (control_) {
			control_->add_weak();
		}
	}

	weak_ptr(weak_ptr&& other) noexcept 
		: ptr_(std::exchange(other.ptr_, nullptr))
		, control_(std::exchange(other.control_, nullptr))
	{ }

	// constructor from shared_ptr
	template <typename U>
	weak_ptr(const shared_ptr<U>& shared) noexcept 
		: ptr_(shared.ptr_), control_(shared.control_) {
		static_assert(std::is_convertible_v<U*, T*>,
				"U* must be convertible to T*");
		if (control_) {
			control_->add_weak();
		}
	}

	// copy constructor from compitable types
	template <typename U>
	weak_ptr(const weak_ptr<U>& other) noexcept 
		: ptr_(other.ptr_), control_(other.control_)
	{
		static_assert(std::is_convertible_v<U*, T*>,
				"U* must be convertible to T*");
		if (control_) {
			control_->add_weak();
		}
	}

	~weak_ptr() {
		if (control_) {
			control_->release_weak();
		}
	}

	weak_ptr& operator =(const weak_ptr& other) noexcept {
		weak_ptr(other).swap(*this);
		return *this;
	}

	weak_ptr& operator =(weak_ptr&& other) noexcept {
		weak_ptr(std::move(other)).swap(*this);
		return *this;
	}

	// Assignment from shared_ptr
	template <typename U>
	weak_ptr& operator =(const shared_ptr<U>& other) noexcept {
		weak_ptr(other).swap(*this);
		return *this;
	}

	// Assignment from shared_ptr
	template <typename U>
	weak_ptr& operator =(const weak_ptr<U>& other) noexcept {
		weak_ptr(other).swap(*this);
		return *this;
	}

	void reset() noexcept {
		weak_ptr().swap(*this);
	}
	
	long use_count() noexcept {
		return control_ ? control_->shared_count() : 0;
	}

	bool expired() const noexcept {
		return use_count() == 0;
	}

	shared_ptr<T> lock() const noexcept {
		if (control_ && control_->try_add_shared_ref()) {
			return shared_ptr<T>(ptr_, control_);
		}
		return shared_ptr<T>();
	}
	
	void swap(weak_ptr& other) noexcept {
		using std::swap;
		std::swap(other.ptr_, ptr_);
		std::swap(other.control_, control_);
	}

private:

	template<typename U> friend class shared_ptr;
	template<typename U> friend class weak_ptr;

	T* ptr_;
	control_block_base* control_;
};

template <typename T, typename... Args>
stel::shared_ptr<T> make_shared(Args&&... args) {
	auto control = new control_block_obj<T>(std::forward<Args>(args)...);
	try {
		return shared_ptr<T>(control, control->get_object_ptr());
	} catch (...) {
		delete control;
		throw;
	}
}

}

