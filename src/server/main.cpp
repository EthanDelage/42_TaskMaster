#include "common/Logger.hpp"
#include "server/ConfigParser.hpp"
#include "server/Taskmaster.hpp"
#include <csignal>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <pwd.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

#define DAEMON_USER "daemon"

static int daemon();
static int create_pidfile(uid_t uid, gid_t gid);
static int daemon_start(const char *daemon_user);

int main(int argc, char **argv) {
#ifndef DISABLE_DAEMON
  int pidfile_fd = -1;
#endif
  if (argc != 2) {
    std::cerr << "usage: " << argv[0] << " <config_file>" << std::endl;
    return 0;
  }
  try {
    Logger::init("./server.log");
    ConfigParser config(argv[1]);
    (void)config.parse();
#ifndef DISABLE_DAEMON
    Logger::get_instance().info("Starting Taskmasterd ...");
    std::cout << "Starting Taskmasterd ..." << std::endl;
    pidfile_fd = daemon_start(DAEMON_USER);
    if (pidfile_fd == -1) {
      return EXIT_FAILURE;
    }
    Logger::get_instance().debug("main: daemon started");
#endif
    Taskmaster taskmaster(config);
    taskmaster.loop();
  } catch (const std::exception &e) {
    Logger::get_instance().error(e.what());
    Logger::get_instance().info("Shutting down with failure...");
    return EXIT_FAILURE;
  }
#ifndef DISABLE_DAEMON
  Logger::get_instance().info(std::string("main: closing pidfile_fd=") +
                              std::to_string(pidfile_fd));
  close(pidfile_fd);
  Logger::get_instance().debug(std::string("main: unlink ") +
                               TASKMASTER_PIDFILE);
  unlink(TASKMASTER_PIDFILE);
#endif
  Logger::get_instance().info("Shutting down...");
  return EXIT_SUCCESS;
}

static int daemon_start(const char *daemon_user) {
  uid_t uid;
  gid_t gid;

  if (geteuid() != 0) {
    Logger::get_instance().error("daemon_start: Daemon must start as root");
    return -1;
  }

  struct passwd *pw = getpwnam(daemon_user);
  if (!pw) {
    Logger::get_instance().error(std::string(
        std::string("daemon_start: User '") + daemon_user + "' not found"));
    return -1;
  }

  uid = pw->pw_uid;
  gid = pw->pw_gid;

  int pidfd = create_pidfile(uid, gid);
  if (pidfd < 0) {
    return -1;
  }

  if (daemon() < 0) {
    Logger::get_instance().error(
        std::string("daemon_start: failed to daemon: ") + strerror(errno));
    return -1;
  }

  dprintf(pidfd, "%d\n", getpid());

  if (setgid(gid) < 0) {
    Logger::get_instance().error(std::string("daemon_start: setgid: ") +
                                 strerror(errno));
    return -1;
  }
  if (setuid(uid) < 0) {
    Logger::get_instance().error(std::string("daemon_start: setgid: ") +
                                 strerror(errno));
    return -1;
  }

  return pidfd;
}

static int daemon() {
  pid_t pid = fork();
  if (pid < 0) {
    Logger::get_instance().error(std::string("Taskmaster::daemon: fork(): ") +
                                 strerror(errno));
    return -1;
  }
  if (pid > 0) {
    Logger::get_instance().info("daemon: exiting parent process");
    exit(0);
  }

  if (setsid() < 0) {
    Logger::get_instance().error(std::string("Taskmaster::daemon: setsid(): ") +
                                 strerror(errno));
    return -1;
  }

  umask(0);

  close(STDIN_FILENO);
  close(STDOUT_FILENO);
  close(STDERR_FILENO);

  if (open("/dev/null", O_RDWR) != STDIN_FILENO) { /* 'fd' should be 0 */
    Logger::get_instance().error("daemon: STDIN fd not equal to 0");
    return -1;
  }
  if (dup2(STDIN_FILENO, STDOUT_FILENO) != STDOUT_FILENO) {
    Logger::get_instance().error("daemon: STDOUT fd not equal to 1");
    return -1;
  }
  if (dup2(STDIN_FILENO, STDERR_FILENO) != STDERR_FILENO) {
    Logger::get_instance().error("daemon: STDERR fd not equal to 1");
    return -1;
  }
  return 0;
}

static int create_pidfile(uid_t uid, gid_t gid) {
  int fd = open(TASKMASTER_PIDFILE, O_RDWR | O_CREAT, 0644);
  if (fd < 0) {
    Logger::get_instance().error(std::string("create_pidfile: open: ") +
                                 strerror(errno));
    return -1;
  }

  if (flock(fd, LOCK_EX | LOCK_NB) < 0) {
    if (errno == EWOULDBLOCK)
      Logger::get_instance().error("create_pidfile: Daemon already running !");
    else
      Logger::get_instance().error(std::string("create_pidfile: flock: ") +
                                   strerror(errno));
    close(fd);
    return -1;
  }

  if (ftruncate(fd, 0) < 0) {
    Logger::get_instance().error(std::string("create_pidfile: ftruncate: ") +
                                 strerror(errno));
    close(fd);
    return -1;
  }

  if (fchown(fd, uid, gid) < 0) {
    Logger::get_instance().error(std::string("create_pidfile: fchown: ") +
                                 strerror(errno));
    close(fd);
    return -1;
  }

  return fd;
}
