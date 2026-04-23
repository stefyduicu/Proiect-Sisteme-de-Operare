#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#define NAME_LEN 32
#define CATEGORY_LEN 16
#define DESC_LEN 128
typedef struct {
  int id;
  char inspector[NAME_LEN];
  double latitude;
  double longitude;
  char category[CATEGORY_LEN];
  int severity;
  long timestamp;
  char description[DESC_LEN];
} Report;
void get_path(char *out, const char *district, const char *file) {
  sprintf(out, "%s/%s", district, file);
}
void ensure_district(const char *district) {
  struct stat st;
  if (stat(district, &st) == -1) {
    mkdir(district, S_IRWXU | S_IRGRP | S_IXGRP);
  }
  char path[256];
  int fd;
  get_path(path, district, "reports.dat");
  fd = open(path, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
  if (fd != -1) {
    close(fd);
    chmod(path, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
  }
  get_path(path, district, "district.cfg");
  fd = open(path, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP);
  if (fd != -1) {
    close(fd);
    chmod(path, S_IRUSR | S_IWUSR | S_IRGRP);
  }
  get_path(path, district, "logged_district");
  fd = open(path, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
  if (fd != -1) {
    close(fd);
    chmod(path, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
  }
}
void check_write(const char *path, int is_manager) {
  struct stat st;
  if (stat(path, &st) == -1)
    {
    return;
    }
  if (is_manager != 0) {
    if ((st.st_mode & S_IWUSR) == 0) {
        write(2, "Manager has no write permission\n", 32);
        exit(1);
    }
  } else {
    if ((st.st_mode & S_IWGRP) == 0) {
        write(2, "Inspector has no write permission\n", 34);
        exit(1);
    }
  }
}
void log_action(const char *district, const char *user, const char *role, const char *action) {
  char path[256];
  get_path(path, district, "logged_district");
  int fd = open(path, O_WRONLY | O_APPEND);
  if (fd == -1)
    return;
  char buf[256];
  sprintf(buf, "%ld | %s | %s | %s\n", time(NULL), user, role, action);
  write(fd, buf, strlen(buf));
  close(fd);
}
void add(const char *district, const char *user, const char *role) {
  ensure_district(district);
  char path[256];
  get_path(path, district, "reports.dat");
  int fd = open(path, O_RDWR);
  if (fd == -1) return;
  Report r;
  off_t size = lseek(fd, 0, SEEK_END);
  r.id = (int)(size / sizeof(Report));
  memset(r.inspector, 0, NAME_LEN);
  strncpy(r.inspector, user, NAME_LEN - 1);
  printf("lat lon cat sev desc:\n");
  scanf("%lf %lf %15s %d", &r.latitude, &r.longitude, r.category, &r.severity);
  getchar();
  fgets(r.description, DESC_LEN, stdin);
  int len = strlen(r.description);
  if (len > 0 && r.description[len - 1] == '\n') {
    r.description[len - 1] = '\0';
  }
  r.timestamp = time(NULL);
  write(fd, &r, sizeof(Report));
  close(fd);
  log_action(district, user, role, "ADD");
}
void print_perm(mode_t m) {
  char p[10];
  if ((m & S_IRUSR) != 0)
    p[0] = 'r';
  else
    p[0] = '-';
  if ((m & S_IWUSR) != 0)
    p[1] = 'w';
  else
    p[1] = '-';
  if ((m & S_IXUSR) != 0)
    p[2] = 'x';
  else
    p[2] = '-';
  if ((m & S_IRGRP) != 0)
    p[3] = 'r';
  else
    p[3] = '-';
  if ((m & S_IWGRP) != 0)
    p[4] = 'w';
  else
    p[4] = '-';
  if ((m & S_IXGRP) != 0)
    p[5] = 'x';
  else
    p[5] = '-';
  if ((m & S_IROTH) != 0) p[6] = 'r';
    else p[6] = '-';
  if ((m & S_IWOTH) != 0)
    p[7] = 'w';
  else
    p[7] = '-';
  if ((m & S_IXOTH) != 0)
    p[8] = 'x';
  else p[8] = '-';
    p[9] = '\0';
  printf("Perm: %s\n", p);
}
void list(const char *district) {
  char path[256];
  get_path(path, district, "reports.dat");
  int fd = open(path, O_RDONLY);
  if (fd == -1)
    return;
  Report r;
  while (read(fd, &r, sizeof(Report)) == sizeof(Report)) {
    printf("%d | %s | %s | %d\n", r.id, r.inspector, r.category, r.severity);
  }
  struct stat st;
  if (stat(path, &st) == 0) {
    print_perm(st.st_mode);
  }
  close(fd);
}
int main(int argc, char **argv) {
  if (argc < 4) {
    return 0;
  }
  const char *district = argv[1];
  const char *user = argv[2];
  const char *role = argv[3];
  if (argc == 5 && strcmp(argv[4], "list") == 0) {
    list(district);
  } else {
    add(district, user, role);
  }
  return 0;
}
