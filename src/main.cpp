#include <iostream>

#include "shared_ptr.hpp"
#include <memory>

struct Foo
{
    Foo(int _val) : val(_val) { std::cout << "Foo...\n"; }
    ~Foo() { std::cout << "~Foo...\n"; }
    std::string print() { return std::to_string(val); }
    int val;
};


void print_ptr(stel::shared_ptr<int> ptr) {
	std::cout << "ptr count inside function: " << ptr.use_count() << std::endl;
	if (ptr) {
		std::cout << "Hello from function: " << *ptr << std::endl;
	}
}

void print_std_ptr(std::shared_ptr<int> ptr) {
	std::cout << "std_ptr count inside function: " << ptr.use_count() << std::endl;
	if (ptr) {
		std::cout << "Hello from std_function: " << *ptr << std::endl;
	}
}

class Person
{
	std::string m_name;
	std::weak_ptr<Person> m_partner; // initially created empty

public:

	Person(const std::string &name): m_name(name)
	{
		std::cout << m_name << " created\n";
	}
	~Person()
	{
		std::cout << m_name << " destroyed\n";
	}

	friend bool partnerUp(std::shared_ptr<Person> &p1, std::shared_ptr<Person> &p2)
	{
		if (!p1 || !p2)
			return false;

		p1->m_partner = p2;
		p2->m_partner = p1;

		std::cout << p1->m_name << " is now partnered with " << p2->m_name << '\n';
		std::cout << "p1 count: " << p1.use_count() << std::endl;
		std::cout << "p2 count: " << p2.use_count() << std::endl;

		return true;
	}
};

int main() {

	stel::shared_ptr<int> ptr = stel::make_shared<int>(2);
	std::shared_ptr<int> std_ptr = std::make_shared<int>(42);

	if (ptr.get()) {
		std::cout << "Pointer is not null\n";
		std::cout << ".get() is " << *ptr.get() << std::endl;
	}

	std::cout << "ptr: " << *ptr << std::endl;
	std::cout << "std ptr: " << *std_ptr << std::endl;

	printf("printf ptr: %d\n", *ptr);
	printf("printf std_ptr: %d\n", *std_ptr);

	if (ptr) {
		std::cout << "Stel pointer is not null\n";
	}

	std::cout << "ptr count: " << ptr.use_count() << std::endl;

	auto new_ptr = ptr;

	std::cout << "ptr count: " << ptr.use_count() << std::endl;

	print_ptr(ptr);

	std::cout << "ptr count: " << ptr.use_count() << std::endl;

	std::cout << "std_ptr count: " << std_ptr.use_count() << std::endl;
	print_std_ptr(std_ptr);
	std::cout << "std_ptr count: " << std_ptr.use_count() << std::endl;

	auto new_ptr_assig = stel::make_shared<int>(3);
	new_ptr_assig = ptr;

	std::cout << "ptr count after assignment: " << ptr.use_count() << std::endl;

	std::shared_ptr<int> std_ptr1 = std::make_shared<int>(323);
	auto std_ptr2 = std_ptr1;

	std::cout << "count of std_ptr1: " << std_ptr1.use_count() << std::endl;

	std::shared_ptr<int> std_ptr3 = std::make_shared<int>(323);
	auto std_ptr4 = std_ptr3;

	std::cout << "count of std_ptr3: " << std_ptr3.use_count() << std::endl;

	std_ptr4 = std_ptr1;

	std::cout << "after assignmet\n";

	std::cout << "count of std_ptr1: " << std_ptr1.use_count() << std::endl;
	std::cout << "count of std_ptr3: " << std_ptr3.use_count() << std::endl;

	auto ppp1 = std_ptr1;
	auto ppp2 = std_ptr1;
	auto ppp3 = std_ptr1;

	std::cout << "count of std_ptr1: " << std_ptr1.use_count() << std::endl;
	std::cout << "count of std_ptr3: " << std_ptr3.use_count() << std::endl;

	auto xxx1 = std_ptr3;
	auto xxx2 = std_ptr3;

	std::cout << "count of std_ptr1: " << std_ptr1.use_count() << std::endl;
	std::cout << "count of std_ptr3: " << std_ptr3.use_count() << std::endl;
	xxx2.reset();
	xxx1.reset();
	std::cout << "after reset\n";
	std::cout << "count of std_ptr3: " << std_ptr3.use_count() << std::endl;


	/// ---------------
	///
	std::cout << "stel ptr\n\n";
	stel::shared_ptr<int> stel_ptr1 = stel::make_shared<int>(323);
	auto stel_ptr2 = stel_ptr1;

	std::cout << "count of stel_ptr1: " << stel_ptr1.use_count() << std::endl;

	stel::shared_ptr<int> stel_ptr3 = stel::make_shared<int>(323);
	auto stel_ptr4 = stel_ptr3;

	std::cout << "count of stel_ptr3: " << stel_ptr3.use_count() << std::endl;
	std::cout << "Assigning\n";
	stel_ptr4 = stel_ptr1;

	std::cout << "after assignmet\n";

	std::cout << "count of stel_ptr1: " << stel_ptr1.use_count() << std::endl;
	std::cout << "count of stel_ptr3: " << stel_ptr3.use_count() << std::endl;
	//std::cout << "count of stel_ptr4: " << stel_ptr4.use_count() << std::endl;

	auto pp1 = stel_ptr1;
	auto pp2 = stel_ptr1;
	auto pp3 = stel_ptr1;

	std::cout << "count of stel_ptr1: " << stel_ptr1.use_count() << std::endl;
	std::cout << "count of stel_ptr3: " << stel_ptr3.use_count() << std::endl;


	auto xx1 = stel_ptr3;
	auto xx2 = stel_ptr3;

	std::cout << "count of stel_ptr1: " << stel_ptr1.use_count() << std::endl;
	std::cout << "count of stel_ptr3: " << stel_ptr3.use_count() << std::endl;

	stel::shared_ptr<Foo> p1 = stel::make_shared<Foo>(100);
	   stel::shared_ptr<Foo> p2 = stel::make_shared<Foo>(200);
	   auto print = [&]()
	   {
	       std::cout << " p1=" << (p1 ? p1->print() : "nullptr");
	       std::cout << " p2=" << (p2 ? p2->print() : "nullptr") << '\n';  
	   };
	   print();

	   p1.swap(p2);
	   print();

	   p1.reset();
	   print();

	   p1.swap(p2);
	   print();


    return 0;
}
