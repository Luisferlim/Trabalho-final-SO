#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/uinput.h>
#include <termios.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <glob.h>

#define BAUDRATE B115200

#define AXIS_MIN -32767
#define AXIS_MAX 32767

// Função auxiliar para emitir eventos
int emit(int fd, int type, int code, int value) {
    struct input_event ev;
    memset(&ev, 0, sizeof(struct input_event));
    ev.type = type;
    ev.code = code;
    ev.value = value;
    return write(fd, &ev, sizeof(struct input_event));
}

// Função para detectar automaticamente o primeiro /dev/ttyACM* disponível
// Retorna string estática, ou NULL se não encontrar
// Função corrigida para retornar uma string válida!
char detected_device[256];
const char* detect_serial_acm_device() {
    glob_t glob_result;
    int ret = glob("/dev/ttyACM*", 0, NULL, &glob_result);
    if (ret == 0 && glob_result.gl_pathc > 0) {
        strncpy(detected_device, glob_result.gl_pathv[0], sizeof(detected_device)-1);
        detected_device[sizeof(detected_device)-1] = 0;
        globfree(&glob_result);
        return detected_device;
    }
    globfree(&glob_result);
    return NULL;
}
int main() {
    const char* serial_device = detect_serial_acm_device();
    if (!serial_device) {
        fprintf(stderr, "Erro: nenhum dispositivo /dev/ttyACM* encontrado.\n");
        return 1;
    }

    printf("Detectado dispositivo serial: %s\n", serial_device);

    int uinput_fd, serial_fd;
    struct termios tty;

    // Abrir porta serial detectada
    serial_fd = open(serial_device, O_RDWR | O_NOCTTY);
    if (serial_fd < 0) {
        perror("Erro ao abrir porta serial");
        return 1;
    }

    tcgetattr(serial_fd, &tty);
    cfsetospeed(&tty, BAUDRATE);
    cfsetispeed(&tty, BAUDRATE);
    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
    tty.c_iflag = 0;
    tty.c_oflag = 0;
    tty.c_lflag = 0;
    tty.c_cc[VMIN] = 1;
    tty.c_cc[VTIME] = 1;
    tcsetattr(serial_fd, TCSANOW, &tty);

    // Abrir /dev/uinput
    uinput_fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (uinput_fd < 0) {
        perror("Erro ao abrir /dev/uinput");
        close(serial_fd);
        return 1;
    }

    // Configurar joystick virtual
    ioctl(uinput_fd, UI_SET_EVBIT, EV_KEY);
    ioctl(uinput_fd, UI_SET_KEYBIT, BTN_TRIGGER);
    ioctl(uinput_fd, UI_SET_KEYBIT, BTN_THUMB);
    ioctl(uinput_fd, UI_SET_EVBIT, EV_ABS);
    ioctl(uinput_fd, UI_SET_ABSBIT, ABS_X);
    ioctl(uinput_fd, UI_SET_ABSBIT, ABS_Y);
    ioctl(uinput_fd, UI_SET_ABSBIT, ABS_RX);
    ioctl(uinput_fd, UI_SET_ABSBIT, ABS_RY);

    struct uinput_user_dev uidev;
    memset(&uidev, 0, sizeof(uidev));
    snprintf(uidev.name, UINPUT_MAX_NAME_SIZE, "STM32 Virtual Joystick");
    uidev.id.bustype = BUS_USB;
    uidev.id.vendor  = 0x1234;
    uidev.id.product = 0x5678;
    uidev.id.version = 1;

    uidev.absmin[ABS_X] = AXIS_MIN;
    uidev.absmax[ABS_X] = AXIS_MAX;
    uidev.absmin[ABS_Y] = AXIS_MIN;
    uidev.absmax[ABS_Y] = AXIS_MAX;
    uidev.absmin[ABS_RX] = AXIS_MIN;
    uidev.absmax[ABS_RX] = AXIS_MAX;
    uidev.absmin[ABS_RY] = AXIS_MIN;
    uidev.absmax[ABS_RY] = AXIS_MAX;

    write(uinput_fd, &uidev, sizeof(uidev));
    ioctl(uinput_fd, UI_DEV_CREATE);

    char buf[256];
    int ax, ay, bx, by, bt1, bt2;

    printf("Esperando dados do STM32 via %s...\n", serial_device);

    while (1) {
        int len = read(serial_fd, buf, sizeof(buf) - 1);
        if (len <= 0) continue;
        buf[len] = '\0';

        // Busca dentro do buffer a primeira substring começando com 'a'
        char* start = strchr(buf, 'a');
        if (!start) continue;

        // Copia a linha para parsing
        char line[256];
        strncpy(line, start, sizeof(line));
        line[sizeof(line) - 1] = '\0';

        if (sscanf(line, "aX:%d aY:%d bX:%d bY:%d | BTN1:%d BTN2:%d",
           &ax, &ay, &bx, &by, &bt1, &bt2) == 6) {

    // Assumindo ADC de 8 bits (0–255). Se for 10 bits, mude para 1023.
    const int adc_max = 255;

    int norm_ax = ((ax * (AXIS_MAX - AXIS_MIN)) / adc_max) + AXIS_MIN;
    int norm_ay = ((ay * (AXIS_MAX - AXIS_MIN)) / adc_max) + AXIS_MIN;
    int norm_bx = ((bx * (AXIS_MAX - AXIS_MIN)) / adc_max) + AXIS_MIN;
    int norm_by = ((by * (AXIS_MAX - AXIS_MIN)) / adc_max) + AXIS_MIN;

    // 🛑 Zona morta: ignora pequenas oscilações perto do centro (ajuste se quiser)
    if (abs(norm_ax) < 3000) norm_ax = 0;
    if (abs(norm_ay) < 3000) norm_ay = 0;
    if (abs(norm_bx) < 3000) norm_bx = 0;
    if (abs(norm_by) < 3000) norm_by = 0;

    emit(uinput_fd, EV_ABS, ABS_X, norm_ax);
    emit(uinput_fd, EV_ABS, ABS_Y, norm_ay);
    emit(uinput_fd, EV_ABS, ABS_RX, norm_bx);
    emit(uinput_fd, EV_ABS, ABS_RY, norm_by);
    emit(uinput_fd, EV_KEY, BTN_TRIGGER, bt1);
    emit(uinput_fd, EV_KEY, BTN_THUMB, bt2);
    emit(uinput_fd, EV_SYN, SYN_REPORT, 0);
}


        usleep(5000); // 5 ms
    }

    ioctl(uinput_fd, UI_DEV_DESTROY);
    close(uinput_fd);
    close(serial_fd);

    return 0;
}

