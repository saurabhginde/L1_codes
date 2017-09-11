#include <errno.h>      // Error number definitions
#include <stdint.h>     // C99 fixed data types
#include <stdio.h>      // Standard input/output definitions
#include <stdlib.h>     // C standard library
#include <string.h>     // String function definitions
#include <unistd.h>     // UNIX standard function definitions
#include <fcntl.h>      // File control definitions
#include <termios.h>    //usb library
#include<netinet/in.h>  //socket transmission
#include <sys/types.h>  //socket transmission
#include <sys/socket.h> //socket treansmission
#include <arpa/inet.h> //socket transmission
#include <time.h>   //getting system time

struct pid_info{
    char pid_hex[2];
    int polled_on_request;
    int broadcast;
    int data_length;
    int read_size;
    int wait_time;
    //char mid_hex[2];
    char filter_1[10]   ;
    //char filter_2[10];
    int read_index;
};

//global variable definitions
char packet_content[1000];
char pid_data[200];
char pid_data_dtc[250];
//char buf_2[100];

struct pid_info pid_descriptor(char[]);

int ascii_analysis(char[]);

int pid_monitor(struct pid_info, int, char[]);

char *pid_data_extractor(char[], int, int);

int request_polling(char *command, int wait_t, int read_s, int port_desc, int data_length, int read_index, int dtc);

char *dtc_pid_data_extractor(char monitored_string[], int read_index, int data_size);

// Open usb-serial port for reading & writing
int open_port(void){

    int fd;    // File descriptor for the port
    fd = open("/dev/ttyUSB2", O_RDWR | O_NOCTTY);

    if (fd == -1){
        fprintf(stderr, "open_port: Unable to open /dev/ttyUSB0 %s\n",strerror(errno));
        exit(EXIT_FAILURE);
    }

    else
        fcntl(fd, F_SETFL, 0);

    return fd;
}


int main(void){
    int rc;
    int fd;     //port descriptor
    struct termios   options;       //com port settings
    char buf[200];
    int tcflush_resp;
    int write_resp;
    int read_resp;
    struct pid_info pid_attr;
    int i;
    int resp_pid_monitor;
    int ecu_off_flag = 0;

    //struct pid_info pids_polling_list[4];

    fd = open_port();

    if((rc = tcgetattr(fd, &options)) < 0){
        fprintf(stderr, "failed to get attr: %d, %s\n", fd, strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Set the baud rates to 38400
    cfsetispeed(&options, B57600);

    // Set the baud rates to 38400
    cfsetospeed(&options, B57600);

    //cfmakeraw(&options);
    options.c_cflag |= (CLOCAL | CREAD);   // Enable the receiver and set local mode
    options.c_cflag &= ~PARENB;             //No parity
    options.c_cflag &= ~CSTOPB;            // 1 stop bit
    options.c_cflag &= ~CRTSCTS;           // Disable sable hardware flow control
    options.c_cflag &= ~CSIZE;             // Mask the character size bits
    options.c_cflag |= CS8;                // Select 8 data bits
    //options.c_lflag |= (ICANON | ECHO | ECHOE);     //canonical

    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);     //raw

    options.c_cc[VMIN]  = 0;
    options.c_cc[VTIME] = 50;

    if((rc = tcsetattr(fd, TCSANOW, &options)) < 0){
        fprintf(stderr, "failed to set attr: %d, %s\n", fd, strerror(errno));
        exit(EXIT_FAILURE);
    }

    //at command set up of IC

    RESET:

    ecu_off_flag = 0;

    //full reset
    usleep(250000); // delay value cchanged from 0.5s to 0.25s

    memset(&buf, '\0', sizeof buf);
    tcflush_resp = tcflush(fd, TCIOFLUSH);

    write_resp = write(fd, "ATWS\r", 5);
    usleep(1000000);// delay value cchanged from 2.5s to 1s
    read_resp = read(fd, &buf, 20);

    printf("\nFull hardware reset\n");
    printf("\n%s\n", buf);

    usleep(250000);    // delay value cchanged from 0.5s to 0.25s

    //terminate line feeds
    memset(&buf, '\0', sizeof buf);
    tcflush_resp = tcflush(fd, TCIOFLUSH);

    write_resp = write(fd, "ATL0\r", 5);
    usleep(1000000);// delay value cchanged from 2.5s to 1s
    read_resp = read(fd, &buf, 8);

    printf("\nstop linefeed\n");
    printf("\n%s\n", buf);

    usleep(250000);// delay value cchanged from 0.5s to 0.25s

    //terminate spaces
    memset(&buf, '\0', sizeof buf);
    tcflush_resp = tcflush(fd, TCIOFLUSH);

    write_resp = write(fd, "ATS0\r", 5);
    usleep(1000000);// delay value cchanged from 2.5s to 1s
    read_resp = read(fd, &buf, 8);

    printf("\nterminate spaces\n");
    printf("\n%s\n", buf);

    usleep(250000);// delay value cchanged from 0.5s to 0.25s



    char *pid_polling_list[] = {    //page1 pids
        "60","6E","AF","9E","69","66","2C","6C","AC","64","5E","F7","54","5B"
    };

    while(!ecu_off_flag){
        memset(&packet_content, '\0', sizeof packet_content);   //flash packet data
        strcpy(packet_content, "#J1587#");

        for(i = 0; i < sizeof(pid_polling_list) / sizeof(pid_polling_list[0]); i++){
            // BE ,B7 and 5C have been removed as data is not available from euro 4 ECU
            // ecu switch off logic written after the if and elseif statements

            //BE : A.190 Engine Speed
            pid_attr = pid_descriptor("BE");
            resp_pid_monitor = request_polling(pid_attr.pid_hex, pid_attr.wait_time, pid_attr.read_size, fd, pid_attr.data_length, pid_attr.read_index, 0);   //dtc flag = 0
            strcat(packet_content, ",");

            //B7 : A.183 Fuel Rate (Instantaneous)
            pid_attr = pid_descriptor("B7");
            request_polling(pid_attr.pid_hex, pid_attr.wait_time, pid_attr.read_size, fd, pid_attr.data_length, pid_attr.read_index, 0);   //dtc flag = 0
            strcat(packet_content, ",");

            //5C : A.92  Percent Engine Load
            pid_attr = pid_descriptor("5C");
            request_polling(pid_attr.pid_hex, pid_attr.wait_time, pid_attr.read_size, fd, pid_attr.data_length, pid_attr.read_index, 0);   //dtc flag = 0
            strcat(packet_content, ",");



            pid_attr = pid_descriptor(pid_polling_list[i]);
            if(pid_attr.broadcast){
                pid_monitor(pid_attr, fd, pid_attr.filter_1); //response from PID monitor stored in resp_pid_monitor variable
                strcat(packet_content, ";");
            }
            else if(pid_attr.polled_on_request){
               request_polling(pid_attr.pid_hex, pid_attr.wait_time, pid_attr.read_size, fd, pid_attr.data_length, pid_attr.read_index, 0); //response frpm PID monitor stored in resp_pid_monitor variable    //dtc flag = 0
                strcat(packet_content, ";");
            }

            // engine off decision is based on response for 60 : secondary PID also its not needed in firmware code as engine off decision made on CAN
            if(resp_pid_monitor == 2){
                printf("\nECU is switched off/No data\n");
                ecu_off_flag = 1;
                goto RESET;
            }
        }

        //polling dtc's
        strcat(packet_content, "#");
        pid_attr = pid_descriptor("C2");
        request_polling(pid_attr.pid_hex, pid_attr.wait_time, pid_attr.read_size, fd, pid_attr.data_length, pid_attr.read_index, 1);    //dtc flag = 1
        strcat(packet_content, "#");
    }

    close(fd);
}

struct pid_info pid_descriptor(char pid_hex[]){
    int length_of_pid_chart = 18;
    struct pid_info pid_chart[length_of_pid_chart];
    int i;

    //pid BE:engine rpm
    strcpy(pid_chart[0].pid_hex, "BE");
    pid_chart[0].polled_on_request = 1;
    pid_chart[0].broadcast = 0;
    pid_chart[0].data_length = 2;
    pid_chart[0].read_size = 38;
    pid_chart[0].wait_time = 2500000;
    strcpy(pid_chart[0].filter_1, "80BEXXXXXX");
    pid_chart[0].read_index = 4;
    //strcpy(pid_chart[0].filter_2, "XXXXXXXXXX");

    //pid B7:fuel rate
    strcpy(pid_chart[1].pid_hex, "B7");
    pid_chart[1].polled_on_request = 1;
    pid_chart[1].broadcast = 0;
    pid_chart[1].data_length = 2;
    pid_chart[1].read_size = 18;
    pid_chart[1].wait_time = 6000000;
    strcpy(pid_chart[1].filter_1, "XXXXXXB7XX");
    pid_chart[1].read_index = 4;
    //strcpy(pid_chart[1].filter_2, "XXXXXXXXXX");

    //pid 5C:engine load
    strcpy(pid_chart[2].pid_hex, "5C");
    pid_chart[2].polled_on_request = 1;
    pid_chart[2].broadcast = 0;
    pid_chart[2].data_length = 1;
    pid_chart[2].read_size = 38;
    pid_chart[2].wait_time = 6000000;
    strcpy(pid_chart[2].filter_1, "XX5CXXXXXX");
    pid_chart[2].read_index = 4;
    //strcpy(pid_chart[2].filter_2, "XXXXXXXXXX");

    //pid 5C:fuel rate
    strcpy(pid_chart[3].pid_hex, "60");
    pid_chart[3].polled_on_request = 1;  // changed from 0 to 1
    pid_chart[3].broadcast = 0; // changed from 1 to 0
    pid_chart[3].data_length = 1;
    pid_chart[3].read_size = 14;
    pid_chart[3].wait_time = 3000000; // wait time reduced from 12s to 3s
    strcpy(pid_chart[3].filter_1, "8CXXXX60XX");
    pid_chart[3].read_index = 4;
    //strcpy(pid_chart[3].filter_2, "XXXXXXXXXX");

    //pid C2: active error codes
    strcpy(pid_chart[4].pid_hex, "C2");
    pid_chart[4].polled_on_request = 1;
    pid_chart[4].broadcast = 0;
    pid_chart[4].data_length = 0;   //to be assigned based on data payload
    pid_chart[4].read_size = 200;
    pid_chart[4].wait_time = 10000000; // wait time reduced from 15s to 10s
    strcpy(pid_chart[4].filter_1, "XXC2XXXXXX");
    pid_chart[4].read_index = 0;    //complete response needs to be send across to distinguish source ECus

    //pid 6E :  Engine Coolant Temperature
    strcpy(pid_chart[5].pid_hex, "6E");
    pid_chart[5].polled_on_request = 1;
    pid_chart[5].broadcast = 0;
    pid_chart[5].data_length = 1;   //changed
    pid_chart[5].read_size = 38;
    pid_chart[5].wait_time = 6000000;
    strcpy(pid_chart[5].filter_1, "XXXXXXXXXX");
    pid_chart[5].read_index = 4;

    //AF :  Engine Oil Temperature
    strcpy(pid_chart[6].pid_hex, "AF");
    pid_chart[6].polled_on_request = 1;
    pid_chart[6].broadcast = 0;
    pid_chart[6].data_length = 2;   //changed
    pid_chart[6].read_size = 38;
    pid_chart[6].wait_time = 6000000;
    strcpy(pid_chart[6].filter_1, "XXXXXXXXXX");
    pid_chart[6].read_index = 4;

    //9E :  Instantaneous Fuel Economy (Natural Gas)
    strcpy(pid_chart[7].pid_hex, "9E");
    pid_chart[7].polled_on_request = 1;
    pid_chart[7].broadcast = 0;
    pid_chart[7].data_length = 2;   //changed
    pid_chart[7].read_size = 12;
    pid_chart[7].wait_time = 6000000;
    strcpy(pid_chart[7].filter_1, "XXXXXXXXXX");
    pid_chart[7].read_index = 4;


    //69 :  Intake Manifold Temperature
    strcpy(pid_chart[8].pid_hex, "69");
    pid_chart[8].polled_on_request = 1;
    pid_chart[8].broadcast = 0;
    pid_chart[8].data_length = 1;   //changed
    pid_chart[8].read_size = 16;
    pid_chart[8].wait_time = 6000000;
    strcpy(pid_chart[8].filter_1, "XXXXXXXXXX");
    pid_chart[8].read_index = 4;

    //66 :  Boost Pressure
    strcpy(pid_chart[9].pid_hex, "66");
    pid_chart[9].polled_on_request = 1;
    pid_chart[9].broadcast = 0;
    pid_chart[9].data_length = 1;   //changed
    pid_chart[9].read_size = 38;
    pid_chart[9].wait_time = 6000000;
    strcpy(pid_chart[9].filter_1, "XXXXXXXXXX");
    pid_chart[9].read_index = 4;

    //2C : A.44  Attention/Warning Indicator Lamps Status
    strcpy(pid_chart[10].pid_hex, "2C");
    pid_chart[10].polled_on_request = 1;
    pid_chart[10].broadcast = 0;
    pid_chart[10].data_length = 1;   //changed
    pid_chart[10].read_size = 16;
    pid_chart[10].wait_time = 6000000;
    strcpy(pid_chart[10].filter_1, "XXXXXXXXXX");
    pid_chart[10].read_index = 4;

    //6c : A.108 Barometric Pressure
    strcpy(pid_chart[11].pid_hex, "6C");
    pid_chart[11].polled_on_request = 1;
    pid_chart[11].broadcast = 0;
    pid_chart[11].data_length = 1;   //changed
    pid_chart[11].read_size = 12;
    pid_chart[11].wait_time = 6000000;
    strcpy(pid_chart[11].filter_1, "XXXXXXXXXX");
    pid_chart[11].read_index = 4;

    //AC :  Air Inlet Temperature
    strcpy(pid_chart[12].pid_hex, "AC");
    pid_chart[12].polled_on_request = 1;
    pid_chart[12].broadcast = 0;
    pid_chart[12].data_length = 1;   //changed
    pid_chart[12].read_size = 12;
    pid_chart[12].wait_time = 6000000;
    strcpy(pid_chart[12].filter_1, "XXXXXXXXXX");
    pid_chart[12].read_index = 4;

    //64 :  Engine Oil Pressure
    strcpy(pid_chart[13].pid_hex, "64");
    pid_chart[13].polled_on_request = 1;
    pid_chart[13].broadcast = 0;
    pid_chart[13].data_length = 1;   //changed
    pid_chart[13].read_size = 38;
    pid_chart[13].wait_time = 6000000;
    strcpy(pid_chart[13].filter_1, "XXXXXXXXXX");
    pid_chart[13].read_index = 4;

    //5E :  Fuel Delivery Pressure
    strcpy(pid_chart[14].pid_hex, "5E");
    pid_chart[14].polled_on_request = 1;
    pid_chart[14].broadcast = 0;
    pid_chart[14].data_length = 1;   //changed
    pid_chart[14].read_size = 38;
    pid_chart[14].wait_time = 6000000;
    strcpy(pid_chart[14].filter_1, "XXXXXXXXXX");
    pid_chart[14].read_index = 4;

    //F7 :  Total Engine Hours
    strcpy(pid_chart[15].pid_hex, "F7");
    pid_chart[15].polled_on_request = 1;
    pid_chart[15].broadcast = 0;
    pid_chart[15].data_length = 4;   //changed
    pid_chart[15].read_size = 14;
    pid_chart[15].wait_time = 6000000;
    strcpy(pid_chart[15].filter_1, "XXXXXXXXXX");
    pid_chart[15].read_index = 4;

    //54 :  Road Speed
    strcpy(pid_chart[16].pid_hex, "54");
    pid_chart[16].polled_on_request = 1;
    pid_chart[16].broadcast = 0;
    pid_chart[16].data_length = 1;   //changed
    pid_chart[16].read_size = 16;
    pid_chart[16].wait_time = 6000000;
    strcpy(pid_chart[16].filter_1, "XXXXXXXXXX");
    pid_chart[16].read_index = 4;

    //5B :  Percent Accelerator Pedal Position
    strcpy(pid_chart[17].pid_hex, "5B");
    pid_chart[17].polled_on_request = 1;
    pid_chart[17].broadcast = 0;
    pid_chart[17].data_length = 1;   //changed
    pid_chart[17].read_size = 16;
    pid_chart[17].wait_time = 6000000;
    strcpy(pid_chart[17].filter_1, "XXXXXXXXXX");
    pid_chart[17].read_index = 4;



    for(i = 0; i < length_of_pid_chart; i++){
        if(strcmp(pid_chart[i].pid_hex, pid_hex) == 0){
            break;
        }
    }

    return(pid_chart[i]);
}

int ascii_analysis(char buf[]){
    int i;
    int len;

    for(i = 0; i < strlen(buf); i++){
        printf("\ncharacter: %c + ascii value: %d\n", buf[i], buf[i]);
    }
    len = strlen(buf);
    printf("\ntotal length of payload in buf: %d\n", len);

    return 0;
}

int pid_monitor(struct pid_info pid_prop, int port_desc, char f1_filter[]){
    char buf[200];
    char pid_payload[100] = "";
    int tcflush_resp;
    int write_resp;
    int read_resp;
    char f1_filter_command[15];
    int i, j;

    strcpy(f1_filter_command, "ATF1");
    strcat(f1_filter_command, f1_filter);
    strcat(f1_filter_command, "\r");
    printf("\n\nfilter 1 command:");
    printf("\n%s", f1_filter_command);

    //executing f1 filter
    usleep(1000000);

    memset(&buf, '\0', sizeof(buf));
    tcflush_resp = tcflush(port_desc, TCIOFLUSH);

    write_resp = write(port_desc, f1_filter_command, strlen(f1_filter_command));
    usleep(1000000); //// wait time reduced from 2.5s  to 1s
    read_resp = read(port_desc, &buf, 17);

    printf("\n\nf1 filter response:");
    printf("\n%s", buf);
    //-------------------

    //execute monitor all command
    usleep(1000000);

    memset(&buf, '\0', sizeof(buf));
    tcflush_resp = tcflush(port_desc, TCIOFLUSH);

    write_resp = write(port_desc, "ATMA\r", 5);
    usleep(pid_prop.wait_time);
    read_resp = read(port_desc, &buf, pid_prop.read_size);
    //usleep(300000);
    //---------------------------

    //terminate monitor
    write_resp = write(port_desc, "-", 1);
    usleep(2000000);// wait time reduced from 3.5s  to 2s
    //-----------------

    //terminate f1 filter
    //usleep(500000); //comment this out

    write_resp = write(port_desc, "ATF1\r", 5);
    usleep(2500000);
    //no read
    //------------------

    printf("\n\nmonitor all response:");
    printf("\n%s", buf);

    printf("\nlength of buf: %ld\n", strlen(buf));

    for(i = 0, j = 5; (i < sizeof(pid_payload)) && (j < strlen(buf)); i++, j++){
        pid_payload[i] = buf[j];
    }
    if(strlen(pid_payload) == 0){
       return(2);   //no data available
    }
    else if(pid_payload[0] != 56 && pid_payload[0] != 57 && (pid_payload[0] < 65 || pid_payload[0] > 70)){  //repsonse from peripheral ECUs and error code handling
       return(2);
    }
    else{
        strcat(packet_content, pid_prop.pid_hex);
        strcat(packet_content, "=");
        pid_data_extractor(pid_payload, pid_prop.read_index, pid_prop.data_length);
        strcat(packet_content, pid_data);
        printf("\n%s\n", packet_content);
    }

    return(0);
}

char *pid_data_extractor(char monitored_string[], int read_index, int data_size){
    //char *pid_data;
    memset(&pid_data, '\0', sizeof(pid_data));
    int i, j;

    //pid_data = (char *) malloc((data_size * 2) + 1);

    for(i = 0, j = read_index; i < (data_size * 2) && j < strlen(monitored_string) ; i++, j++){
        if( (monitored_string[j] >= 48 && monitored_string[j] < 58) || (monitored_string[j] >= 65 && monitored_string[j] <= 70) || monitored_string[j] == '\n' || monitored_string[j] == '\r'){
            if(monitored_string[j] == '\n' || monitored_string[j] == '\r'){
                monitored_string[j] = '-';
            }
            pid_data[i] = monitored_string[j];
        }
    }

    //pid_data[sizeof(pid_data) - 1] = '\0';
    printf("\n\npid data after extraction:");
    printf("\n%s", pid_data);

    return pid_data;
}

int request_polling(char *command, int wait_t, int read_s, int port_desc, int data_length, int read_index, int dtc){
    char write_command[5] = "00";
    char pid_payload[200] = "";
    char buf[200];
    int tcflush_resp;
    int read_resp;
    int write_resp;
    int i, j;

    strcat(write_command, command);
    strcat(write_command, "\r");
    printf("\n\npid request command:");
    printf("\n%s", write_command);

    //setting internal timers for dtc response
    if(dtc){
        usleep(500000);// delay time changed from 1 sec to 0.5s

        memset(&buf, '\0', sizeof buf);
        tcflush_resp = tcflush(port_desc, TCIOFLUSH);

        write_resp = write(port_desc, "ATST64\r", 7);   //internal timer extended to ten seconds for dtc polling
        usleep(1000000);// delay time changed from 2 sec to 1s
        read_resp = read(port_desc, &buf, 10);
        printf("\n\nrepsonse to extending internal timer:");
        printf("\n%s", buf);
    }
    //----------------------------------------

    //pid polling
    usleep(1000000);

    memset(&buf, '\0', sizeof buf);
    tcflush_resp = tcflush(port_desc, TCIOFLUSH);

    write_resp = write(port_desc, write_command, strlen(write_command));
    usleep(wait_t);
    read_resp = read(port_desc, &buf, read_s);
    //-----------

    printf("\n\nresponse to request:");
    printf("\n%s", buf);

    //releasing data bus
    write_resp = write(port_desc, "-", 1);
    usleep(2000000);// delay time changed from 5 sec to 2s
    //------------------

    //printf("\n\nresponse to request:");
    //printf("\n%s", buf);

    for(i = 0, j = 5; i < sizeof(pid_payload) && j < strlen(buf); i++, j++){
        pid_payload[i] = buf[j];
    }

    //setting internal timers for dtc response
    if(dtc){
        usleep(500000);// delay time changed from 1 sec to 0.5s

        memset(&buf, '\0', sizeof buf);
        tcflush_resp = tcflush(port_desc, TCIOFLUSH);

        write_resp = write(port_desc, "ATST00\r", 7);   //internal timer extended to ten seconds for dtc polling
        usleep(1000000);// delay time changed from 2 sec to 1s
        read_resp = read(port_desc, &buf, 10);
        printf("\n\nrepsonse to resetting internal timer:");
        printf("\n%s",buf);
    }

    if(strlen(pid_payload) == 0){
        return(2);   //no data available
    }
    else if(pid_payload[0] != 56 && pid_payload[0] != 57 && (pid_payload[0] < 65 || pid_payload[0] > 70)){  //repsonse from peripheral ECUs and error code handling
        return(2);
    }
    else{
        strcat(packet_content, command);
        strcat(packet_content, "=");

        if(!dtc){
            strcat(packet_content, pid_data_extractor(pid_payload, read_index, data_length));
        }
        else{       //handling dtc response
            data_length = strlen(pid_payload);
            printf("\n\ndata size of dtc payload: %d", data_length);
            //strcat(packet_content, dtc_pid_data_extractor(pid_payload, read_index, data_length));
            dtc_pid_data_extractor(pid_payload, read_index, data_length);
            strcat(packet_content, pid_data_dtc);
        }

        printf("\n\npacket  content:");
        printf("\n%s", packet_content);
    }

    return 0;
}

char *dtc_pid_data_extractor(char monitored_string[], int read_index, int data_size){
    //char pid_data[200];
    memset(&pid_data_dtc, '\0', sizeof pid_data_dtc);
    int i, j;
    //int count = 0;

    printf("\n\nmonitored dtc string:");
    printf("\n%s", monitored_string);

    //pid_data = (char *) malloc(data_size + 1);
    printf("\n\nsize of pid data: %ld", sizeof pid_data_dtc);

    for(i = 0, j = read_index; i < (sizeof pid_data_dtc) && j < strlen(monitored_string) ; i++, j++){ //data size is equal to lenth of monitored string
        //count++;
        //printf("\n\ncount = %d", count);
        if( (monitored_string[j] >= 48 && monitored_string[j] < 58) || (monitored_string[j] >= 65 && monitored_string[j] <= 70) || monitored_string[j] == '\n' || monitored_string[j] == '\r'){
            if(monitored_string[j] == '\n' || monitored_string[j] == '\r'){
                monitored_string[j] = '-';
            }
            pid_data_dtc[i] = monitored_string[j];
            //printf("\n\n%s", pid_data_dtc);
        }
    }

    //pid_data[sizeof(pid_data) - 1] = '\0';
    printf("\n\ndtc data after extraction:");
    printf("\n%s", pid_data_dtc);

    //return pid_data;
}



