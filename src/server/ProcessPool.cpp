#include "server/ProcessPool.hpp"

#include <iostream>
#include <unordered_map>

ProcessPool::ProcessPool() = default;

ProcessPool::ProcessPool(std::unordered_map<std::string, process_config_t>&& config_map) {
  for (auto &[name, config] : config_map) {
    _process_pool.emplace(name, std::move(config));
  }
}

ProcessPool& ProcessPool::operator=(ProcessPool&& other) noexcept
{
  if (this != &other)
  {
    _process_pool = std::move(other._process_pool);
  }
  return *this;
}

void ProcessPool::emplace(process_config_t&& process_config) {
  _process_pool.emplace(process_config.name, std::move(process_config));
}

ProcessPool::PoolIterator ProcessPool::erase(PoolIterator it) {
  return _process_pool.erase(it);
}

ProcessPool::PoolIterator ProcessPool::find(std::string const & key) {
  return _process_pool.find(key);
}

ProcessPool::PoolType::node_type ProcessPool::extract(std::string const & key) {
  return _process_pool.extract(key);
}

void ProcessPool::move_from(ProcessPool& other, std::string const &key) {
  auto node = other.extract(key);
  _process_pool.insert(std::move(node));
}

std::mutex &ProcessPool::get_mutex() { return _mutex; }

std::unordered_map<std::string, ProcessGroup>& ProcessPool::get_pool() {
  return _process_pool;
}

ProcessPool::PoolIterator ProcessPool::begin() {
  return _process_pool.begin();
}

ProcessPool::ConstPoolIterator ProcessPool::begin() const {
  return _process_pool.begin();
}

ProcessPool::ConstPoolIterator ProcessPool::cbegin() const {
  return _process_pool.cbegin();
}

ProcessPool::PoolIterator ProcessPool::end() {
  return _process_pool.end();
}

ProcessPool::ConstPoolIterator ProcessPool::end() const {
  return _process_pool.end();
}

ProcessPool::ConstPoolIterator ProcessPool::cend() const {
  return _process_pool.cend();
}

std::ostream &operator<<(std::ostream &os, const ProcessPool &process_pool) {
  for (auto& [name, process_group] : process_pool) {
    os << process_group;
  }
  return os;
}
