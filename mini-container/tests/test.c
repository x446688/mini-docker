#include "create.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>

enum {
  // ARGTABLE_ARG_MAX is the maximum number of arguments
  ARGTABLE_ARG_MAX = 20
};

/* global arg_xxx structs */
struct arg_lit *help, *version;
struct arg_int *uid;
struct arg_str *mnt;
struct arg_str *cmd;
struct arg_str *arg;
struct arg_lit *vrb;
struct arg_end *end;

int main(int argc, char **argv) {
  // used for container stack
  char *stack = 0;
  stack = malloc(CONTAINER_STACK_SIZE);
  // used for container config
  container_cfg config = {0};

  config.hostname = "mini";
  config.uid = 0;
  config.mnt = "/tmp/mini-docker";
  config.cmd = "/usr/bin/busybox";
  config.arg = "sh";
  
  // used for container pid
  int container_pid = 0;
  // socket pair
  int sockets[2] = {0};

  int exitcode = 0;
  char progname[] = "mini-docker";

  // Check if running as root
  if (geteuid() != 0) {
    printf("mini-docker should be run as root\n");
  }

  config.hostname = "mini";

  system("./tests/init.sh");

  // Initialize a socket pair to communicate with the container
  printf("initializing socket pair...\n");
  if (socketpair(AF_LOCAL, SOCK_SEQPACKET, 0, sockets)) {
    printf("failed to initialialize socket pair: %m\n");
    exitcode = 1;
    return exitcode;
  }

  printf("setting socket flags...\n");
  if (fcntl(sockets[0], F_SETFD, FD_CLOEXEC)) {
    printf("failed to socket fcntl: %m\n");
    exitcode = 1;
    return exitcode;
  }
  config.fd = sockets[1];

  printf("initializing container...\n");
  if ((container_pid = container_init(&config,stack+CONTAINER_STACK_SIZE)) ==
      -1) {
    printf("failed to container_init\n");
    exitcode = 1;
    return exitcode;
  }
  printf("container pid: %d\n",container_pid);
  // Prepare cgroups for the process 
  printf("initializing cgroups...\n");
  if (cgroups_init(config.hostname, container_pid) == -1) {
    printf("failed to initialize cgroups\n");
    exitcode = 1;
    return exitcode;
  }

  printf("configuring user namespace...\n");
  if (user_namespace_prepare_mappings(container_pid, sockets[0])) {
    exitcode = 1;
    printf("failed to user_namespace_set_user: %s\nstopping container...\n", strerror(errno));
    return exitcode;
  }
  printf("waiting for container to exit...\n");
  exitcode |= container_wait(container_pid);
  printf("container exited\n");
  // Clear resources (cgroups, stack, sockets)

  printf("freeing resources...\n");

  printf("freeing stack...\n");
  free(stack);

  printf("freeing sockets...\n");
  close(sockets[0]);
  close(sockets[1]);

  printf("freeing cgroups...\n");
  cgroups_free(config.hostname);
  return exitcode;
}
