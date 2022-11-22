#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>
#include <semaphore.h>

#define MSGSIZE 16

static sem_t component_sem;
static sem_t client1_component_sem;
static sem_t client2_component_sem;

int client(int *, int *, int);
int server(int[][], int *);
void *CreateComponent(void *ptr);
int fatal(char *s);
void *makeCar(void *ptr);
void *paintCar(void *ptr);
void *inspectCar(void *ptr);

int component_number = 0;
int client_component_number[2] = {0, 0};

typedef struct Car
{
    int thread_num;
    bool isCreated;
    bool isPainted;
    bool isInspected;
    struct Car *next;
} Car;

int main()
{
    if (sem_init(&component_sem, 0, 1) == -1)
    {
        perror("sem_init");
        exit(1);
    }
    int i = 0;
    int pipe1[2][2]; // 서버 -> 클라이언트1 쓰기용
    int pipe2[2];    // 클라이언트 -> 서버 쓰기용 (클라이언트 공용)

    if (pipe(pipe1[0]) == -1)
        fatal("pipe call");

    if (fcntl(pipe1[0][0], F_SETFL, O_NONBLOCK) == -1)
        fatal("fcntl call");

    if (pipe(pipe1[1]) == -1)
        fatal("pipe call");

    if (fcntl(pipe1[1][0], F_SETFL, O_NONBLOCK) == -1)
        fatal("fcntl call");

    if (pipe(pipe2) == -1)
        fatal("pipe call");

    if (fcntl(pipe2[0], F_SETFL, O_NONBLOCK) == -1)
        fatal("fcntl call");

    while (i < 2)
    {
        switch (fork())
        {
        case -1:
            fatal("fork call");
        case 0:
            client(pipe1[i], pipe2, i);
        default:
            i++;
            if (i == 2)
            {
                server(pipe1, pipe2);
            }
        }
    }
}

int client(int p1[2], int p2[2], int thread_num)
{
    if (thread_num == 0)
    {
        if (sem_init(&client1_component_sem, 0, 0) == -1)
        {
            perror("sem_init");
            exit(1);
        }
    }
    else
    {
        if (sem_init(&client2_component_sem, 0, 0) == -1)
        {
            perror("sem_init");
            exit(1);
        }
    }
    int nread;
    pthread_t thread[3];
    Car *headCar = (Car *)malloc(sizeof(Car));
    headCar->thread_num = thread_num;
    headCar->next = NULL;
    char inbuf[MSGSIZE];
    close(p1[1]); // 서버 -> 클라이언트 쓰기용 닫기
    close(p2[0]); /* 읽기를 닫는다 */

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
        if (client_component_number[thread_num] < 3)
        {
            write(p2[1], &thread_num, sizeof(int));
            sleep(1);
        }

        switch (nread = read(p1[0], inbuf, MSGSIZE))
        {
        case -1:
            if (errno == EAGAIN)
            {
                printf("(pipe empty)\n");
                sleep(1);
                break;
            }
            else
                fatal("read call");
        case 0:
            printf("End of conversation\n");
            exit(0);
        default:
            printf("%s received\n", inbuf);
            switch (thread_num)
            {
            case 0:
                sem_wait(&client1_component_sem);
                client_component_number[thread_num]++;
                sem_post(&client1_component_sem);
                break;
            case 1:
                sem_wait(&client2_component_sem);
                client_component_number[thread_num]++;
                sem_post(&client2_component_sem);
                break;
            }
        }
    }
}

int server(int p1[2][2], int p2[2])
{
    for (int i = 0; i < 2; i++)
    {
        close(p1[i][0]); /* 읽기를 닫는다 */
    }
    close(p2[1]); /* 쓰기를 닫는다 */
    pthread_t thread[100];
    int i = 0;
    int *client_thread_number = (int *)malloc(sizeof(int));
    int nread;
    char *cmp = "component";

    int err_code;

    for (int i = 0; i < 3; i++)
    {

        err_code = pthread_create(&thread[i], NULL, CreateComponent, NULL);
        if (err_code)
        {
            printf("ERROR code is %d\n", err_code);
            exit(1);
        }
    }
    printf("thread created\n");

    for (;;)
    {
        switch (nread = read(p2[0], client_thread_number, sizeof(int)))
        {
        case -1:
            if (errno == EAGAIN)
            {
                printf("(pipe empty)\n");
                sleep(1);
                break;
            }
            else
                fatal("read call");
        case 0:
            printf("End of conversation\n");
            exit(0);
        default:
            printf("request from : %d th thread\n", *client_thread_number);
            if (component_number > 0)
            {
                sem_wait(&component_sem);
                component_number--;
                sem_post(&component_sem);
                write(p1[*client_thread_number][1], cmp, MSGSIZE);
            }
        }
    }
}

int fatal(char *s) /* 오류 메시지를 프린트하고 죽는다. */
{
    perror(s);
    exit(1);
}

void *CreateComponent(void *ptr)
{
    for (;;)
    {
        if (component_number < 100)
        {
            sem_wait(&component_sem);
            component_number++;
            sem_post(&component_sem);
        }
        sleep(1);
    }
}

void *makeCar(void *ptr)
{
    int i = 0;
    Car *current_made_car = ptr;
    int thread_num = current_made_car->thread_num;
    for (;;)
    {
        if (client_component_number[thread_num] > 0)
        {
            switch (thread_num)
            {
            case 0:
                sem_wait(&client1_component_sem);
                client_component_number[thread_num]--;
                sem_post(&client1_component_sem);
                break;
            case 1:
                sem_wait(&client2_component_sem);
                client_component_number[thread_num]--;
                sem_post(&client2_component_sem);
                break;
            }
            current_made_car->next = (Car *)malloc(sizeof(Car));
            current_made_car = current_made_car->next;
            current_made_car->isCreated = true;
            current_made_car->isPainted = false;
            current_made_car->isInspected = false;
            current_made_car->next = NULL;
            printf("car %d is created, client have %d components\n", i, client_component_number[thread_num]);
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
        if (current_inspect_target->next != NULL && current_inspect_target->next->isPainted)
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
