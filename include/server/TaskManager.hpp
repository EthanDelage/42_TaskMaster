#ifndef TASKMANAGER_HPP
#define TASKMANAGER_HPP

class TaskManager {
public:
  explicit TaskManager();

  void run();

private:
  void fsm();
};

#endif // TASKMANAGER_HPP
