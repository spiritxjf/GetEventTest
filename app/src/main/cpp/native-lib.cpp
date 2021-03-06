#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/inotify.h>
#include <sys/limits.h>
#include <sys/poll.h>
#include <jni.h>
#include <string>
#include <linux/input.h>
#include <err.h>
#include <errno.h>
#include <unistd.h>
#include<android/log.h>

extern "C" {
int wd;

#define TAG "93367 - "
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR,TAG ,__VA_ARGS__) // 定义LOGE类型

static struct pollfd *ufds = NULL;
static char **device_names;
static int nfds;

static int open_device(const char *device, int print_flags)
{
    int version;
    int fd;
    int clkid = CLOCK_MONOTONIC;
    struct pollfd *new_ufds;
    char **new_device_names;
    char name[80];
    char location[80];
    char idstr[80];
    struct input_id id;

    fd = open(device, O_RDWR);
    if(fd < 0) {
        return -1;
    }

    if(ioctl(fd, EVIOCGVERSION, &version)) {
        return -1;
    }
    if(ioctl(fd, EVIOCGID, &id)) {
        return -1;
    }
    name[sizeof(name) - 1] = '\0';
    location[sizeof(location) - 1] = '\0';
    idstr[sizeof(idstr) - 1] = '\0';
    if(ioctl(fd, EVIOCGNAME(sizeof(name) - 1), &name) < 1) {
        name[0] = '\0';
    }
    if(ioctl(fd, EVIOCGPHYS(sizeof(location) - 1), &location) < 1) {
        location[0] = '\0';
    }
    if(ioctl(fd, EVIOCGUNIQ(sizeof(idstr) - 1), &idstr) < 1) {
        idstr[0] = '\0';
    }

    if (ioctl(fd, EVIOCSCLOCKID, &clkid) != 0) {
        // a non-fatal error
    }

    new_ufds = (pollfd *)realloc(ufds, sizeof(ufds[0]) * (nfds + 1));
    if(new_ufds == NULL) {
        fprintf(stderr, "out of memory\n");
        return -1;
    }
    ufds = new_ufds;
    new_device_names = (char **)realloc(device_names, sizeof(device_names[0]) * (nfds + 1));
    if(new_device_names == NULL) {
        fprintf(stderr, "out of memory\n");
        return -1;
    }
    device_names = new_device_names;

    ufds[nfds].fd = fd;
    ufds[nfds].events = POLLIN;
    device_names[nfds] = strdup(device);
    nfds++;

    return 0;
}

int close_device(const char *device, int print_flags)
{
    int i;
    for(i = 1; i < nfds; i++) {
        if(strcmp(device_names[i], device) == 0) {
            int count = nfds - i - 1;
            free(device_names[i]);
            memmove(device_names + i, device_names + i + 1, sizeof(device_names[0]) * count);
            memmove(ufds + i, ufds + i + 1, sizeof(ufds[0]) * count);
            nfds--;
            return 0;
        }
    }

    return -1;
}

static int read_notify(const char *dirname, int nfd, int print_flags)
{
    int res;
    char devname[PATH_MAX];
    char *filename;
    char event_buf[512];
    int event_size;
    int event_pos = 0;
    struct inotify_event *event;

    res = read(nfd, event_buf, sizeof(event_buf));
    if(res < (int)sizeof(*event)) {
        if(errno == EINTR)
            return 0;
        fprintf(stderr, "could not get event, %s\n", strerror(errno));
        return 1;
    }
    //printf("got %d bytes of event information\n", res);

    strcpy(devname, dirname);
    filename = devname + strlen(devname);
    *filename++ = '/';

    while(res >= (int)sizeof(*event)) {
        event = (struct inotify_event *)(event_buf + event_pos);
        //printf("%d: %08x \"%s\"\n", event->wd, event->mask, event->len ? event->name : "");
        if(event->len) {
            strcpy(filename, event->name);
            if(event->mask & IN_CREATE) {
                open_device(devname, print_flags);
            }
            else {
                close_device(devname, print_flags);
            }
        }
        event_size = sizeof(*event) + event->len;
        res -= event_size;
        event_pos += event_size;
    }
    return 0;
}

static int scan_dir(const char *dirname, int print_flags)
{
    char devname[PATH_MAX];
    char *filename;
    DIR *dir;
    struct dirent *de;
    dir = opendir(dirname);
    if(dir == NULL)
        return -1;
    strcpy(devname, dirname);
    filename = devname + strlen(devname);
    *filename++ = '/';
    while((de = readdir(dir))) {
        if(de->d_name[0] == '.' &&
           (de->d_name[1] == '\0' ||
            (de->d_name[1] == '.' && de->d_name[2] == '\0')))
            continue;
        strcpy(filename, de->d_name);
        open_device(devname, print_flags);
    }
    closedir(dir);
    return 0;
}

const char *device_path = "/dev/input";

JNIEXPORT jint JNICALL Java_com_tct_inputtest_MainActivity_openDevice(
        JNIEnv *env,
        jobject /* this */) {
    int res;
    struct input_event event;

    opterr = 0;

    nfds = 1;
    if (NULL != ufds)
    {
        free(ufds);
        ufds = NULL;
    }

    ufds = (pollfd *)calloc(1, sizeof(ufds[0]));
    ufds[0].fd = inotify_init();
    ufds[0].events = POLLIN;

    wd = inotify_add_watch(ufds[0].fd, device_path, IN_DELETE | IN_CREATE);
    if(wd < 0) {
        fprintf(stderr, "could not add watch for %s, %s\n", device_path, strerror(errno));
        return 1;
    }
    res = scan_dir(device_path, 0);
    if(res < 0) {
        fprintf(stderr, "scan dir failed for %s\n", device_path);
        if (wd>=0)inotify_rm_watch(ufds[0].fd, wd);
        return 1;
    }

    return 0;
}

JNIEXPORT jint JNICALL Java_com_tct_inputtest_MainActivity_closeDevice(
        JNIEnv *env,
        jobject /* this */) {
    //res = inotify_add_watch(ufds[0].fd, device_path, IN_DELETE | IN_CREATE);
    if (wd>=0) {
        inotify_rm_watch(ufds[0].fd, wd);
        wd = -1;
    }
    if (NULL != ufds)
    {
        free(ufds);
        ufds = NULL;
    }

    return 0;
}

JNIEXPORT jint JNICALL Java_com_tct_inputtest_MainActivity_readDevice(
        JNIEnv *env,
        jobject /* this */) {
    int i;
    int res;
    struct input_event event;

        //int pollres =
    poll(ufds, nfds, -1);
    //printf("poll %d, returned %d\n", nfds, pollres);
    if(ufds[0].revents & POLLIN) {
        read_notify(device_path, ufds[0].fd, 0);
    }
    for(i = 1; i < nfds; i++) {
        if(ufds[i].revents) {
            if(ufds[i].revents & POLLIN) {
                res = read(ufds[i].fd, &event, sizeof(event));
                if(res < (int)sizeof(event)) {
                    fprintf(stderr, "could not get event\n");
                    return 1;
                }

                //LOGE("Event = 0x%x", event.code << 24 | event.type << 16 | event.value);
                return (event.type & 0x0000000f) << 28  | (event.code & 0x00000fff) << 16 | (event.value & 0x0000ffff);
            }
        }
    }
}


}
