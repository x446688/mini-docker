#define _GNU_SOURCE 1
#include "create.h"
#include "logger.h"
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/capability.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <sys/mount.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <sched.h>
#include <errno.h>
#include <grp.h>
#include <linux/capability.h>
#include <linux/limits.h>
#include <linux/prctl.h>
#include <linux/sched.h>

int
user_namespace_init (uid_t uid, int fd)
{
  int unshared = unshare (CLONE_NEWUSER);
  int result = 0;

  logdoc (LOG_LVL_INFO, LOGFILE, "setting user namespace...");

  logdoc (LOG_LVL_INFO, LOGFILE, "writing to socket...");
  if (write (fd, &unshared, sizeof (unshared)) != sizeof (unshared))
    {
      logdoc (LOG_LVL_INFO, LOGFILE, "failed to write socket");
      return -1;
    }

  logdoc (LOG_LVL_INFO, LOGFILE, "reading from socket...");
  if (read (fd, &result, sizeof (result)) != sizeof (result))
    {
      logdoc (LOG_LVL_INFO, LOGFILE, "failed to read from socket");
      return -1;
    }

  if (result)
    {
      return -1;
    }

  logdoc (LOG_LVL_INFO, LOGFILE, "switching to uid / gid...");

  logdoc (LOG_LVL_INFO, LOGFILE, "setting uid and gid mappings...");
  if (setgroups (1, &(gid_t)
                 {
                 uid}
      ) == -1 && errno != EPERM)
    {
      logdoc (LOG_LVL_ERROR, LOGFILE, "setgroups failed");
      return -1;
    }
  if (setresgid (uid, uid, uid) || setresuid (uid, uid, uid))
    {
      logdoc (LOG_LVL_ERROR, LOGFILE, "failed to set uid / gid mappings");
      return -1;
    }

  logdoc (LOG_LVL_INFO, LOGFILE, "user namespace set");

  return 0;
}

// pivot_root is a system call to swap the mount at / with another.
// glibc does not provide a wrapper for it.
long
pivot_root (const char *new_root, const char *put_old)
{
  logdoc (LOG_LVL_INFO, LOGFILE, "calling pivot_root syscall...");
  return syscall (SYS_pivot_root, new_root, put_old);
}

int
__set_capabilites ()
{
  int drop_caps[] = {
    CAP_AUDIT_CONTROL, CAP_AUDIT_READ, CAP_AUDIT_WRITE, CAP_BLOCK_SUSPEND,
    CAP_DAC_READ_SEARCH, CAP_FSETID, CAP_IPC_LOCK, CAP_MAC_ADMIN,
    CAP_MAC_OVERRIDE, CAP_MKNOD, CAP_SETFCAP, CAP_SYSLOG,
    CAP_SYS_ADMIN, CAP_SYS_BOOT, CAP_SYS_MODULE, CAP_SYS_NICE,
    CAP_SYS_RAWIO, CAP_SYS_RESOURCE, CAP_SYS_TIME, CAP_WAKE_ALARM
  };
  for (int i = 0; i < sizeof (drop_caps) / sizeof (int); ++i)
    {
      prctl (PR_CAPBSET_DROP, drop_caps[i], 0, 0, 0);
    }
  cap_t caps = NULL;
  caps = cap_get_proc ();
  if (!caps)
    {
      logdoc (LOG_LVL_ERROR, LOGFILE, "cap_get_proc failed");
      return -1;
    }
  if (cap_set_flag
      (caps, CAP_PERMITTED, sizeof (drop_caps) / sizeof (int), drop_caps,
       CAP_CLEAR)
      || cap_set_flag (caps, CAP_EFFECTIVE, sizeof (drop_caps) / sizeof (int),
                       drop_caps, CAP_CLEAR)
      || cap_set_flag (caps, CAP_INHERITABLE,
                       sizeof (drop_caps) / sizeof (int), drop_caps,
                       CAP_CLEAR))
    {
      logdoc (LOG_LVL_ERROR, LOGFILE, "cap_set_flag failed");
      cap_free (caps);
      return -1;
    }
  if (cap_set_proc (caps) == -1)
    {
      logdoc (LOG_LVL_ERROR, LOGFILE, "cap_set_proc failed");
      cap_free (caps);
      return -1;
    }

  cap_free (caps);
  return 0;
}

// syslog module is required to correctly display errors
int
__on_start (void *arg)
{
  container_cfg *config = arg;
  logdoc (LOG_LVL_INFO, LOGFILE, "%s, %s, %s", config->mnt, config->cmd,
          config->arg);
  logdoc (LOG_LVL_ERROR, LOGFILE, "DEBUG hostname bytes:");
  logdoc (LOG_LVL_ERROR, LOGFILE, config->hostname);

  logdoc (LOG_LVL_INFO, LOGFILE, "Setting hostname");
  if (sethostname ("mini", 4))
    {
      logdoc (LOG_LVL_ERROR, LOGFILE, "sethostname failed");
      logdoc (LOG_LVL_ERROR, LOGFILE, strerror (errno));
      close (config->fd);
      return -1;
    }

  logdoc (LOG_LVL_INFO, LOGFILE, "Setting mountpoint");
  if (mount_set (config->mnt))
    {
      logdoc (LOG_LVL_ERROR, LOGFILE, "mount failed");
      logdoc (LOG_LVL_ERROR, LOGFILE, strerror (errno));
      close (config->fd);
      return -1;
    }

  logdoc (LOG_LVL_INFO, LOGFILE, "Setting namespace");
  if (user_namespace_init (config->uid, config->fd))
    {
      logdoc (LOG_LVL_ERROR, LOGFILE, "failed to init user namespace");
      logdoc (LOG_LVL_ERROR, LOGFILE, strerror (errno));
      close (config->fd);
      return -1;
    }

  logdoc (LOG_LVL_INFO, LOGFILE, "Setting capabilities");
  if (__set_capabilites ())
    {
      logdoc (LOG_LVL_ERROR, LOGFILE, "failed to set caps");
      logdoc (LOG_LVL_ERROR, LOGFILE, strerror (errno));
      close (config->fd);
      return -1;
    }
  if (close (config->fd))
    {
      logdoc (LOG_LVL_ERROR, LOGFILE, "closing container socket failed");
      logdoc (LOG_LVL_ERROR, LOGFILE, strerror (errno));
      return -1;
    }
  // argv must be NULL terminated
  // IM DEBUGGING THIS
  char buff[256];
  getcwd (buff, 256);
  // const char* command = config->cmd;
  char *newargv[] = { config->cmd, NULL };
  for (int i = 0; newargv[i] != NULL; ++i)
    {
      logdoc (LOG_LVL_ERROR, LOGFILE, "arg[%d]: %s", i, newargv[i]);
    }
  logdoc (LOG_LVL_INFO, LOGFILE, "Running \"%s\" in %s", newargv[0], buff);
  if (execve (newargv[0], newargv, NULL) == -1)
    {
      logdoc (LOG_LVL_ERROR, LOGFILE,
              "Failed to run %s in container (execve): %s", newargv[0],
              strerror (errno));
      return -1;
    }

  return 0;
}

int
cgroups_init (char *hostname, pid_t pid)
{
  logdoc (LOG_LVL_INFO, LOGFILE, "begin cgroups_init\n");
  char cgroup_dir[PATH_MAX] = { 0 };
  cgroups_setting *procs_setting = &(cgroups_setting) {.name =
      CGROUPS_CGROUP_PROCS,.value = "" };
  snprintf (procs_setting->value, CGROUPS_CONTROL_FIELD_SIZE, "%d", pid);
  logdoc (LOG_LVL_INFO, LOGFILE, "creating cgroup settings\n");
  cgroups_setting cgroups_settings[] = {
    {.name = "memory.max",
     .value = CGROUPS_MEMORY_MAX},
    {.name = "cpu.weight",
     .value = CGROUPS_CPU_WEIGHT},
    {.name = "pids.max",
     .value = CGROUPS_PIDS_MAX},
    *procs_setting
  };
  logdoc (LOG_LVL_WARNING, LOGFILE, "initializing cgroup_dir...");
  snprintf (cgroup_dir, sizeof (cgroup_dir), "/sys/fs/cgroup/%s", hostname);
  logdoc (LOG_LVL_INFO, LOGFILE, "creating cgroup_dir directory (%s)\n",
          cgroup_dir);
  if (mkdir (cgroup_dir, S_IRUSR | S_IWUSR | S_IXUSR) == -1
      && errno != EEXIST)
    {
      logdoc (LOG_LVL_INFO, LOGFILE, "failed to create cgroup_dir: %s\n",
              strerror (errno));
      return -1;
    }
  for (int i = 0; i < 4; ++i)
    {
      char setting_dir[PATH_MAX] = { 0 };
      int fd = 0;
      snprintf (setting_dir, sizeof (setting_dir), "%s/%s", cgroup_dir,
                cgroups_settings[i].name);
      fd = open (setting_dir, O_WRONLY);
      write (fd, cgroups_settings[i].value,
             strlen (cgroups_settings[i].value));
      close (fd);
    }
  return 0;
}

int
user_namespace_prepare_mappings (pid_t pid, int fd)
{
  int map_fd = 0;
  int unshared = -1;
  int n;
  logdoc (LOG_LVL_INFO, LOGFILE,
          "preparing namespace mappings to pid %d and fd %d\n", pid, fd);

  if ((n = read (fd, &unshared, sizeof (unshared))) != sizeof (unshared))
    {
      logdoc (LOG_LVL_INFO, LOGFILE,
              "failed to retrieve status from socket: %d vs %d\n",
              sizeof (unshared), n);
      return -1;
    }
  if (unshared != -1)
    {
      char setgroups_path[PATH_MAX];
      snprintf (setgroups_path, sizeof (setgroups_path), "/proc/%d/setgroups",
                pid);
      int setgroups_fd = open (setgroups_path, O_WRONLY);
      if (setgroups_fd >= 0)
        {
          if (write (setgroups_fd, "deny", 4) < 0)
            {
              // Non-fatal: older kernels may not have this file
              logdoc (LOG_LVL_INFO, LOGFILE,
                      "warning: could not write to setgroups: %m\n");
            }
          close (setgroups_fd);
        }
    }
  if (unshared != -1)
    {
      char dir[PATH_MAX] = { 0 };
      for (char **file = (char *[])
           { "uid_map", "gid_map", 0 }; *file; file++)
        {
          if (snprintf (dir, sizeof (dir), "/proc/%d/%s", pid, *file) >
              (int) sizeof (dir))
            {
              return -1;
            }

          if ((map_fd = open (dir, O_WRONLY)) == -1)
            {
              logdoc (LOG_LVL_INFO, LOGFILE, "error opening map_fd");
              return -1;
            }

          dprintf (map_fd, "0 0 2000\n");
          close (map_fd);
        }
    }
  write (fd, &(int)
         { 0 }, sizeof (int));
  logdoc (LOG_LVL_INFO, LOGFILE, "user namespace mappings prepared...\n");
  return 0;
}

int
cgroups_free (char *hostname)
{
  char dir[PATH_MAX] = { 0 };
  logdoc (LOG_LVL_INFO, LOGFILE, "freeing cgroups...\n");
  snprintf (dir, sizeof (dir), "/sys/fs/cgroup/%s", hostname);
  rmdir (dir);
  return 0;
}

int
container_init (container_cfg *container_config, char *stack)
{
  int container_pid = 0;
  int flags = CLONE_NEWNS | CLONE_NEWCGROUP | CLONE_NEWPID | CLONE_NEWIPC |
    CLONE_NEWNET | CLONE_NEWUTS;
  logdoc (LOG_LVL_INFO, LOGFILE, "stack at: %p\n", stack);
  if ((container_pid =
       clone (__on_start, stack, flags | SIGCHLD, container_config)) == -1)
    {
      logdoc (LOG_LVL_INFO, LOGFILE, "failed to clone!\n");
      return 1;
    }

  printf ("resetting container socket...\n");
  close (container_config->fd);
  container_config->fd = 0;

  return container_pid;
}

int
container_wait (int container_pid)
{
  int container_status = 0;

  waitpid (container_pid, &container_status, 0);

  return WEXITSTATUS (container_status);
}

void
container_stop (int container_pid)
{
  kill (container_pid, SIGKILL);
}

int
mount_set (char *mnt)
{
  logdoc (LOG_LVL_INFO, LOGFILE, "setting mount to %s...\n", mnt);

  logdoc (LOG_LVL_INFO, LOGFILE, "remounting / with MS_PRIVATE...\n");
  if (mount (NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL))
    {
      logdoc (LOG_LVL_INFO, LOGFILE, "failed to remount /: %m\n");
      return -1;
    }

  struct stat st;
  if (stat (mnt, &st) == -1 || !S_ISDIR (st.st_mode))
    {
      logdoc (LOG_LVL_INFO, LOGFILE,
              "error: %s is not a valid directory: %m\n", mnt);
      return -1;
    }

  char oldroot_path[PATH_MAX];
  snprintf (oldroot_path, sizeof (oldroot_path), "%s/.oldroot", mnt);

  logdoc (LOG_LVL_INFO, LOGFILE, "creating oldroot directory: %s\n",
          oldroot_path);
  if (mkdir (oldroot_path, 0755) == -1 && errno != EEXIST)
    {
      logdoc (LOG_LVL_INFO, LOGFILE, "failed to create oldroot: %m\n");
      return -1;
    }

  logdoc (LOG_LVL_INFO, LOGFILE, "bind-mounting %s to itself...\n", mnt);
  if (mount (mnt, mnt, NULL, MS_BIND | MS_PRIVATE, NULL))
    {
      logdoc (LOG_LVL_INFO, LOGFILE, "failed to bind mount %s: %m\n", mnt);
      rmdir (oldroot_path);
      return -1;
    }

  logdoc (LOG_LVL_INFO, LOGFILE, "calling pivot_root(%s, %s)...\n", mnt,
          oldroot_path);
  if (pivot_root (mnt, oldroot_path))
    {
      logdoc (LOG_LVL_INFO, LOGFILE, "pivot_root failed: %m\n");
      return -1;
    }

  logdoc (LOG_LVL_INFO, LOGFILE, "changing directory to /...\n");
  if (chdir ("/"))
    {
      logdoc (LOG_LVL_INFO, LOGFILE, "failed to chdir /: %m\n");
      return -1;
    }

  logdoc (LOG_LVL_INFO, LOGFILE, "unmounting old root...\n");
  if (umount2 ("/.oldroot", MNT_DETACH))
    {
      logdoc (LOG_LVL_INFO, LOGFILE,
              "warning: failed to umount /.oldroot: %m\n");
    }

  logdoc (LOG_LVL_INFO, LOGFILE, "removing oldroot directory...\n");
  if (rmdir ("/.oldroot"))
    {
      logdoc (LOG_LVL_INFO, LOGFILE,
              "warning: failed to rmdir /.oldroot: %m\n");
    }

  logdoc (LOG_LVL_INFO, LOGFILE, "mount set successfully\n");
  return 0;
}
