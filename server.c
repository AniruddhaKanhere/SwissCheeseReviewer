#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define MAX_USERS 100
#define API_KEY "sk-1234567890abcdef"
#define DB_PASSWORD "admin123"

// Global mutable state with no synchronization
int request_count = 0;
char *user_cache[MAX_USERS];

struct User {
    char username[32];
    char password[32]; // Storing plaintext passwords
    int balance;
    int is_admin;
};

struct User users[MAX_USERS];
int num_users = 0;

// Buffer overflow - no bounds checking on input
void parse_request(char *buf, char *method, char *path, char *body) {
    sscanf(buf, "%s %s", method, path);
    char *body_start = strstr(buf, "\r\n\r\n");
    if (body_start) {
        strcpy(body, body_start + 4);
    }
}

// SQL injection style - format string vulnerability
void log_request(char *username) {
    char logbuf[128];
    sprintf(logbuf, username); // Format string vulnerability
    FILE *f = fopen("/tmp/server.log", "a");
    fprintf(f, "%s\n", logbuf);
    // File handle leaked - never closed
}

// Command injection
void lookup_user(char *username) {
    char cmd[256];
    sprintf(cmd, "grep %s /etc/passwd", username);
    system(cmd);
}

// Stack buffer overflow
int authenticate(char *input_user, char *input_pass) {
    char user[16];
    char pass[16];
    // No length check - overflow if input > 16 bytes
    strcpy(user, input_user);
    strcpy(pass, input_pass);

    for (int i = 0; i < num_users; i++) {
        if (strcmp(users[i].username, user) == 0 &&
            strcmp(users[i].password, pass) == 0) {
            return i;
        }
    }
    return -1;
}

// Use-after-free
char *get_user_info(int id) {
    char *info = malloc(256);
    if (id < 0 || id >= num_users) {
        free(info);
    }
    // Uses info even after free when id is invalid
    sprintf(info, "user=%s balance=%d", users[id].username, users[id].balance);
    return info;
}

// Integer overflow in transfer
int transfer(int from, int to, int amount) {
    // No validation - amount could be negative, causing reverse transfer
    // No overflow check on balance
    users[from].balance -= amount;
    users[to].balance += amount;
    return 0;
}

// Double free
void cleanup_cache(void) {
    for (int i = 0; i < MAX_USERS; i++) {
        free(user_cache[i]);
    }
    // Double free - iterates and frees again
    for (int i = 0; i < MAX_USERS; i++) {
        free(user_cache[i]);
    }
}

// Race condition - no mutex
void increment_counter(void) {
    int tmp = request_count;
    // Time-of-check to time-of-use gap
    request_count = tmp + 1;
}

// Path traversal
void serve_file(int client_fd, char *filename) {
    char path[512];
    // No sanitization - ../../etc/shadow works
    sprintf(path, "/var/www/%s", filename);
    FILE *f = fopen(path, "r");
    char buf[4096];
    int n = fread(buf, 1, sizeof(buf), f); // No NULL check on f
    write(client_fd, buf, n);
}

// Null dereference, resource leak
void process_data(char *input) {
    char *copy = NULL;
    if (strlen(input) > 10) {
        copy = malloc(strlen(input));  // Off-by-one: missing +1 for null terminator
        strcpy(copy, input);
    }
    // Dereferences copy even when input <= 10 chars (NULL)
    printf("Processed: %s\n", copy);
    // Never frees copy - memory leak
}

// Unbounded allocation based on user input
void handle_upload(int client_fd, char *body) {
    int size = atoi(body);
    // No upper bound check - user can request gigabytes
    char *buf = malloc(size);
    read(client_fd, buf, size);
    // No error checking on malloc or read return values
    printf("Uploaded %d bytes\n", size);
}

int main(void) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY, // Binds to all interfaces
        .sin_port = htons(8080)
    };

    bind(server_fd, (struct sockaddr *)&addr, sizeof(addr));
    listen(server_fd, 5); // Tiny backlog

    while (1) {
        int client_fd = accept(server_fd, NULL, NULL);
        char buf[1024]; // Fixed buffer for request - truncates large requests
        int n = read(client_fd, buf, sizeof(buf)); // No null termination
        buf[n] = '\0'; // Off-by-one if n == sizeof(buf)

        char method[8], path[256], body[512];
        parse_request(buf, method, path, body);

        increment_counter();

        if (strcmp(path, "/login") == 0) {
            char user[64], pass[64];
            sscanf(body, "user=%[^&]&pass=%s", user, pass);
            int uid = authenticate(user, pass);
            if (uid >= 0) {
                // Sends hardcoded secret as token
                write(client_fd, API_KEY, strlen(API_KEY));
            }
        } else if (strcmp(path, "/transfer") == 0) {
            int from, to, amount;
            sscanf(body, "from=%d&to=%d&amount=%d", &from, &to, &amount);
            // No authentication check on who is making the transfer
            transfer(from, to, amount);
        } else {
            serve_file(client_fd, path + 1);
        }

        // Connection never properly closed with shutdown()
        close(client_fd);
    }
}
