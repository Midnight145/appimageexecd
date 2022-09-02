#include <stdio.h>
#include <sys/inotify.h>
#include <unistd.h> // for read, close
#include <stdlib.h>
#include <signal.h> // signal handling
#include <string.h> // strrchr, strcmp
#include <sys/stat.h> // chmod
#include <errno.h> // errno
#include <sysexits.h> // error codes
 
#define LEN_NAME 255 // maximum file name length
#define EVENT_SIZE  ( sizeof (struct inotify_event) )
#define BUF_LEN     ( EVENT_SIZE + LEN_NAME )
 
int fd, wd;
 
void sig_handler(int sig){
    inotify_rm_watch(fd, wd);
    close(fd);
    exit(EX_OK);
}

int main(int argc, char **argv) {

    if (argc != 2) {
        printf("usage: appimageexecd /path/to/watch/");
        exit(EX_USAGE);
    }

    signal(SIGINT, sig_handler);

    int path_length = sizeof(argv[1]) / sizeof(argv[1][0]);
    char *watch_path;

    watch_path = argv[1];

    // append / to watch_path as necessary
    if (watch_path[path_length - 2] != '/') {
        strcat(watch_path, "/");
    }

    fd = inotify_init();

    wd = inotify_add_watch(fd, watch_path, IN_CREATE);

    if (wd == -1) {
        fprintf(stderr, "Could not watch: %s\n", watch_path);
        exit(errno);
    }
    else {
        printf("Watching: %s\n", watch_path);
    }

    while (1) {
        char buffer[BUF_LEN];
        read(fd,buffer,BUF_LEN);

        // append filename to watch path to get "true" filename
        char filename[LEN_NAME + sizeof(watch_path) / sizeof(watch_path[0])];
        strcpy(filename, watch_path);
        strcat(filename, ((struct inotify_event *) &buffer[0])->name);

        // get event from buffer
        struct inotify_event *event = (struct inotify_event *) &buffer[0];

        if ( event->len ){
            if ( event->mask & IN_CREATE ) {
                if ( event->mask & IN_ISDIR ) { continue; }

                char* filetype = strrchr(event->name, '.');
                
                if (filetype == NULL) { continue; }

                // check filetype
                if (strcmp(filetype, ".AppImage") == 0 || strcmp(filetype, ".appimage") == 0) {
                    printf("Marking %s as executable\n", filename);

                    // check chmod success
                    if (chmod(filename, (mode_t) 0755) == -1) {
                        fprintf(stderr, "Error marking %s as executable\n", strcat(watch_path, filename));
                        fprintf(stderr, "Error: %s\n", strerror(errno));
                    }
                }
            }
        }

        // empty buffer
        memset(buffer, 0, BUF_LEN);
    }
}