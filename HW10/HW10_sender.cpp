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
#include <atomic>
#include <fstream>

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
void waiter(map_t* map, id_t id, int& keptId, mx* mutex, cn* condition, std::atomic<bool>& exitCondition) {
	while (true) {
		{
			std::unique_lock lock{ *mutex };
			condition->wait(lock, [map, &keptId, &exitCondition]() { return !map->empty() || exitCondition && (std::prev(map->end()))->first != keptId; });

			if (exitCondition) {
				break;
			}

			id_t last = map->rbegin()->first;
			if (last != keptId) {
				std::cout << map->at(last) << std::endl;
				keptId = map->rbegin()->first;
			}
		}
	}
}

//При аварийном завершении программы - раскоментить дефайн, запустить программу, закоментить обратно
//#define CLEAN_SHARED

int main() {
	const std::string shared_memory_name = "managed_shared_memory";
#ifdef CLEAN_SHARED
	boost::interprocess::shared_memory_object::remove(shared_memory_name.c_str());
	return 0;
#endif // CLEAN_SHARED

	/* creating */
	boost::interprocess::managed_shared_memory shared_memory(boost::interprocess::open_or_create, shared_memory_name.c_str(), 8096);
	std::string mutex_name = "mutex";
	std::string condition_name = "condition";
	record_allocator_t alloc{ shared_memory.get_segment_manager() };
	auto mutex = shared_memory.find_or_construct<mx>(mutex_name.c_str())();
	auto condition = shared_memory.find_or_construct<cn>(condition_name.c_str())();
	auto map = shared_memory.find_or_construct <map_t>("map")(std::less<id_t>(), alloc);
	auto clientsOnline = shared_memory.find_or_construct < std::atomic<int> >("clientsOnline")(0);
	std::ofstream logFile("messageLog.txt", std::ios::app);

	/* working */
	string_t msg{ shared_memory.get_segment_manager() };
	int keptId = -1;
	std::atomic<bool> exitCondition = false;
	{
		std::lock_guard lock(*mutex);
		if (!map->empty()) {
			keptId = map->size();
		}
	}
	(*clientsOnline)++;
	std::cout << "Clients: " << *clientsOnline << std::endl;
	std::thread waiter(waiter, map, keptId, std::ref(keptId), mutex, condition, std::ref(exitCondition));
	while (boost::container::getline(std::cin, msg)) {
		if (msg == "/exit/") {
			exitCondition = true;
			condition->notify_all();
			waiter.join();
			(*clientsOnline)--;
			break;
		}
		else {
			std::scoped_lock lock(*mutex);
			logFile << msg << std::endl;
			map->emplace(++keptId, msg);
			condition->notify_all();
		}
	}
	if (*clientsOnline == 0) {
		std::cout << "Removed shared memory" << std::endl;
		boost::interprocess::shared_memory_object::remove(shared_memory_name.c_str());
	}
	return EXIT_SUCCESS;
}



