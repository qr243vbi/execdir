#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <lmdb.h>


#define USAGE "Usage: execdir [-h] [-v] [-s] [-a] [-a] [-p] [-n NAME PATH] [-r NAME] [-g NAME] [-l] " \
"[ARGS...]"

#define VERSION "0.4.0"

#define EXECDIR_FILE ".execdir.db"

#define print_error(...) fprintf(stderr, "execdir: " __VA_ARGS__);


// strdup wrapper
char *xstrdup(const char *s) {
    char *str;

    str = strdup(s);
    if(!str) {
        print_error("cannot allocate memory: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    return str;
}

int create_directory(const char *path) {
    char *temp;
    char *p = NULL;
    // Copy the path to a temporary string
    temp = xstrdup(path);
    // Iterate through the path and create directories
    for (p = temp; *p; p++) {
        if (*p == '/') {
            if (p != temp){
                *p = '\0'; // Temporarily terminate the string
                if (mkdir(temp, 0755) == -1) {
                    if (errno != EEXIST) {
                        perror("mkdir failed");
                        return -1;
                    }
                }
                *p = '/'; // Restore the string
            }
        }
    }

    // Create the final directory
    if (mkdir(temp, 0755) == -1) {
        if (errno != EEXIST) {
            perror("mkdir failed");
            return -1;
        }
    }
    return 0; // Success
}


// get the current user's home directory
char *get_home_dir() {
    char *home_dir;

    home_dir = getenv("HOME");

    // fall back to the passwd file
    if(!home_dir) {
        uid_t uid;
        struct passwd *pw;

        uid = getuid();
        pw = getpwuid(uid);

        if(!pw)
            return NULL;

        home_dir = pw->pw_dir;
    }

    return home_dir;
}

// return execdir file path
char *get_execdir_file_path() {
    char *path;
    char *home_dir;

    home_dir = get_home_dir();
    if(!home_dir) {
        print_error("cannot get the home directory\n");
        exit(EXIT_FAILURE);
    }

    path = malloc(strlen(home_dir) + strlen("/" EXECDIR_FILE) + 1);
    if(!path) {
        print_error("cannot allocate memory: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    sprintf(path, "%s/" EXECDIR_FILE, home_dir);

    return path;
}

// getcwd wrapper
char *xgetcwd() {
    char *cwd;
    long size;
    char *buf;

    size = pathconf(".", _PC_PATH_MAX);
    if(size == -1)
        return NULL;

    buf = malloc(size);
    if(!buf)
        return NULL;

    cwd = getcwd(buf, size);

    return cwd;
}

// join all the arguments by adding a space between them
char *argv_to_str(int argc, char **argv) {
    char *str;
    size_t str_len = 0;

    for(int i = 0; i < argc; i++) {
        str_len += strlen(argv[i]);

        if(i + 1 != argc)
            str_len++;
    }

    str = calloc(1, str_len + 1);
    if(!str)
        return NULL;

    for(int i = 0; i < argc; i++) {
        strcat(str, argv[i]);

        // don't add a space at the end
        if(i + 1 != argc)
            strcat(str, " ");
    }

    return str;
}

int sh_exec_cmd(int argc, char **argv) {
    int status;
    char *cmd;

    cmd = argv_to_str(argc, argv);
    if(!cmd) {
        print_error("cannot allocate memory for command string\n");
        exit(EXIT_FAILURE);
    }

    status = system(cmd);
    if(status == -1) {
        print_error("failed to execute command: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    free(cmd);

    return WIFEXITED(status) ? WEXITSTATUS(status) : EXIT_FAILURE;
}

int exec_cmd(char **argv) {
    int status;

    status = execvp(*argv, argv);
    if(status == -1) {
        print_error("failed to execute command: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    return status;
}

void usage_message() {
    fprintf(stderr, USAGE "\n");
    exit(EXIT_FAILURE);
}

void help_message() {
    printf(USAGE "\n\n"
           "Options:\n"
           "  -h            display this help and exit\n"
           "  -v            output version information and exit\n"
           "  -s            execute the command as a shell command\n"
           "  -n NAME PATH  add an alias for a path\n"
           "  -r NAME       remove an alias\n"
           "  -a            use aliases (-aa for using only aliases)\n"
           "  -g NAME       get alias variable\n"
           "  -p            create directory if absent\n"
           "  -l            list all aliases\n\n"
           "Report bugs to <https://github.com/qr243vbi/execdir/issues>\n");
    exit(EXIT_SUCCESS);
}

typedef struct {
    MDB_env *env;
    MDB_txn *txn;
    MDB_dbi dbi;
} LMDB_Database;

void handle_error(int rc) {
    if (rc != 0) {
        fprintf(stderr, "Error: %s\n", mdb_strerror(rc));
        exit(EXIT_FAILURE);
    }
    return;
}

LMDB_Database* open_or_create_lmdb_database(const char *path, int flags) {
    create_directory(path);
    LMDB_Database *db = malloc(sizeof(LMDB_Database));
    if (db == NULL) {
        fprintf(stderr, "Memory allocation failure\n");
        exit(EXIT_FAILURE);
    }

    // Create the LMDB environment
    int rc = mdb_env_create(&db->env);
    handle_error(rc);

    // Set the map size (10 MB in this example)
    rc = mdb_env_set_mapsize(db->env, 10485760);
    handle_error(rc);

    // Open the environment
    rc = mdb_env_open(db->env, path, 0, 0664);
    handle_error(rc);

    // Start a read-write transaction
    rc = mdb_txn_begin(db->env, NULL, flags, &db->txn);
    handle_error(rc);

    // Try to open the database
    rc = mdb_open(db->txn, NULL, 0, &db->dbi);

    // If the database does not exist, create it
    if (rc == MDB_NOTFOUND) {
        rc = mdb_open(db->txn, NULL, MDB_CREATE, &db->dbi);
        handle_error(rc);
    } else {
        handle_error(rc); // Handle other possible errors
    }

    return db;
}

void close_lmdb_database(LMDB_Database *db) {
    if (db->txn) {
        mdb_txn_commit(db->txn);
    }
    mdb_close(db->env, db->dbi);
    mdb_env_close(db->env);
    free(db);
    return;
}

char* get_string_value(LMDB_Database *db, const char *key_str) {
    MDB_val key, data;
    key.mv_size = strlen(key_str) + 1; // Include null terminator
    key.mv_data = (void*)key_str;

    // Retrieve the value
    int rc = mdb_get(db->txn, db->dbi, &key, &data);

    if (rc == 0) {
        // Value found, allocate space for the string and copy it
        char *value = malloc(data.mv_size);
        if (value == NULL) {
            fprintf(stderr, "Memory allocation failure\n");
            exit(EXIT_FAILURE);
        }
        memcpy(value, data.mv_data, data.mv_size);
        value[data.mv_size - 1] = '\0'; // Ensure null-termination
        return value; // Return the value
    } else if (rc == MDB_NOTFOUND) {
        return NULL; // Key not found
    } else {
        mdb_txn_commit(db->txn); // Commit the transaction
        handle_error(rc); // Handle other possible errors
    }

    return NULL; // Should never reach here
}

void put_string_value(LMDB_Database *db, const char *key_str, const char *value_str) {
    MDB_val key, data;

    // Prepare key and value
    key.mv_size = strlen(key_str) + 1; // Include null terminator
    key.mv_data = (void*)key_str;

    data.mv_size = strlen(value_str) + 1; // Include null terminator
    data.mv_data = (void*)value_str;

    // Store the key-value pair
    int rc = mdb_put(db->txn, db->dbi, &key, &data, 0);

    handle_error(rc);
    return;
}


void drop_string_value(LMDB_Database *db, const char *key_str) {
    MDB_val key;

    // Prepare key and value
    key.mv_size = strlen(key_str) + 1; // Include null terminator
    key.mv_data = (void*)key_str;

    // Store the key-value pair
    int rc = mdb_del(db->txn, db->dbi, &key, NULL);

    handle_error(rc);
    return;
}

char * get_path_by_name(const char * execdir, const char * name){
    LMDB_Database * db = open_or_create_lmdb_database(execdir, MDB_RDONLY);
    char * value = get_string_value(db, name);
    close_lmdb_database(db);
    return value;
}

void drop_path_by_name(const char * execdir, const char * name){
    LMDB_Database * db = open_or_create_lmdb_database(execdir, 0);
    drop_string_value(db, name);
    close_lmdb_database(db);
    return;
}

void add_alias_to_db(const char * execdir, const char * name, const char * value){
    LMDB_Database * db = open_or_create_lmdb_database(execdir, 0);
    put_string_value(db, name, value);
    close_lmdb_database(db);
    return;
}

void list_keys_and_values_db(LMDB_Database *db) {
    MDB_cursor *cursor;
    MDB_val key, value;

    // Open a cursor
    int rc = mdb_cursor_open(db->txn, db->dbi, &cursor);
    handle_error(rc);

    // Iterate through the key-value pairs
    while (mdb_cursor_get(cursor, &key, &value, MDB_NEXT) == 0) {
        printf("%.*s:%.*s\n",
               (int)key.mv_size - 1, (char *)key.mv_data,
               (int)value.mv_size - 1, (char *)value.mv_data);
    }

    // Close the cursor
    mdb_cursor_close(cursor);
    return;
}

void list_keys_and_values(const char * execdir){
    LMDB_Database * db = open_or_create_lmdb_database(execdir, MDB_RDONLY);
    list_keys_and_values_db(db);
    close_lmdb_database(db);
    return;
}

int main(int argc, char **argv) {
    int opt;
    char *execdir_file_path = 0;
    char *cwd;
    char *path;
    struct stat st;
    int help_opt = 0;
    int version_opt = 0;
    int get_alias_opt = 0;
    int sh_exec_opt = 0;
    int add_alias_opt = 0;
    int rm_alias_opt = 0;
    int ls_alias_opt = 0;
    int use_alias_opt = 0;
    int mkdir_opt = 0;

    while((opt = getopt(argc, argv, "hvsarlpng")) != -1) {
        switch(opt) {
            case 'g':
                get_alias_opt = 1;
                break;
            case 'h':
                help_opt = 1;
                break;
            case 'v':
                version_opt = 1;
                break;
            case 's':
                sh_exec_opt = 1;
                break;
            case 'a':
                use_alias_opt += 1;
                break;
            case 'p':
                mkdir_opt = 1;
                break;
            case 'n':
                add_alias_opt = 1;
                break;
            case 'r':
                rm_alias_opt = 1;
                break;
            case 'l':
                ls_alias_opt = 1;
                break;
            case '?':
                exit(EXIT_FAILURE);
        }
    }

    // skip to the non-option arguments
    argc -= optind;
    argv += optind;

    if(help_opt) {
        help_message();
    } else if(version_opt) {
        printf("execdir version %s\n", VERSION);
        exit(EXIT_SUCCESS);
    }

    // load aliases
    execdir_file_path = get_execdir_file_path();

    if(add_alias_opt) {
        if(argc < 2) {
            print_error("-a requires two arguments\n");
            exit(EXIT_FAILURE);
        }
        add_alias_to_db(execdir_file_path, argv[0], argv[1]);
        if ((mkdir_opt == 1) && create_directory(argv[1])) {};
        goto do_not_skip_success;
    } else if(rm_alias_opt) {
        if(argc < 1) {
            print_error("-r requires one argument\n");
            exit(EXIT_FAILURE);
        }
        drop_path_by_name(execdir_file_path, argv[0]);
        goto do_not_skip_success;
    } else if(get_alias_opt) {
        if (argc < 1){
            print_error("-r requires one argument\n");
            exit(EXIT_FAILURE);
        }
        char* value = get_path_by_name(execdir_file_path, argv[0]);
        if (value == NULL){
            value = "(null)";
        }
        printf("%s\n", value);
        goto do_not_skip_success;
    } else if (ls_alias_opt) {
        list_keys_and_values(execdir_file_path);
        goto do_not_skip_success;
    }

    goto skip_success;
    do_not_skip_success:
    {
        free(execdir_file_path);
        exit(EXIT_SUCCESS);
    }
    skip_success:

    if(argc < 2) {
        usage_message();
    }

    // save and skip the path argument
    path = *argv;
    argc -= 1;
    argv += 1;

    if (use_alias_opt > 1){
        goto alias_stat_exec_2;
    }
    // try to get an alias if path doesn't exist
    if(stat(path, &st) == -1 || !S_ISDIR(st.st_mode)) {
        if (use_alias_opt == 0) {
            goto alias_stat_exec_0;
        }
        alias_stat_exec_2:
        char *name = path;

        path = get_path_by_name(execdir_file_path, name);
        if(!path) {
            if (use_alias_opt == 1) {
                print_error("path or ");
            }
            print_error("alias for path \"%s\" not found\n", name);
            exit(EXIT_FAILURE);
        } else {
            if(stat(path, &st) == -1 || !S_ISDIR(st.st_mode)) {
                alias_stat_exec_0:
                if ((mkdir_opt == 1) && create_directory(path)) {
                    print_error("path \"%s\" not found\n", path);
                    exit(EXIT_FAILURE);
                }
            }
        }
    }

    cwd = xgetcwd();
    if(!cwd) {
        print_error("cannot get the current working directory\n");
        exit(EXIT_FAILURE);
    }

    if(chdir(path) == -1) {
        print_error("cannot change \"%s\" directory: %s\n", path, strerror(errno));
        exit(EXIT_FAILURE);
    }

    // set old and new path to reflect the directory change
    setenv("OLDPWD", cwd, 1);
    setenv("PWD", path, 1);

    free(cwd);
    free(execdir_file_path);

    exit(sh_exec_opt ? sh_exec_cmd(argc, argv) : exec_cmd(argv));
}
