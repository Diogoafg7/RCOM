#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>

#define BAUDRATE B38400
#define _POSIX_SOURCE 1

#define FALSE 0
#define TRUE 1

#define BUF_SIZE 256
#define TIMETORUN 3

volatile int STOP = FALSE;

int alarmEnabled = FALSE;
int alarmCount = 0;

void alarmHandler(int signal)
{
    alarmEnabled = FALSE;
    alarmCount++;

    printf("Alarm #%d\n", alarmCount);

    if (alarmCount <= TIMETORUN)
    {
        // Implemente aqui o código para retransmitir o frame SET
        printf("Retransmitindo o frame SET...\n");
    }
    else
    {
        printf("Número máximo de retransmissões alcançado. Encerrando o programa.\n");
        exit(1); // Encerre o programa após atingir o número máximo de retransmissões
    }
}

int main(int argc, char *argv[])
{
    const char *serialPortName = argv[1];

    if (argc < 2)
    {
        printf("Uso incorreto do programa\n"
               "Uso: %s <SerialPort>\n"
               "Exemplo: %s /dev/ttyS1\n",
               argv[0],
               argv[0]);
        exit(1);
    }

    int fd = open(serialPortName, O_RDWR | O_NOCTTY);

    if (fd < 0)
    {
        perror(serialPortName);
        exit(-1);
    }

    struct termios oldtio;
    struct termios newtio;

    if (tcgetattr(fd, &oldtio) == -1)
    {
        perror("tcgetattr");
        exit(-1);
    }

    memset(&newtio, 0, sizeof(newtio));

    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    newtio.c_lflag = 0;
    newtio.c_cc[VTIME] = 0;
    newtio.c_cc[VMIN] = 5;

    tcflush(fd, TCIOFLUSH);

    if (tcsetattr(fd, TCSANOW, &newtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    printf("Nova estrutura de termios configurada\n");

    unsigned char set[5] = {0x7E, 0x03, 0x03, 0x00, 0x7E};
    unsigned char ua[5];

    set[3] = set[1] ^ set[2];

    (void)signal(SIGALRM, alarmHandler);
    
    int bytes_wr;

    while (alarmCount < TIMETORUN)
    {
        int bytes_wr = write(fd, set, sizeof(set));
        int bytes = read(fd, ua, sizeof(ua));

        // Verifique se o quadro UA foi recebido corretamente
        if (bytes == sizeof(ua) &&
            ua[0] == 0x7E &&
            ua[1] == 0x01 &&
            ua[2] == 0x07 &&
            ua[3] == (ua[1] ^ ua[2]) &&
            ua[4] == 0x7E)
        {
            printf("Quadro UA recebido corretamente. Desativando o alarme.\n");
            alarm(0); // Desativar o alarme
            break;     
        }
        else
        {
            // Caso contrário, aguarde até a próxima tentativa
            if (alarmEnabled == FALSE)
            {
                alarm(3); // Defina o alarme para ser acionado em 3s
                alarmEnabled = TRUE;
            }
        }
    }

    printf("%d bytes escritos\n", bytes_wr);

    sleep(1);

    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd);

    printf("Programa encerrado\n");

    return 0;
}

