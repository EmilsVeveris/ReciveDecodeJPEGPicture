#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <windows.h>
#include <time.h>
#include <signal.h>
#include <stdlib.h>

//Function prototypes
void    INThandler(int);
void    print_error(const char * context);
HANDLE  open_serial_port(const char * device, uint32_t baud_rate);
SSIZE_T read_port(HANDLE port, uint8_t * buffer, size_t size);
int     jrk_get_variable(HANDLE port, uint8_t offset, uint8_t * buffer, uint8_t length);

//global variable
HANDLE port;

int main()
{
  
signal(SIGINT, INThandler);

int check;
struct timeval tv;
struct timezone tz;
struct tm *today;
int zone;
char dirname[128];

// Choose the serial port name. 
const char * device = "\\\\.\\COM4";

// Choose the baud rate (bits per second).
uint32_t baud_rate = 115200;

uint8_t buffer[1];
uint8_t buffer_t[1];
uint8_t check_buffer[4];
char ch;
int i = 0, x = 0;
int count = 0;

FILE* pOutput;

char jpeg[6];
unsigned char b;
int  startingByte1 = 255;
int  startingByte2 = 216;
int  startingByte3 = 255;

HANDLE port = open_serial_port(device, baud_rate);
if (port == INVALID_HANDLE_VALUE) { return 1; }
  printf("Connected to usb.\n");
 
while(1)
{
  
    while(1)
    {
        int result = jrk_get_variable(port, 0x02, buffer_t, sizeof(buffer_t));
        check_buffer[2] = check_buffer[1];
        check_buffer[1] = check_buffer[0];
        check_buffer[0] = buffer_t[0];

        if((check_buffer[0] == startingByte1) && (check_buffer[1] == startingByte2) && (check_buffer[2] == startingByte3))
        {
            printf("If is true\n");
            time_t t = time(NULL);
            struct tm tm = *localtime(&t);
            sprintf(dirname,"pictures/%d-%02d-%02d-%02d-%02d-%02d.jpg", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
            pOutput = fopen(dirname,"wb");
            b=(unsigned char)startingByte1;
            fwrite(&b, 1, sizeof(b), pOutput);
            b=(unsigned char)startingByte2;
            fwrite(&b, 1, sizeof(b), pOutput);
            b=(unsigned char)startingByte3;
            fwrite(&b, 1, sizeof(b), pOutput);
            count = 3;
            break;
        }
    }
 
    while(1)
    {

        int result = jrk_get_variable(port, 0x02, buffer, sizeof(buffer));
        if(result == 0)
        {

        check_buffer[3] = check_buffer[2];
        check_buffer[2] = check_buffer[1];
        check_buffer[1] = check_buffer[0];
        check_buffer[0] = buffer[0];
        count = count + 1;
        

        b=(unsigned char)buffer[0];
        fwrite(&b, 1, sizeof(b), pOutput);

        if((check_buffer[2] == 217) && (check_buffer[3] == 255))
        {
            printf("count = : %d\n", count);
            printf("Picture is recived\n");
            printf("check_buffer 0 = : %d\n", check_buffer[0]);
            printf("check_buffer 1 = : %d\n", check_buffer[1]);
            break;
        }
        }
        
    }
  fclose(pOutput);
  
  
  printf("Picture file is closed\n");
  
  x = x+1;
}

CloseHandle(port);
return 0;
}

void  INThandler(int sig)
{
     char  c;

     signal(sig, SIG_IGN);
     printf("Did you hit Ctrl-C?\n"
            "Do you really want to quit? [y/n] ");
     c = getchar();
     if (c == 'y' || c == 'Y')
     {
          CloseHandle(port);
          exit(0);
     }
     else
          signal(SIGINT, INThandler);
     getchar(); // Get new line character
}

void print_error(const char * context)
{
  DWORD error_code = GetLastError();
  char buffer[256];
  DWORD size = FormatMessageA(
    FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_MAX_WIDTH_MASK,
    NULL, error_code, MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
    buffer, sizeof(buffer), NULL);
  if (size == 0) { buffer[0] = 0; }
  fprintf(stderr, "%s: %s\n", context, buffer);
}

// Opens the specified serial port, configures its timeouts, and sets its
// baud rate.  Returns a handle on success, or INVALID_HANDLE_VALUE on failure.
HANDLE open_serial_port(const char * device, uint32_t baud_rate)
{
  HANDLE port = CreateFileA(device, GENERIC_READ | GENERIC_WRITE, 0, NULL,
    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (port == INVALID_HANDLE_VALUE)
  {
    print_error(device);
    return INVALID_HANDLE_VALUE;
  }
 
  // Flush away any bytes previously read or written.
  BOOL success = FlushFileBuffers(port);
  if (!success)
  {
    print_error("Failed to flush serial port");
    CloseHandle(port);
    return INVALID_HANDLE_VALUE;
  }
 
  // Configure read and write operations to time out after 100 ms.
  COMMTIMEOUTS timeouts = {0};
  timeouts.ReadIntervalTimeout = 0;
  timeouts.ReadTotalTimeoutConstant = 200;
  timeouts.ReadTotalTimeoutMultiplier = 0;
  timeouts.WriteTotalTimeoutConstant = 100;
  timeouts.WriteTotalTimeoutMultiplier = 0;
 
  success = SetCommTimeouts(port, &timeouts);
  if (!success)
  {
    print_error("Failed to set serial timeouts");
    CloseHandle(port);
    return INVALID_HANDLE_VALUE;
  }
 
  // Set the baud rate and other options.
  DCB state = {0};
  state.DCBlength = sizeof(DCB);
  state.BaudRate = baud_rate;
  state.ByteSize = 8;
  state.Parity = NOPARITY;
  state.StopBits = ONESTOPBIT;
  success = SetCommState(port, &state);
  if (!success)
  {
    print_error("Failed to set serial settings");
    CloseHandle(port);
    return INVALID_HANDLE_VALUE;
  }
 
  return port;
}

// Reads bytes from the serial port.
// Returns after all the desired bytes have been read, or if there is a
// timeout or other error.
// Returns the number of bytes successfully read into the buffer, or -1 if
// there was an error reading.
SSIZE_T read_port(HANDLE port, uint8_t * buffer, size_t size)
{
  DWORD received;
  BOOL success = ReadFile(port, buffer, size, &received, NULL);
  if (!success)
  {
    print_error("Failed to read from port");
    return -1;
  }
  return received;
}

int jrk_get_variable(HANDLE port, uint8_t offset, uint8_t * buffer, uint8_t length)
{
  SSIZE_T received = read_port(port, buffer, length);
  if (received < 0) { return -1; }
  if (received != length)
  {
   // fprintf(stderr, "read timeout: expected %u bytes, got %lld\n", length, (int64_t)received);
    return -1;
  }
  return 0;
}