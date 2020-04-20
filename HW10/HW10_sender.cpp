#define BOOST_DATE_TIME_NO_LIB
#define BOOST_USE_WINDOWS_H

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <shared_mutex>
#include <chrono>
#include <conio.h>
#include <sys/types.h>
#include <string>
#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/containers/string.hpp>
#include <boost/interprocess/containers/map.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/sync/interprocess_mutex.hpp>
#include <boost/interprocess/sync/interprocess_condition.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/bind.hpp>

using segment_manager_t = boost::interprocess::managed_shared_memory::segment_manager;
using char_allocator_t = boost::interprocess::allocator < char, segment_manager_t >;
using string_t = boost::interprocess::basic_string < char, std::char_traits < char >, char_allocator_t>;
using id_t = std::size_t;
using record_t = std::pair<const id_t, string_t>;
using record_allocator_t = boost::interprocess::allocator <record_t, segment_manager_t>;
using map_t = boost::interprocess::map <id_t, string_t, std::less <id_t>, record_allocator_t>;

using mx = boost::interprocess::interprocess_mutex;
using cn = boost::interprocess::interprocess_condition;

/* myMap <id, msg> */
void waiter(map_t* map, id_t id, int& keptId, mx* mutex, cn* condition) {
	while (true) {
		{
			boost::interprocess::scoped_lock lock{ *mutex };
			condition->wait(lock);

			if (map->rbegin()->first != keptId) {
				std::cout << map->rbegin()->second << std::endl;
				keptId = map->rbegin()->first;
			}
		}
	}
}

//#define CLEAN_SHARED

int main() {
	const std::string shared_memory_name = "managed_shared_memory";
#ifdef CLEAN_SHARED
	boost::interprocess::shared_memory_object::remove(shared_memory_name.c_str());
	return 0;
#endif // CLEAN_SHARED

	/* creating */
	boost::interprocess::managed_shared_memory shared_memory(boost::interprocess::open_or_create, shared_memory_name.c_str(), 1024);
	std::string mutex_name = "mutex";
	std::string condition_name = "condition";
	record_allocator_t alloc{ shared_memory.get_segment_manager() };
	auto mutex = shared_memory.find_or_construct<mx>(mutex_name.c_str())();
	auto condition = shared_memory.find_or_construct<cn>(condition_name.c_str())();
	auto map = shared_memory.find_or_construct <map_t>("map")(std::less<id_t>(), alloc);

	/* working */
	std::string msg;
	int keptId = -1;
	if (!map->empty()) {
		keptId = map->size();
	}

	std::thread waiter(waiter, map, keptId, std::ref(keptId), mutex, condition);
	while (std::getline(std::cin, msg)) {
		if (msg == "/exit/") {
			condition->notify_all();
			waiter.join();
			break;
		}
		else {
			std::unique_lock lock(*mutex);
			string_t bMsg(msg.data(), shared_memory.get_segment_manager());
			map->insert(std::pair<id_t, string_t>(++keptId, bMsg));
			lock.unlock();
			condition->notify_all();
		}
	}

	boost::interprocess::shared_memory_object::remove(shared_memory_name.c_str());
	return EXIT_SUCCESS;
}

