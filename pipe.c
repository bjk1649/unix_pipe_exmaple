#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#define MSGSIZE 6

int parent(int *);
int child(int *);
int fatal(char *s); /* 오류 메시지를 프린트하고 죽는다. */

char *msg1 = "hello";
char *msg2 = "bye!!";

int main()
{
    int pfd[2];

    /* 파이프를 개방한다 */
    if (pipe(pfd) == -1)
        fatal("pipe call");

    /* p[0]의 O_NONBLOCK 플래그를 1로 설정한다 */
    if (fcntl(pfd[0], F_SETFL, O_NONBLOCK) == -1)
        fatal("fcntl call");

    switch (fork())
    {
    case -1: /* 오류 */
        fatal("fork call");
    case 0: /* 자식 */
        child(pfd);
    default: /* 부모 */
        parent(pfd);
    }
}

int parent(int p[2]) /* 부모의 코드 */
{
    int nread;
    char buf[MSGSIZE];

    close(p[1]);
    for (;;)
    {
        switch (nread = read(p[0], buf, MSGSIZE))
        {
        case -1:
            /* 파이프에 아무것도 없는지 검사한다. */
            if (errno == EAGAIN)
            {
                printf("(pipe empty)\n");
                sleep(1);
                break;
            }
            else
                fatal("read call");
        case 0:
            /* 파이프가 닫혔음. */
            printf("End of conversation\n");
            exit(0);
        default:
            printf("MSG=%s\n", buf);
        }
    }
}

int child(int p[2])
{
    int count;
    close(p[0]);
    for (count = 0; count < 3; count++)
    {
        write(p[1], msg1, MSGSIZE);
        sleep(3);
    }
    /* 마지막 메시지를 보낸다 */
    write(p[1], msg2, MSGSIZE);
    exit(0);
}
int fatal(char *s) /* 오류 메시지를 프린트하고 죽는다. */
{
    perror(s);
    exit(1);
}
