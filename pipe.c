#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

#define MSGSIZE 6

int client(int *);
int server(int *);
void *CreateComponent(void *ptr);
int fatal(char *s); /* 오류 메시지를 프린트하고 죽는다. */

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
        client(pfd);
    default: /* 부모 */
        server(pfd);
    }
}

int client(int p[2]) /* 부모의 코드 */
{
    int nread;
    int component;

    close(p[1]);
    for (;;)
    {
        switch (nread = read(p[0], &component, sizeof(int)))
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
            printf("%d components received\n", component);
        }
    }
}

int server(int p[2])
{
    int number_of_components = 0;
    close(p[0]);
    pthread_t thread[100];
    int i = 0;

    int err_code;
    printf("In main: creating thread %d\n", i);
    for (int i = 0; i < 3; i++)
    {
        err_code = pthread_create(&thread[i], NULL, CreateComponent, (void *)&number_of_components);
        if (err_code)
        {
            printf("ERROR code is %d\n", err_code);
            exit(1);
        }
    }
    for (;;)
    {
        // number_of_components를 pipe로 write한다.
        if (number_of_components > 0)
        {
            printf("%dcomponents has send to client \n", number_of_components);
            if (write(p[1], &number_of_components, sizeof(int)) == -1)
                fatal("write call");
            number_of_components = 0;
        }
        sleep(2);
    }
}

int fatal(char *s) /* 오류 메시지를 프린트하고 죽는다. */
{
    perror(s);
    exit(1);
}

void *CreateComponent(void *ptr)
{
    int *number_of_components = (int *)ptr;
    int j;
    int i = 0;
    for (;;)
    {
        // 3초에 한 번 number_of_compenets의 값을 1 증가시킨다.
        sleep(1);
        *number_of_components++;
        printf("number_of_components = %d\n", *number_of_components);
    }
}
