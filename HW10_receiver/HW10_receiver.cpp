#define BOOST_DATE_TIME_NO_LIB

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <mutex>
#include <chrono>
#include <string>
#include <boost/interprocess/mapped_region.hpp>
#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/containers/map.hpp>
#include <boost/interprocess/containers/string.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/sync/interprocess_mutex.hpp>
#include <boost/interprocess/sync/interprocess_condition.hpp>


using segment_manager_t = boost::interprocess::managed_shared_memory::segment_manager;
using char_allocator_t = boost::interprocess::allocator < char, segment_manager_t>;
using string_t = boost::interprocess::basic_string < char, std::char_traits < char >, char_allocator_t>;
using id_t = std::size_t;
using record_t = std::pair <const id_t, string_t>;
using record_allocator_t = boost::interprocess::allocator <record_t, segment_manager_t>;
using map_t = boost::interprocess::map <id_t, string_t, std::less <id_t>, record_allocator_t>;

class Client {
private:
	const std::string shared_memory_name = "managed_shared_memory";
	std::string mutex_name = "mutex";
	std::string condition_name = "condition";
	boost::interprocess::interprocess_mutex* mutex;
	boost::interprocess::interprocess_condition* condition;
	map_t* map;

	Client() {
		boost::interprocess::shared_memory_object::remove(shared_memory_name.c_str());
		boost::interprocess::managed_shared_memory shared_memory(boost::interprocess::open_or_create, shared_memory_name.c_str(), 1024);
		mutex = shared_memory.find_or_construct<boost::interprocess::interprocess_mutex>(mutex_name.c_str())();
		condition = shared_memory.find_or_construct<boost::interprocess::interprocess_condition>(condition_name.c_str())();
		map = shared_memory.construct <map_t>("map")(shared_memory.get_segment_manager());
		string_t msg(shared_memory.get_segment_manager());
	}

	void f() {
	}

};