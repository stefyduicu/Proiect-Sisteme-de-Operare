#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include<sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#define NAME_LEN 64
#define CATEGORY_LEN 32
#define DESC_LEN 256
#define DISTRICT_LEN 64
#define FIELD_LEN 32
#define OP_LEN 4
#define VALUE_LEN 128
#define LOG_LINE_LEN 512
#define PERM_DIR 0750
#define PERM_REPORTS 0664
#define PERM_CFG 0640
#define PERM_LOG 0644
#define ROLE_INSPECTOR "inspector"
#define ROLE_MANAGER "manager"
#define REPORTS_FILE "reports.dat"
#define CONFIG_FILE "district.cfg"
#define LOG_FILE "logged_district"
typedef struct {
    int id;
    char inspector[NAME_LEN];
    double latitude;
    double longitude;
    char category[CATEGORY_LEN];
    int severity;
    time_t timestamp;
    char description[DESC_LEN];
} Report;
typedef struct {
    char role[NAME_LEN];
    char user[NAME_LEN];
    char command[NAME_LEN];
    char district[DISTRICT_LEN];
    int report_id;
    int threshold;
    char conditions[8][VALUE_LEN];
    int num_conditions;
} Args;
static void parse_args(int argc, char *argv[], Args *a);
static void usage(const char *prog);
static void set_permissions(const char *path, mode_t mode);
static void check_permission(const char *path, mode_t required_bit, const char *role);
static char *mode_to_str(mode_t mode, char buf[10]);
static void log_action(const char *district, const char *role, const char *user, const char *action);
static void build_path(char *out, size_t sz, const char *district, const char *file);
static void ensure_district(const char *district);
static void create_symlink(const char *district);
static void print_report(const Report *r);
static void cmd_add(const Args *a);
static void cmd_list(const Args *a);
static void cmd_view(const Args *a);
static void cmd_remove_report(const Args *a);
static void cmd_remove_district(const Args *a);
static void cmd_update_threshold(const Args *a);
static void cmd_filter(const Args *a);
static int parse_condition(const char *input, char *field, char *op, char *value);
static int match_condition(Report *r, const char *field, const char *op, const char *value);
static void usage(const char *prog)
{
    fprintf(stderr, "Utilizare:\n");
    fprintf(stderr, "  %s --role <manager|inspector> --user <nume> --add <cartier>\n", prog);
    fprintf(stderr, "  %s --role <manager|inspector> --user <nume> --list <cartier>\n", prog);
    fprintf(stderr, "  %s --role <manager|inspector> --user <nume> --view <cartier> <id>\n", prog);
    fprintf(stderr, "  %s --role manager             --user <nume> --remove_report <cartier> <id>\n", prog);
    fprintf(stderr, "  %s --role manager             --user <nume> --update_threshold <cartier> <valoare>\n", prog);
    fprintf(stderr, "  %s --role <manager|inspector> --user <nume> --filter <cartier> <conditie> [conditii...]\n", prog);
    fprintf(stderr, "  %s --role manager             --user <nume> --remove_district <cartier>\n",prog);
    exit(EXIT_FAILURE);
}

static void parse_args(int argc, char *argv[], Args *a)
{
    memset(a, 0, sizeof(*a));

    if (argc < 2) {
        usage(argv[0]);
    }
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--role") == 0 && i + 1 < argc) {
            i++;
            strncpy(a->role, argv[i], NAME_LEN - 1);
        } else if (strcmp(argv[i], "--user") == 0 && i + 1 < argc) {
            i++;
            strncpy(a->user, argv[i], NAME_LEN - 1);
        } else if (strcmp(argv[i], "--add") == 0 && i + 1 < argc) {
            strncpy(a->command, "add", NAME_LEN - 1);
            i++;
            strncpy(a->district, argv[i], DISTRICT_LEN - 1);
        } else if (strcmp(argv[i], "--list") == 0 && i + 1 < argc) {
            strncpy(a->command, "list", NAME_LEN - 1);
            i++;
            strncpy(a->district, argv[i], DISTRICT_LEN - 1);
        } else if (strcmp(argv[i], "--view") == 0 && i + 2 < argc) {
            strncpy(a->command, "view", NAME_LEN - 1);
            i++;
            strncpy(a->district, argv[i], DISTRICT_LEN - 1);
            i++;
            a->report_id = atoi(argv[i]);
        } else if (strcmp(argv[i], "--remove_report") == 0 && i + 2 < argc) {
            strncpy(a->command, "remove_report", NAME_LEN - 1);
            i++;
            strncpy(a->district, argv[i], DISTRICT_LEN - 1);
            i++;
            a->report_id = atoi(argv[i]);
        }else if (strcmp(argv[i], "--remove_district") == 0 && i + 2 < argc) {
            strncpy(a->command, "remove_district", NAME_LEN - 1);
            i++;
            strncpy(a->district, argv[i], DISTRICT_LEN - 1);
            i++;
            a->report_id = atoi(argv[i]);
        }
	else if (strcmp(argv[i], "--update_threshold") == 0 && i + 2 < argc) {
            strncpy(a->command, "update_threshold", NAME_LEN - 1);
            i++;
            strncpy(a->district, argv[i], DISTRICT_LEN - 1);
            i++;
            a->threshold = atoi(argv[i]);
        } else if (strcmp(argv[i], "--filter") == 0 && i + 1 < argc) {
            strncpy(a->command, "filter", NAME_LEN - 1);
            i++;
            strncpy(a->district, argv[i], DISTRICT_LEN - 1);
            while (i + 1 < argc && argv[i + 1][0] != '-') {
                if (a->num_conditions < 8) {
                    i++;
                    strncpy(a->conditions[a->num_conditions], argv[i], VALUE_LEN - 1);
                    a->num_conditions++;
                } else {
                    i++;
                }
            }
        }
    }

    if (a->role[0] == '\0') {
        fprintf(stderr, "Eroare: Lipseste --role.\n");
        usage(argv[0]);
    }

    if (strcmp(a->role, ROLE_INSPECTOR) != 0 && strcmp(a->role, ROLE_MANAGER) != 0) {
        fprintf(stderr, "Eroare: Rol invalid.\n");
        usage(argv[0]);
    }

    if (a->user[0] == '\0') {
        fprintf(stderr, "Eroare: Lipseste --user.\n");
        usage(argv[0]);
    }

    if (a->command[0] == '\0') {
        fprintf(stderr, "Eroare: Lipseste comanda.\n");
        usage(argv[0]);
    }
}

static char *mode_to_str(mode_t mode, char buf[10])
{
    if (mode & S_IRUSR) buf[0] = 'r'; else buf[0] = '-';
    if (mode & S_IWUSR) buf[1] = 'w'; else buf[1] = '-';
    if (mode & S_IXUSR) buf[2] = 'x'; else buf[2] = '-';
    if (mode & S_IRGRP) buf[3] = 'r'; else buf[3] = '-';
    if (mode & S_IWGRP) buf[4] = 'w'; else buf[4] = '-';
    if (mode & S_IXGRP) buf[5] = 'x'; else buf[5] = '-';
    if (mode & S_IROTH) buf[6] = 'r'; else buf[6] = '-';
    if (mode & S_IWOTH) buf[7] = 'w'; else buf[7] = '-';
    if (mode & S_IXOTH) buf[8] = 'x'; else buf[8] = '-';
    buf[9] = '\0';
    return buf;
}

static void set_permissions(const char *path, mode_t mode)
{
    if (chmod(path, mode) < 0) {
        perror("Eroare la chmod");
    }
}

static void check_permission(const char *path, mode_t required_bit, const char *role)
{
    struct stat st;
    mode_t check_bit = required_bit;

    if (stat(path, &st) < 0) {
        fprintf(stderr, "Eroare la stat pe %s\n", path);
        exit(EXIT_FAILURE);
    }

    if (strcmp(role, ROLE_INSPECTOR) == 0) {
        if (required_bit == S_IRUSR) {
            check_bit = S_IRGRP;
        } else if (required_bit == S_IWUSR) {
            check_bit = S_IWGRP;
        } else if (required_bit == S_IXUSR) {
            check_bit = S_IXGRP;
        }
    }

    if (!(st.st_mode & check_bit)) {
        fprintf(stderr, "Eroare: Rolul '%s' nu are permisiuni pe fisier.\n", role);
        exit(EXIT_FAILURE);
    }
}

static void log_action(const char *district, const char *role, const char *user, const char *action)
{
    if (strcmp(role, ROLE_INSPECTOR) == 0) {
        return;
    }

    char log_path[512];
    snprintf(log_path, sizeof(log_path), "%s/%s", district, LOG_FILE);

    struct stat st;
    if (stat(log_path, &st) == 0) {
        if (!(st.st_mode & S_IWUSR)) {
            return;
        }
    }

    int fd = open(log_path, O_WRONLY | O_CREAT | O_APPEND, PERM_LOG);
    if (fd < 0) {
        return;
    }

    time_t now = time(NULL);
    char ts[64];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", localtime(&now));

    char line[LOG_LINE_LEN];
    int len = snprintf(line, sizeof(line), "[%s] role=%s user=%s action=%s\n", ts, role, user, action);
    write(fd, line, len);
    close(fd);

    set_permissions(log_path, PERM_LOG);
}

static void build_path(char *out, size_t sz, const char *district, const char *file)
{
    snprintf(out, sz, "%s/%s", district, file);
}

static void create_symlink(const char *district)
{
    char link_name[512];
    snprintf(link_name, sizeof(link_name), "active_reports-%s", district);

    char target[512];
    snprintf(target, sizeof(target), "%s/%s", district, REPORTS_FILE);

    struct stat lst;
    if (lstat(link_name, &lst) == 0) {
        if (S_ISLNK(lst.st_mode)) {
            struct stat target_st;
            if (stat(link_name, &target_st) < 0) {
                fprintf(stderr, "Warning: Symlink-ul vechi indica spre un fisier sters.\n");
            }
            unlink(link_name);
        } else {
            return;
        }
    }

    if (symlink(target, link_name) < 0) {
        perror("Eroare la creare symlink");
    }
}

static void ensure_district(const char *district)
{
    struct stat st;

    if (stat(district, &st) < 0) {
        if (mkdir(district, PERM_DIR) < 0) {
            perror("Nu am putut crea folderul cartierului");
            exit(EXIT_FAILURE);
        }
    }
    set_permissions(district, PERM_DIR);

    char rpath[512];
    build_path(rpath, sizeof(rpath), district, REPORTS_FILE);
    if (stat(rpath, &st) < 0) {
        int fd = open(rpath, O_WRONLY | O_CREAT, PERM_REPORTS);
        if (fd >= 0) {
            close(fd);
        }
    }
    set_permissions(rpath, PERM_REPORTS);

    char cpath[512];
    build_path(cpath, sizeof(cpath), district, CONFIG_FILE);
    if (stat(cpath, &st) < 0) {
        int fd = open(cpath, O_WRONLY | O_CREAT, PERM_CFG);
        if (fd >= 0) {
            const char *def = "severity_threshold=1\n";
            write(fd, def, strlen(def));
            close(fd);
        }
    }
    set_permissions(cpath, PERM_CFG);

    char lpath[512];
    build_path(lpath, sizeof(lpath), district, LOG_FILE);
    if (stat(lpath, &st) < 0) {
        int fd = open(lpath, O_WRONLY | O_CREAT, PERM_LOG);
        if (fd >= 0) {
            close(fd);
        }
    }
    set_permissions(lpath, PERM_LOG);

    create_symlink(district);
}

static void print_report(const Report *r)
{
    char ts[64];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", localtime(&r->timestamp));
    printf("ID: %d\n", r->id);
    printf("Inspector: %s\n", r->inspector);
    printf("GPS: %.6f, %.6f\n", r->latitude, r->longitude);
    printf("Categorie: %s\n", r->category);
    printf("Gravitate: %d\n", r->severity);
    printf("Data: %s\n", ts);
    printf("Descriere: %s\n", r->description);
    printf("--------------------------\n");
}

static void cmd_add(const Args *a)
{
    ensure_district(a->district);
    char rpath[512];
    build_path(rpath, sizeof(rpath), a->district, REPORTS_FILE);
    check_permission(rpath, S_IWUSR, a->role);
    int fd = open(rpath, O_RDWR | O_CREAT, PERM_REPORTS);
    if (fd < 0) {
        perror("Eroare la deschidere rapoarte");
        exit(EXIT_FAILURE);
    }
    int max_id = 0;
    Report tmp;
    while (read(fd, &tmp, sizeof(Report)) == sizeof(Report)) {
        if (tmp.id > max_id) {
            max_id = tmp.id;
        }
    }
    Report r;
    memset(&r, 0, sizeof(r));
    r.id = max_id + 1;
    r.timestamp = time(NULL);
    strncpy(r.inspector, a->user, NAME_LEN - 1);
    printf("Latitudine: ");
    scanf("%lf", &r.latitude);
    printf("Longitudine: ");
    scanf("%lf", &r.longitude);
    printf("Categorie: ");
    scanf("%31s", r.category);
    printf("Gravitate (1/2/3): ");
    scanf("%d", &r.severity);
    if (r.severity < 1 || r.severity > 3) r.severity = 1;
    int c;
    while ((c = getchar()) != '\n' && c != EOF) {
    }
    printf("Descriere: ");
    fgets(r.description, DESC_LEN, stdin);
    int dlen = strlen(r.description);
    if (dlen > 0) {
        if (r.description[dlen - 1] == '\n') {
            r.description[dlen - 1] = '\0';
        }
    }
    lseek(fd, 0, SEEK_END);
    if (write(fd, &r, sizeof(Report)) < 0) {
        perror("Eroare la salvare");
    }
    close(fd);
    set_permissions(rpath, PERM_REPORTS);
    printf("Raportul cu ID %d a fost adaugat.\n", r.id);
    char action[128];
    snprintf(action, sizeof(action), "add report_id=%d", r.id);
    log_action(a->district, a->role, a->user, action);
}

static void cmd_list(const Args *a)
{
    char rpath[512];
    build_path(rpath, sizeof(rpath), a->district, REPORTS_FILE);
    struct stat st;
    if (stat(rpath, &st) < 0) {
        fprintf(stderr, "Nu gasesc rapoarte pentru %s.\n", a->district);
        exit(EXIT_FAILURE);
    }
    check_permission(rpath, S_IRUSR, a->role);
    char perm_str[10];
    mode_to_str(st.st_mode, perm_str);
    char mtime_str[64];
    strftime(mtime_str, sizeof(mtime_str), "%Y-%m-%d %H:%M:%S", localtime(&st.st_mtime));
    printf("Fisier: %s\n", rpath);
    printf("Permisiuni: %s\n", perm_str);
    printf("Dimensiune: %lld bytes\n", (long long)st.st_size);
    printf("Modificat: %s\n", mtime_str);
    printf("==========================\n");
    int fd = open(rpath, O_RDONLY);
    if (fd < 0) {
        perror("Eroare citire");
        exit(EXIT_FAILURE);
    }
    Report r;
    int count = 0;
    while (read(fd, &r, sizeof(Report)) == sizeof(Report)) {
        print_report(&r);
        count++;
    }
    close(fd);
    if (count == 0) {
        printf("Nu exista rapoarte inregistrate.\n");
    }
    log_action(a->district, a->role, a->user, "list");
}

static void cmd_view(const Args *a)
{
    char rpath[512];
    build_path(rpath, sizeof(rpath), a->district, REPORTS_FILE);
    check_permission(rpath, S_IRUSR, a->role);
    int fd = open(rpath, O_RDONLY);
    if (fd < 0) {
        perror("Eroare citire");
        exit(EXIT_FAILURE);
    }
    Report r;
    int gasit = 0;
    while (read(fd, &r, sizeof(Report)) == sizeof(Report)) {
        if (r.id == a->report_id) {
            print_report(&r);
            gasit = 1;
            break;
        }
    }
    close(fd);
    if (gasit == 0) {
        printf("Nu am gasit raportul %d.\n", a->report_id);
    } else {
        char action[128];
        snprintf(action, sizeof(action), "view report_id=%d", a->report_id);
        log_action(a->district, a->role, a->user, action);
    }
}

static void cmd_remove_report(const Args *a)
{
    if (strcmp(a->role, ROLE_MANAGER) != 0) {
        fprintf(stderr, "Eroare: Doar un manager poate sterge.\n");
        exit(EXIT_FAILURE);
    }
    char rpath[512];
    build_path(rpath, sizeof(rpath), a->district, REPORTS_FILE);
    check_permission(rpath, S_IWUSR, a->role);
    int fd = open(rpath, O_RDWR);
    if (fd < 0) {
        perror("Eroare deschidere fisier");
        exit(EXIT_FAILURE);
    }
    off_t target_offset = -1;
    Report r;
    while (read(fd, &r, sizeof(Report)) == sizeof(Report)) {
        if (r.id == a->report_id) {
            target_offset = lseek(fd, 0, SEEK_CUR) - sizeof(Report);
            break;
        }
    }
    if (target_offset < 0) {
        printf("Raportul %d nu exista.\n", a->report_id);
        close(fd);
        exit(EXIT_FAILURE);
    }
    off_t citire_offset = target_offset + sizeof(Report);
    off_t scriere_offset = target_offset;
    while (1) {
        lseek(fd, citire_offset, SEEK_SET);
        ssize_t bytes = read(fd, &r, sizeof(Report));
        if (bytes != sizeof(Report)) {
            break;
        }
        lseek(fd, scriere_offset, SEEK_SET);
        write(fd, &r, sizeof(Report));
        citire_offset = citire_offset + sizeof(Report);
        scriere_offset = scriere_offset + sizeof(Report);
    }
    struct stat st;
    fstat(fd, &st);
    off_t dimensiune_noua = st.st_size - sizeof(Report);
    if (ftruncate(fd, dimensiune_noua) < 0) {
        perror("Eroare la ftruncate");
    }
    close(fd);
    printf("Raportul %d a fost sters cu succes.\n", a->report_id);
    char action[128];
    snprintf(action, sizeof(action), "remove_report report_id=%d", a->report_id);
    log_action(a->district, a->role, a->user, action);
}

static void cmd_update_threshold(const Args *a)
{
    if (strcmp(a->role, ROLE_MANAGER) != 0) {
        fprintf(stderr, "Eroare: Acces interzis.\n");
        exit(EXIT_FAILURE);
    }
    char cpath[512];
    build_path(cpath, sizeof(cpath), a->district, CONFIG_FILE);
    struct stat st;
    if (stat(cpath, &st) < 0) {
        perror("Eroare la citire config");
        exit(EXIT_FAILURE);
    }
    if ((st.st_mode & 0777) != PERM_CFG) {
        fprintf(stderr, "Eroare de securitate: permisiunile nu sunt 0640.\n");
        exit(EXIT_FAILURE);
    }
    check_permission(cpath, S_IWUSR, a->role);
    int fd = open(cpath, O_WRONLY | O_TRUNC);
    if (fd < 0) {
        perror("Eroare scriere config");
        exit(EXIT_FAILURE);
    }
    char buffer[64];
    int len = snprintf(buffer, sizeof(buffer), "severity_threshold=%d\n", a->threshold);
    write(fd, buffer, len);
    close(fd);
    printf("Prag modificat la %d.\n", a->threshold);
    char action[128];
    snprintf(action, sizeof(action), "update_threshold value=%d", a->threshold);
    log_action(a->district, a->role, a->user, action);
}

static int parse_condition(const char *input, char *field, char *op, char *value)
{
    if (input == NULL || field == NULL || op == NULL || value == NULL) {
        return 0;
    }
    const char *p1 = strchr(input, ':');
    if (p1 == NULL) {
        return 0;
    }
    int len_field = p1 - input;
    if (len_field >= FIELD_LEN || len_field == 0) {
        return 0;
    }
    strncpy(field, input, len_field);
    field[len_field] = '\0';
    const char *p2 = strchr(p1 + 1, ':');
    if (p2 == NULL) {
        return 0;
    }
    int len_op = p2 - (p1 + 1);
    if (len_op >= OP_LEN || len_op == 0) {
        return 0;
    }
    strncpy(op, p1 + 1, len_op);
    op[len_op] = '\0';
    if (strcmp(op, "==") != 0 && strcmp(op, "!=") != 0 &&
        strcmp(op, "<") != 0 && strcmp(op, "<=") != 0 &&
        strcmp(op, ">") != 0 && strcmp(op, ">=") != 0) {
        return 0;
    }
    const char *val_start = p2 + 1;
    if (val_start[0] == '\0') {
        return 0;
    }
    strncpy(value, val_start, VALUE_LEN - 1);
    value[VALUE_LEN - 1] = '\0';
    return 1;
}

static int match_condition(Report *r, const char *field, const char *op, const char *value)
{
    if (r == NULL || field == NULL || op == NULL || value == NULL) {
        return 0;
    }
    if (strcmp(field, "severity") == 0) {
        int v = r->severity;
        int t = (int)strtol(value, NULL, 10);
        if (strcmp(op, "==") == 0) return v == t;
        if (strcmp(op, "!=") == 0) return v != t;
        if (strcmp(op, "<") == 0) return v < t;
        if (strcmp(op, "<=") == 0) return v <= t;
        if (strcmp(op, ">") == 0) return v > t;
        if (strcmp(op, ">=") == 0) return v >= t;
    }
    if (strcmp(field, "timestamp") == 0) {
        time_t v = r->timestamp;
        time_t t = (time_t)strtol(value, NULL, 10);
        if (strcmp(op, "==") == 0) return v == t;
        if (strcmp(op, "!=") == 0) return v != t;
        if (strcmp(op, "<") == 0) return v < t;
        if (strcmp(op, "<=") == 0) return v <= t;
        if (strcmp(op, ">") == 0) return v > t;
        if (strcmp(op, ">=") == 0) return v >= t;
    }
    if (strcmp(field, "category") == 0) {
        int cmp = strcmp(r->category, value);
        if (strcmp(op, "==") == 0) return cmp == 0;
        if (strcmp(op, "!=") == 0) return cmp != 0;
        if (strcmp(op, "<") == 0) return cmp < 0;
        if (strcmp(op, "<=") == 0) return cmp <= 0;
        if (strcmp(op, ">") == 0) return cmp > 0;
        if (strcmp(op, ">=") == 0) return cmp >= 0;
    }
    if (strcmp(field, "inspector") == 0) {
        int cmp = strcmp(r->inspector, value);
        if (strcmp(op, "==") == 0) return cmp == 0;
        if (strcmp(op, "!=") == 0) return cmp != 0;
        if (strcmp(op, "<") == 0) return cmp < 0;
        if (strcmp(op, "<=") == 0) return cmp <= 0;
        if (strcmp(op, ">") == 0) return cmp > 0;
        if (strcmp(op, ">=") == 0) return cmp >= 0;
    }
    return 0;
}
static void cmd_remove_district(const Args *a){
   if (strcmp(a->role, ROLE_MANAGER) != 0) {
        fprintf(stderr, "Eroare: Acces interzis.\n");
        exit(EXIT_FAILURE);
    }
  int s=fork();
  char path[256];
  snprintf(path,sizeof(path),"active_reports-%s",a->district);
  if(s<0){
    perror(NULL);
    return;
  }
  else
    if(s==0)
      {
	execlp("rm","rm","-rf",a->district,NULL);
        perror(NULL);
	return;
      }
	else if(s!=0)
	  {
	    int status;
	    waitpid(s,&status,0);
	    unlink(path);
	}
}
static void cmd_filter(const Args *a)
{
    char rpath[512];
    build_path(rpath, sizeof(rpath), a->district, REPORTS_FILE);
    check_permission(rpath, S_IRUSR, a->role);
    if (a->num_conditions == 0) {
        fprintf(stderr, "Eroare: Trebuie specificata cel putin o conditie.\n");
        exit(EXIT_FAILURE);
    }
    int fd = open(rpath, O_RDONLY);
    if (fd < 0) {
        perror("Eroare la citire rapoarte");
        exit(EXIT_FAILURE);
    }
    char f_arr[8][FIELD_LEN];
    char o_arr[8][OP_LEN];
    char v_arr[8][VALUE_LEN];
    for (int i = 0; i < a->num_conditions; i++) {
        int ok = parse_condition(a->conditions[i], f_arr[i], o_arr[i], v_arr[i]);
        if (ok == 0) {
            fprintf(stderr, "Eroare la parsare filtru: %s\n", a->conditions[i]);
            close(fd);
            exit(EXIT_FAILURE);
        }
    }
    Report r;
    int gasite = 0;
    while (read(fd, &r, sizeof(Report)) == sizeof(Report)) {
        int valid = 1;
        for (int i = 0; i < a->num_conditions; i++) {
            int potrivire = match_condition(&r, f_arr[i], o_arr[i], v_arr[i]);
            if (potrivire == 0) {
                valid = 0;
                break;
            }
        }
        if (valid == 1) {
            print_report(&r);
            gasite++;
        }
    }
    close(fd);
    if (gasite == 0) {
        printf("Niciun raport nu respecta conditiile.\n");
    }
    log_action(a->district, a->role, a->user, "filter");
}

int main(int argc, char *argv[])
{
    Args argumente;
    parse_args(argc, argv, &argumente);
    if (strcmp(argumente.command, "add") == 0) {
        cmd_add(&argumente);
    } else if (strcmp(argumente.command, "list") == 0) {
        cmd_list(&argumente);
    } else if (strcmp(argumente.command, "view") == 0) {
        cmd_view(&argumente);
    } else if (strcmp(argumente.command, "remove_report") == 0) {
        cmd_remove_report(&argumente);
    } else if (strcmp(argumente.command, "update_threshold") == 0) {
        cmd_update_threshold(&argumente);
    } else if (strcmp(argumente.command, "filter") == 0) {
        cmd_filter(&argumente);
    } else if(strcmp(argumente.command, "remove_district")==0){
	cmd_remove_district(&argumente);
      }else
	 {
        fprintf(stderr, "Comanda este incorecta.\n");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
