#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>

#define MSGSIZE 6

int client(int *);
int server(int *);
void *CreateComponent(void *ptr);
int fatal(char *s); /* 오류 메시지를 프린트하고 죽는다. */
void *makeCar(void *ptr);
void *paintCar(void *ptr);
void *inspectCar(void *ptr);

typedef struct Component
{
    int component;
} Component;

typedef struct Car
{
    bool isCreated;
    bool isPainted;
    bool isInspected;
    struct Car *next;
} Car;

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

int client(int p[2])
{
    int nread;
    Component *componentp = (Component *)malloc(sizeof(Component));
    pthread_t thread[3];
    Car *headCar = (Car *)malloc(sizeof(Car));
    Car *currentCar = headCar;
    headCar->next = NULL;

    close(p[1]); /* 쓰기를 닫는다 */

    int err_code = pthread_create(&thread[0], NULL, makeCar, (void *)headCar); // 차를 만든다.
    if (err_code)
    {
        printf("ERROR code is %d\n", err_code);
        exit(1);
    }
    err_code = 0;
    err_code = pthread_create(&thread[1], NULL, paintCar, (void *)headCar); // 차를 도색한다.
    if (err_code)
    {
        printf("ERROR code is %d\n", err_code);
        exit(1);
    }
    err_code = 0;
    err_code = pthread_create(&thread[2], NULL, inspectCar, (void *)headCar); // 차를 검사한 후 출고한다.
    if (err_code)
    {
        printf("ERROR code is %d\n", err_code);
        exit(1);
    }

    for (;;)
    {
        switch (nread = read(p[0], componentp, sizeof(Component)))
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
            printf("%d components received\n", (componentp->component));
            currentCar->next = (Car *)malloc(sizeof(Car));
            currentCar = currentCar->next;
            currentCar->isCreated = false;
            currentCar->isPainted = false;
            currentCar->isInspected = false;
            currentCar->next = NULL;
        }
    }
}

int server(int p[2])
{
    Component *componentp = (Component *)malloc(sizeof(Component));
    componentp->component = 0;

    close(p[0]);
    pthread_t thread[100];
    int i = 0;

    int err_code;
    printf("In main: creating thread %d\n", i);
    {
        err_code = pthread_create(&thread[i], NULL, CreateComponent, (void *)componentp);
        if (err_code)
        {
            printf("ERROR code is %d\n", err_code);
            exit(1);
        }
    }
    printf("thread created\n");
    for (;;)
    {
        if (componentp->component > 0)
        {
            printf("%d components has send to client \n", componentp->component);
            if (write(p[1], componentp, sizeof(Component)) == -1)
                fatal("write call");
            else
            {
                componentp->component--;
            }
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
    Component *componentp = ptr;
    for (;;)
    {
        // 3초에 한 번 number_of_compenets의 값을 1 증가시킨다.
        componentp->component++;
        printf("number of component = %d\n", componentp->component);
        sleep(1);
    }
}

void *makeCar(void *ptr)
{
    int i = 0;
    Car *current_made_car = ptr;
    for (;;)
    {
        if (current_made_car->next != NULL)
        {
            current_made_car = current_made_car->next;
            current_made_car->isCreated = true;
            printf("car %d is created\n", i);
            i++;
            sleep(3);
        }
    }
}

void *paintCar(void *ptr)
{
    int i = 0;
    Car *current_painted_car = ptr;
    for (;;)
    {
        if ((current_painted_car->next != NULL) && current_painted_car->next->isCreated)
        {
            current_painted_car = current_painted_car->next;
            current_painted_car->isPainted = true;
            printf("car %d is painted\n", i);
            i++;
            sleep(1);
        }
    }
}

void *inspectCar(void *ptr)
{
    int i = 0;
    Car *current_inspect_target = ptr;
    for (;;)
    {
        if (current_inspect_target->next != NULL && current_inspect_target->next->isCreated && current_inspect_target->next->isPainted)
        {
            Car *inpected_car = current_inspect_target;
            current_inspect_target = current_inspect_target->next;
            current_inspect_target->isInspected = true;
            free(inpected_car);
            printf("car %d is inspected\n", i);
            i++;
            sleep(2);
        }
    }
}
