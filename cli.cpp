#include "utili.h"
#include "be_controlled.h"

int main()
{
    be_con b_c(8888);

    create_daemon(b_c, exe_cli);
    //exe_cli(b_c);

    return 0;
}
