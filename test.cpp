#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <linux/videodev2.h>

int main()
{
    int fd = open("/dev/video0", O_RDWR);
    struct v4l2_capability cap;
    struct v4l2_format fmt;
    void *buf = NULL;
    int size = 0;

    ioctl(fd, VIDIOC_QUERYCAP, &cap);
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = 640;
    fmt.fmt.pix.height = 480;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_JPEG;
    ioctl(fd, VIDIOC_S_FMT, &fmt);
    buf = malloc(1024 * 1024);
    size = read(fd, buf, 1024*1024);
    int filefd = open("./capture.jpg", O_CREAT|O_RDWR);
    write(filefd, buf, size);

    return 0;
}
