#include <string.h>
#include <sys/types.h>
#ifndef __CONTAINER_CREATE
#define CGROUPS_MEMORY_MAX "1G"
#define CGROUPS_CPU_WEIGHT "256"
#define CGROUPS_PIDS_MAX "64"
#define CGROUPS_CONTROL_FIELD_SIZE 256
#define CGROUPS_CGROUP_PROCS "cgroup.procs"
#define USER_NAMESPACE_UID_PARENT_RANGE_START 0
#define USER_NAMESPACE_UID_CHILD_RANGE_START 10000
#define USER_NAMESPACE_UID_CHILD_RANGE_SIZE 2000
#define CONTAINER_STACK_SIZE 1024 * 1024
#define LOGFILE "/var/log/mini-docker.log"
#define INNER_LOGFILE "./mini-docker.log"

typedef struct
{
  uid_t uid;
  int fd;
  char *hostname;
  char *cmd;
  char *arg;
  char *mnt;
} container_cfg;
typedef struct
{
  char name[CGROUPS_CONTROL_FIELD_SIZE];
  char value[CGROUPS_CONTROL_FIELD_SIZE];
} cgroups_setting;
int __on_start (void *arg);
int container_init (container_cfg * container_config, char *stack);
int cgroups_free (char *hostname);
int user_namespace_prepare_mappings (pid_t pid, int fd);
int cgroups_init (char *hostname, pid_t pid);
void container_stop (int container_pid);
int container_wait (int container_pid);
int mount_set (char *mnt);
#endif
