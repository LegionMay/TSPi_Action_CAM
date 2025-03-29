#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#define FIFO_PATH "/tmp/gnss_control_fifo"

void print_usage() {
    printf("Usage: gnss_control [command]\n");
    printf("Commands:\n");
    printf("  start   - Start GNSS data collection\n");
    printf("  stop    - Stop GNSS data collection\n");
    printf("  status  - Check if GNSS collection is running\n");
}

int main(int argc, char *argv[]) {
    int fifo_fd;
    
    if (argc != 2) {
        print_usage();
        return 1;
    }
    
    // 验证命令
    if (strcmp(argv[1], "start") != 0 && 
        strcmp(argv[1], "stop") != 0 && 
        strcmp(argv[1], "status") != 0) {
        printf("Invalid command: %s\n", argv[1]);
        print_usage();
        return 1;
    }
    
    // 打开FIFO
    fifo_fd = open(FIFO_PATH, O_WRONLY);
    if (fifo_fd < 0) {
        perror("Failed to open FIFO");
        printf("Make sure GNSS collector is running\n");
        return 1;
    }
    
    // 发送命令
    write(fifo_fd, argv[1], strlen(argv[1]));
    close(fifo_fd);
    
    printf("Command '%s' sent to GNSS collector\n", argv[1]);
    return 0;
}