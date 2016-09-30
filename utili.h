#ifndef _UTILI_H
#define _UTILI_H


enum BE_CON_STATE
{
    LIVING,
    TAKE_P,
    EXE_C,
    EXE_F,
    UPLOAD_F,
    EXE_FAILED
};

struct sock_data
{
    int  state;
    int  len;
    char filename[255];
    char buf[0];
};

const int Kb = 1024;
const int Mb = Kb * 1024;
const long Gb = Mb * 1024;
const long g_max_content = Gb*3; // 最大映射的大小
const int g_max_send_sz = 1196;      // 最大传输的大小
const int g_supervisor_fd_sz = 1024; // epoll监督的描述符的个数
const int g_cliudp_port = 6666;
const int g_broad_port = 7777;

void epoll_add(int epoll_fd, int sockfd, int state);
void epoll_del(int epoll_fd, int sockfd, int state);
void upload_file(int sockfd, const char *path, int len, int state);
void send_file(int sockfd, const char *path, const char *start, int len, int state);
char *get_local_ip(int sockfd);

#endif
