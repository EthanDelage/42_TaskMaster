#ifndef PROCESSPOOL_HPP
#define PROCESSPOOL_HPP

#include "server/ProcessGroup.hpp"

#include <mutex>

class ProcessPool {
  using PoolType = std::unordered_map<std::string, ProcessGroup>;
  using PoolIterator = PoolType::iterator;
  using ConstPoolIterator = PoolType::const_iterator;

public:
  ProcessPool();
  ProcessPool(std::unordered_map<std::string, process_config_t> &&config_map);

  ProcessPool &operator=(ProcessPool &&other) noexcept;

  void emplace(process_config_t &&process_config);
  PoolIterator erase(PoolIterator it);
  PoolIterator find(std::string const &key);
  PoolType::node_type extract(std::string const &key);
  void move_from(ProcessPool &other, std::string const &key);
  bool empty() const;

  std::unordered_map<std::string, ProcessGroup> &get_pool();
  std::mutex &get_mutex();

  PoolIterator begin();
  ConstPoolIterator begin() const;
  ConstPoolIterator cbegin() const;
  PoolIterator end();
  ConstPoolIterator end() const;
  ConstPoolIterator cend() const;

private:
  std::unordered_map<std::string, ProcessGroup> _process_pool;
  std::mutex _mutex;
};

std::ostream &operator<<(std::ostream &os, const ProcessPool &process_pool);

#endif // PROCESSPOOL_HPP
