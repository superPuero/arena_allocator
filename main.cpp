#include <iostream>
#include <vector>
#include <stacktrace>

struct allocation_metadata{

	allocation_metadata() = default;

	template<typename Ty>
	allocation_metadata(Ty* ptr){
		this->ptr = ptr;
		size = sizeof(Ty);
		occupies = size;
		deleter = [](void* obj) { static_cast<Ty*>(obj)->~Ty(); };
	}

	size_t size;
	size_t occupies;

	void(*deleter)(void*);
	void* ptr;

	void invoke_deleter() {
		deleter(ptr);
		std::cout << "Deleter invoked " << '\n';
	}

};

class arena_allocator {
public:
	arena_allocator(size_t size){
		m_size = size;
		m_data = malloc(m_size);

		free_slots = reinterpret_cast<allocation_metadata**>((reinterpret_cast<allocation_metadata*>(static_cast<char*>(m_data) + m_size)) - 1);

		free_slots_occupied = 0;
		m_occupied = 0;
		std::cout << "Arena allocated, size " << m_size << " bytes " << '\n';

	}
	~arena_allocator(){
		if (m_data) {
			delete m_data;
			m_data = nullptr;
			std::cout << "Arena cleared" << '\n';
		}
		else {
			std::cout << "Arena is already empty" << '\n';
		}

	}

public:
	template<typename T,typename...Args> 
	T* add(Args&&...args) {

		auto position = (static_cast<char*>(m_data) + m_occupied);

		T* new_obj = new(position + sizeof(allocation_metadata)) T(std::forward<Args>(args)...);

		allocation_metadata* metadata = new(position) allocation_metadata(new_obj);

		m_occupied += sizeof(T);

		std::cout << "Added " << sizeof(T) << " to occupied, its sizeof(T) " << __LINE__ << '\n';

		m_occupied += sizeof(allocation_metadata);

		std::cout << "Added " << sizeof(allocation_metadata) << " to occupied, its sizeof(allocation_metadata) " << __LINE__ << '\n';


		return new_obj;
	}

	void erase(void* ptr) {
		allocation_metadata* metadata = reinterpret_cast<allocation_metadata*>(static_cast<char*>(ptr) - sizeof(allocation_metadata));
		metadata->invoke_deleter();

		for (int64_t i = 0; i < free_slots_occupied; i++) {
			if (!free_slots[-i]) {
				free_slots[-i] = metadata;

				m_occupied -= metadata->size;

				std::cout << "Removed " << metadata->size << " to occupied, its sizeof(allocation_metadata) " << __LINE__ << '\n';

				m_occupied += sizeof(allocation_metadata*);

				std::cout << "Added " << sizeof(allocation_metadata*) << " to occupied, its sizeof(allocation_metadata*) " << __LINE__ << '\n';


				return;
			};
		}

		//m_occupied -= sizeof(allocation_metadata);

		//std::cout << "Removed " << sizeof(allocation_metadata) << " to occupied, its sizeof(allocation_metadata) " << __LINE__ << '\n';

		m_occupied -= metadata->size;

		std::cout << "Removed " << metadata->size << " to occupied, its metadata->size " << __LINE__ << '\n';

		m_occupied += sizeof(allocation_metadata*);

		std::cout << "Added " << sizeof(allocation_metadata*) << " to occupied, its sizeof(allocation_metadata*) " << __LINE__ << '\n';

		free_slots[-free_slots_occupied] = metadata;
		free_slots_occupied++;
	}

	template<typename T, typename...Args>
	T* add_to_free_slot(Args&&...args) {
		for (int64_t i = 0; i < free_slots_occupied; i++) {
			if (free_slots[-i]) {
				if (sizeof(T) <= free_slots[-i]->size) {
					std::cout << "Found free slot" << '\n';

					auto temp = free_slots[-i];

					free_slots[-i] = nullptr;

					return add_at<T>(temp, std::forward<Args>(args)...);
				}
			}

		}
		std::cout << "Didnt found free slot" << '\n';
		return add<T>(std::forward<Args>(args)...);
	}

public:
	size_t get_occupied() {
		return m_occupied;
	}

private:
	template<typename T, typename...Args>
	T* add_at(allocation_metadata* ptr, Args&&...args) {

		T* new_obj = new(reinterpret_cast<char*>(ptr) + sizeof(allocation_metadata)) T(std::forward<Args>(args)...);

		m_occupied -= ptr->occupies;

		std::cout << "Removed " << ptr->occupies << " to occupied, its ptr->occupies " << __LINE__ << '\n';

		ptr->occupies = sizeof(T);

		m_occupied += ptr->occupies;

		std::cout << "Added " << ptr->occupies << " to occupied, its ptr->occupies " << __LINE__ << '\n';

		ptr->deleter = [](void* obj) { static_cast<T*>(obj)->~T(); };
		
		return new_obj;
	}

private:
	void* m_data;

	size_t m_occupied;
	size_t m_size;
	allocation_metadata** free_slots;
	int64_t free_slots_occupied;
};

class Foo {
public:
	Foo(int n, float d, float d2) : num(n), decimal(d), decimal2(d2){};

	Foo(Foo&& other) noexcept{
		decimal = other.decimal;
		decimal2 = other.decimal2;
	}

	~Foo() {
		std::cout << "Im so dead" << '\n';
	}
public:
	int num[1000];
	float decimal;
	float decimal2;

};

int main() {
	arena_allocator arena(4096);

	auto f1 = arena.add<Foo>(123, 123, 534);
	arena.add<int>(1);
	auto f2 = arena.add<Foo>(123, 123, 534);

	arena.erase(f1);

	auto f3 = arena.add_to_free_slot<int>(123);

	std::cout << arena.get_occupied() << '\n';
}