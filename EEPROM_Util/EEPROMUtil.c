// eeprog - 28C64/28C256 EEPROM programmer host program
// Copyright (c) 2019, David H. Hovemeyer <david.hovemeyer@gmail.com>

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

//TO BUILD: gcc -Wall -o eeprog.exe EEPROMUtil.c

#include <stdint.h>
#include <errno.h>
#include <fcntl.h> 
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <windows.h>
#define strdup _strdup
#else
#include <termios.h>
#include <unistd.h>
#endif

#define VER_MAJOR 1
#define VER_MINOR 1

//
// IO channel data type.
// Files use a FILE*; on Windows the serial port uses a HANDLE
// (and fh is NULL), on POSIX the serial port uses fd and fh.
//
struct IO {
	const char *fileName;
#ifdef _WIN32
	HANDLE h;
#else
	int fd;
#endif
	FILE *fh;
};

#ifdef _WIN32
//
// Minimal getopt implementation (not available with MSVC).
//
static char *eeprog_optarg;
static int eeprog_optind = 1;
static int eeprog_optpos = 0;

static int eeprog_getopt(int argc, char **argv, const char *opts) {
	eeprog_optarg = NULL;
	if (eeprog_optpos == 0) {
		if (eeprog_optind >= argc || argv[eeprog_optind][0] != '-' || argv[eeprog_optind][1] == '\0') {
			return -1;
		}
		if (strcmp(argv[eeprog_optind], "--") == 0) {
			eeprog_optind++;
			return -1;
		}
		eeprog_optpos = 1;
	}
	char c = argv[eeprog_optind][eeprog_optpos++];
	const char *p = (c == ':') ? NULL : strchr(opts, c);
	if (!p) {
		if (!argv[eeprog_optind][eeprog_optpos]) { eeprog_optind++; eeprog_optpos = 0; }
		return '?';
	}
	if (p[1] == ':') {
		if (p[2] == ':') {
			/* Optional argument ("::") */
			if (argv[eeprog_optind][eeprog_optpos]) {
				/* Argument is attached directly, e.g., -oArgument */
				eeprog_optarg = &argv[eeprog_optind][eeprog_optpos];
				eeprog_optind++;
			} else {
				/* No attached argument, e.g., -o. Argument is omitted */
				eeprog_optind++;
			}
			eeprog_optpos = 0;
		} else {
			/* Required argument (":") */
			if (argv[eeprog_optind][eeprog_optpos]) {
				eeprog_optarg = &argv[eeprog_optind][eeprog_optpos];
				eeprog_optind++;
			} else if (eeprog_optind + 1 < argc) {
				eeprog_optarg = argv[eeprog_optind + 1];
				eeprog_optind += 2;
			} else {
				eeprog_optind++;
				eeprog_optpos = 0;
				return '?';
			}
			eeprog_optpos = 0;
		}
	} else if (!argv[eeprog_optind][eeprog_optpos]) {
		eeprog_optind++;
		eeprog_optpos = 0;
	}
	return c;
}

#define getopt eeprog_getopt
#define optarg eeprog_optarg
#define optind eeprog_optind
#endif

//
// Global variables
//
const char *g_port, *g_fileName, *g_outputFileName;
int g_writeProtectEnable, g_writeProtectDisable;
int g_verify;
int g_erase;
off_t g_dataReadSize = -1;
int g_showHelp;


off_t g_dataSize;
uint16_t g_eepromSize = 32768; // 32K bytes
uint8_t g_dataBuf[65536];
uint8_t g_dataReadBuf[65536];
uint16_t g_startAddr=0;

//
// Configure serial communication parameters:
// code stolen from:
//   https://stackoverflow.com/questions/6947413/how-to-open-read-and-write-from-serial-port-in-c
// (POSIX only; Windows configures the port in io_openComm.)
//
#ifndef _WIN32
int set_interface_attribs(int fd, int speed) {
	struct termios tty;

	if (tcgetattr(fd, &tty) < 0) {
		printf("Error from tcgetattr: %s\n", strerror(errno));
		return -1;
	}

	cfsetospeed(&tty, (speed_t)speed);
	cfsetispeed(&tty, (speed_t)speed);

	tty.c_cflag |= (CLOCAL | CREAD);	/* ignore modem controls */
	tty.c_cflag &= ~CSIZE;
	tty.c_cflag |= CS8;		 /* 8-bit characters */
	tty.c_cflag &= ~PARENB;	 /* no parity bit */
	tty.c_cflag &= ~CSTOPB;	 /* only need 1 stop bit */
	tty.c_cflag &= ~CRTSCTS;	/* no hardware flowcontrol */

	/* setup for non-canonical mode */
	tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
	tty.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
	tty.c_oflag &= ~OPOST;

	/* fetch bytes as they become available */
	tty.c_cc[VMIN] = 1;
	tty.c_cc[VTIME] = 1;

	if (tcsetattr(fd, TCSANOW, &tty) != 0) {
		printf("Error from tcsetattr: %s\n", strerror(errno));
		return -1;
	}
	return 0;
}
#endif

//
// Display an error message.
//
void display_error(const char *tag, const char *fmt, va_list args) {
	fprintf(stderr, "%s: ", tag);
	vfprintf(stderr, fmt, args);
}

//
// Display an illegal state error message, meaning
// that an illegal state has been reached (possibly because
// of bad input), but there is no errno value from which
// to produce a more specific diagnostic.
// Terminates the program.
//
void illegal_state(const char *fmt, ...) {
	va_list args;

	va_start(args, fmt);
	display_error("Error", fmt, args);
	fprintf(stderr, "\n");
	va_end(args);

	exit(1);
}

//
// Display a fatal runtime error, in which errno is set
// and we can print a meaningful diagnostic.
// Terminates the program.
//
void fatal(const char *fmt, ...) {
	va_list args;

	va_start(args, fmt);
	display_error("Error", fmt, args);
	fprintf(stderr, ": %s", strerror(errno));
	fprintf(stderr, "\n");
	va_end(args);

	exit(1);
}

#ifdef _WIN32
//
// Display a fatal error using the Win32 last-error code.
// Terminates the program.
//
void fatalWin(const char *fmt, ...) {
	DWORD err = GetLastError();
	char msg[256] = "";
	va_list args;

	FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, err, 0, msg, sizeof(msg), NULL);
	// strip trailing CR/LF
	size_t len = strlen(msg);
	while (len > 0 && (msg[len-1] == '\r' || msg[len-1] == '\n')) { msg[--len] = '\0'; }

	va_start(args, fmt);
	display_error("Error", fmt, args);
	fprintf(stderr, ": %s (error %lu)\n", msg, (unsigned long) err);
	va_end(args);

	exit(1);
}
#endif

//
// Display a non-fatal warning message.
//
void warn(const char *fmt, ...) {
	va_list args;

	va_start(args, fmt);
	display_error("Warning", fmt, args);
	fprintf(stderr, "\n");
	va_end(args);
}

//
// Display help information.
//
void showHelp() {
	printf("Usage: eeprog <options>\n"
		"Options are:\n"
		"  -e                erase EEPROM or erase before writing\n"
		"  -f <filename>     specify input filename\n"
		"  -p <port>         specify comm port\n"
		"  -o <filename>     specify output filename\n"
		"  -s <size>         specify EEPROM size in bytes (1-65536, default:32768)\n"
		"  -r <num bytes>    specify number of bytes to read\n"
		"  -N                enable write protection\n"
		"  -D                disable write protection\n"
		"  -v                verify data after writing\n"
		"  -h                print this help text\n"
		);
}

//
// Allocate memory buffer.
//
void *xmalloc(size_t n) {
	void *buf = malloc(n);
	if (!buf) { fatal("memory allocation failure"); }
	return buf;
}

//
// Duplicate a character string.
//
char *xstrdup(const char *s) {
	char *copy = strdup(s);
	if (!copy) { fatal("memory allocation failure"); }
	return copy;
}

//
// Open a regular file.  mode is an fopen mode string; always use
// binary modes ("rb"/"wb") so that Windows doesn't translate line endings.
//
struct IO *io_openFile(const char *fileName, const char *mode) {
	struct IO *io = xmalloc(sizeof(struct IO));
	memset(io, 0, sizeof(*io));
#ifdef _WIN32
	io->h = INVALID_HANDLE_VALUE;
#else
	io->fd = -1;
#endif
	io->fileName = fileName;
	io->fh = fopen(fileName, mode);
	if (!io->fh) { fatal("couldn't open '%s'", fileName); }
	return io;
}

//
// Open and configure the serial port (57600 baud, 8N1, no flow control).
//
struct IO *io_openComm(const char *port) {
	struct IO *io = xmalloc(sizeof(struct IO));
	memset(io, 0, sizeof(*io));
	io->fileName = port;
#ifdef _WIN32
	// Ports above COM9 need the \\.\ prefix; it is valid for all ports.
	char path[256];
	if (strncmp(port, "\\\\.\\", 4) == 0) {
		snprintf(path, sizeof(path), "%s", port);
	} else {
		snprintf(path, sizeof(path), "\\\\.\\%s", port);
	}
	io->h = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
	if (io->h == INVALID_HANDLE_VALUE) { fatalWin("couldn't open '%s'", port); }

	DCB dcb;
	memset(&dcb, 0, sizeof(dcb));
	dcb.DCBlength = sizeof(dcb);
	if (!GetCommState(io->h, &dcb)) { fatalWin("could not get comm state for '%s'", port); }
	dcb.BaudRate = CBR_57600;
	dcb.ByteSize = 8;
	dcb.Parity = NOPARITY;
	dcb.StopBits = ONESTOPBIT;
	dcb.fBinary = TRUE;
	dcb.fParity = FALSE;
	dcb.fOutxCtsFlow = FALSE;
	dcb.fOutxDsrFlow = FALSE;
	dcb.fDtrControl = DTR_CONTROL_ENABLE;
	dcb.fDsrSensitivity = FALSE;
	dcb.fOutX = FALSE;
	dcb.fInX = FALSE;
	dcb.fNull = FALSE;
	dcb.fRtsControl = RTS_CONTROL_ENABLE;
	dcb.fAbortOnError = FALSE;
	if (!SetCommState(io->h, &dcb)) {
		fatalWin("could not configure communication parameters for '%s'", port);
	}

	// A read returns after 1 byte arrives, or gives up (EOF) after 10 seconds.
	COMMTIMEOUTS to;
	memset(&to, 0, sizeof(to));
	to.ReadTotalTimeoutConstant = 10000;
	to.WriteTotalTimeoutConstant = 10000;
	if (!SetCommTimeouts(io->h, &to)) { fatalWin("could not set timeouts for '%s'", port); }
#else
	io->fd = open(port, O_RDWR);
	if (io->fd < 0) { fatal("couldn't open '%s'", port); }
	io->fh = fdopen(io->fd, "w+");
	if (!io->fh) { fatal("fdopen failed"); }
	if (set_interface_attribs(io->fd, B57600) != 0) {
		fatal("could not configure communication parameters for '%s'", port);
	}
#endif
	return io;
}

//
// Read a single byte from an IO channel; returns EOF on end of file,
// error, or (Windows serial port) timeout.
//
int io_getc(struct IO *io) {
#ifdef _WIN32
	if (io->h != INVALID_HANDLE_VALUE) {
		unsigned char c;
		DWORD n = 0;
		if (!ReadFile(io->h, &c, 1, &n, NULL) || n == 0) { return EOF; }
		return c;
	}
#endif
	return fgetc(io->fh);
}

//
// Get the size of a file.
//
off_t io_getSize(struct IO *io) {
	long size;
	if (fseek(io->fh, 0, SEEK_END) != 0 || (size = ftell(io->fh)) < 0 || fseek(io->fh, 0, SEEK_SET) != 0) {
		fatal("couldn't get size of file '%s'", io->fileName);
	}
	return (off_t) size;
}

//
// Read data from a file or IO device: fatal error
// if an I/O error occurs or not enough data can be read.
//
void io_read(struct IO *io, uint8_t *buf, off_t size) {
	size_t rc = fread(buf, 1, size, io->fh);
	if (rc != size) { fatal("failed to read %ld bytes", (long)size); }
}

//
// Scan until the prompt "> " is seen.
//
void io_scanUntilPrompt(struct IO *io) {
	int last = -1;
	for (;;) {
		int c = io_getc(io);
		if (c == EOF) { fatal("EOF before prompt was seen"); }
		if (c == ' ' && last == '>') {
			// Saw the prompt!
			return;
		}
		last = c;
	}
}

//
// Read a line of text from IO channel.
// Returns a malloc'ed buffer.
//
char *io_readLine(struct IO *io) {
	// The longest line would be the result of an R command
	// with a count of 255, which would be 512 bytes (255
	// data bytes at 2 hex digits per byte, and a \r\n
	// line terminator.)  We'll read up to 1000 bytes,
	// ignoring anything after 1000.
	char *buf = xmalloc(1024);
	size_t pos = 0;
	for (;;) {
		int c = io_getc(io);
		if (c == EOF) {
			break;
		}
		if (pos < 1000) {
			buf[pos++] = (char) c;
		}
		if (c == '\n') {
			break;
		}
	}
	buf[pos] = '\0';
	if (pos >= 2 && buf[pos-2] == '\r' && buf[pos-1] == '\n') {
		buf[pos-2] = '\0';
	} else if (pos >= 1 && buf[pos-1] == '\n') {
		buf[pos-1] = '\0';
	}
	return buf;
}

//
// Expect the "OK" response indicating the successful execution
// of a command.
//
void io_expectOk(struct IO *io) {
	char *resp = io_readLine(io);
	if (strcmp(resp, "OK") != 0) { illegal_state("did not see OK response"); }
	free(resp);
}

//
// Write data to specified IO.
//
void io_write(struct IO *io, const void *buf, size_t n) {
#ifdef _WIN32
	if (io->h != INVALID_HANDLE_VALUE) {
		DWORD written = 0;
		if (!WriteFile(io->h, buf, (DWORD) n, &written, NULL) || written != n) {
			illegal_state("could not write %lu bytes", (unsigned long) n);
		}
		return;
	}
#endif
	size_t rc = fwrite(buf, 1, n, io->fh);
	if (rc != n) { illegal_state("could not write %lu bytes", (unsigned long) n); }
	fflush(io->fh);
}

//
// Send a string to specified IO.
//
void io_send(struct IO *io, const char *s, ...) {
	char buf[1024];
	va_list args;

	va_start(args, s);
	int len = vsnprintf(buf, sizeof(buf), s, args);
	va_end(args);

	if (len < 0 || (size_t) len >= sizeof(buf)) { illegal_state("command too long"); }
	io_write(io, buf, (size_t) len);
}

//
// Close specified IO channel.
// Deletes the IO object.
//
void io_close(struct IO *io) {
#ifdef _WIN32
	if (io->h != INVALID_HANDLE_VALUE) { CloseHandle(io->h); }
#endif
	if (io->fh) { fclose(io->fh); }
	free(io);
}

//
// Parse id string received from firmware.
//
void parseId(char *id) {
	// (POSIX <regex.h> isn't available on Windows, so use sscanf.)
	int major, minor;
	if (sscanf(id, "eeprog %d.%d", &major, &minor) != 2) {
		illegal_state("id string has unexpected format");
	}

	// If we wanted to do something with the firmware version information,
	// this is where we'd do it.

	printf("Detected firmware version %d.%d\n", major, minor);
}

//
// Initiate communication with programmer by:
// - waiting for prompt
// - issuing '?' command
// - parsing ID string (result of '?') command
// - receiving the "OK" response from the '?' command
// If this is successful then we should be ready to communicate
// with the programmer to do actual reading or writing of data.
//
void beginComm(struct IO *comm) {
	io_scanUntilPrompt(comm);
	io_send(comm, "?\r\n");
	char *id = io_readLine(comm);
	//parseId(id);
	free(id);
	io_expectOk(comm);
}

//
// Write g_dataSize bytes of data in g_dataBuf to the EEPROM
// device, starting at address 0.
//
void writeFullData(struct IO *comm, uint16_t startAddr) {
	printf("Writing %d bytes\r\n", (int)g_dataSize);
	fflush(stdout);

	// Make sure the programmer is at the specified start address.
	io_scanUntilPrompt(comm);
	io_send(comm, "A%04x\r\n", startAddr);
	io_expectOk(comm);

	off_t remain = g_dataSize;
	int toWrite = 64 - startAddr%64;		// first write is the number of
											// bytes until the next page boundary
	int pos = 0;
	if(toWrite > remain) { 
		toWrite = (int)remain; 
	}

	while (remain > 0) {
		// Progress indication every 1K.
		if (pos % 1024 == 0) {
			printf(".");
			fflush(stdout);
		}

		// Write one page of up to 64 bytes.
		// We've already set the first write to be the number 
		// of bytes until the next page boundary,
		// all others except the last will be exactly 64 bytes, we're
		// guaranteed to always write at a page-aligned address.
		io_scanUntilPrompt(comm);
		io_send(comm, "P%02x", toWrite);
		for (int i = 0; i < toWrite; i++) {
			io_send(comm, "%02x", g_dataBuf[pos++]);
		}
		io_send(comm, "\r\n");
		io_expectOk(comm);

		remain -= toWrite;
		toWrite = (remain >= 64) ? 64 : (int)remain;
	}

	printf("done\n");
}

void readFullData(struct IO *comm) {
	printf("Reading %d bytes....", (int)g_dataReadSize);
	fflush(stdout);

	// Make sure the programmer is at address 0.
	io_scanUntilPrompt(comm);
	io_send(comm, "A0000\r\n");
	io_expectOk(comm);

	off_t remain = g_dataReadSize;

	int pos = 0;

	while (remain > 0) {
		// Progress indication every 1K.
		if (pos % 1024 == 0) {
			printf(".");
			fflush(stdout);
		}

		// Read 128 bytes at a time
		int toRead = (remain >= 128) ? 128 : (int)remain;
		io_scanUntilPrompt(comm);
		//printf("R%02x\n", toRead);
		io_send(comm, "R%02x\r\n", toRead);

		// Read data
		char *data = io_readLine(comm);
		io_expectOk(comm);

		// Parse returned hex data and store it in g_dataReadBuf
		size_t dataLen = strlen(data);
		if (dataLen != (size_t)toRead * 2) {
			printf("Received data: %s\n", data);
			illegal_state("returned data is wrong size (expected %d, received %lu)",
				toRead * 2, (unsigned long) dataLen);
		}
		for (int i = 0; i < toRead; i++) {
			int val;
			if (sscanf(data + i*2, "%02x", &val) != 1) { illegal_state("invalid data returned"); }
			g_dataReadBuf[pos++] = (uint8_t) val;
		}
		free(data);

		remain -= toRead;
	}

	printf("done\n");
}

int verifyFullData() {
	for (int i = 0; i < g_dataSize; i++) {
		if (g_dataBuf[i] != g_dataReadBuf[i]) {
			printf("Verify: incorrect data byte at address %04x (wrote %02x, read %02x)\n",
				i, g_dataBuf[i], g_dataReadBuf[i]);
			return 0;
		}
	}
	printf("Successful verification!\n");
	return 1;
}

//
// Main function.
//
int main(int argc, char **argv) {
	printf("eeprog host program version %d.%d (https://github.com/daveho/eeprog)\n",
		VER_MAJOR, VER_MINOR);

	// Parse options
	int opt;
	while ((opt = getopt(argc, argv, "ep:f:o:r:s:a:NDvh")) != -1) {
		switch (opt) {
		case 'e':
			g_erase = 1; break;
		case 'p':
			g_port = xstrdup(optarg); break;
		case 'f':
			g_fileName = xstrdup(optarg); break;
		case 'o':
			g_outputFileName = xstrdup(optarg); 	
			break;
		case 'r':
			{
				int readSize;
				if (sscanf(optarg, "%d", &readSize) != 1) {
					illegal_state("invalid data size '%s'", optarg);
				}
				g_dataReadSize = readSize;
			}
			break;
		case 'N':
			g_writeProtectEnable = 1; break;
		case 'D':
			g_writeProtectDisable = 1; break;
		case 'v':
			g_verify = 1; break;
		case 's': 
			{
				int eepromSize;
				if (sscanf(optarg, "%d", &eepromSize) != 1) {
					illegal_state("invalid data size '%s'", optarg);
				}else{
					if(eepromSize < 1 || eepromSize > 65536) {
						illegal_state("invalid data size '%s'", optarg);
					}else{
						printf("EEPROM Size %d bytes\n", eepromSize);
						g_eepromSize = eepromSize;
					}
				}
			}
			break;
		case 'a':
			{
				int startAddr;
				if (sscanf(optarg, "%d", &startAddr) != 1) {
					illegal_state("invalid start address '%s'", optarg);
				}else{
					if(startAddr < 0 || startAddr > 65535) {
						illegal_state("invalid start address '%s'", optarg);
					}else{
						printf("Start Address %d\n", startAddr);
						g_startAddr = startAddr;
					}
				}
			}
			break;
		case 'h':
		case '?':
			g_showHelp = opt; break;
		}
	}

	if (g_showHelp) {
		showHelp();
		exit(g_showHelp == '?' ? 1 : 0);
	}

	if (!g_port) {
		illegal_state("comm port must be specified");
	}

	if (!g_fileName && !g_dataReadSize && !g_erase) {
		illegal_state("either -f, -e, or -r must be specified");
	}
	if(!g_outputFileName && g_dataReadSize > 0){
		g_outputFileName = "output.bin";
		printf("Using default output file name: %s\n", g_outputFileName);
	}
	if (g_outputFileName && g_dataReadSize < 0) {
		illegal_state("-r must be specified to specify read size");
	}

	struct IO *comm = io_openComm(g_port);

	// Initiate communication with programmer
	beginComm(comm);

	if(g_erase) {
		printf("Erasing EEPROM...");
		g_dataSize = g_eepromSize; //EEPROM size 
		memset(g_dataBuf, 0xFF, sizeof(g_dataBuf));
		writeFullData(comm, 0);
		printf("Done\n");
	}

	if (g_fileName) {
		struct IO *dataIn = NULL;
		dataIn = io_openFile(g_fileName, "rb");
		g_dataSize = io_getSize(dataIn);
		if ((g_dataSize+g_startAddr) > g_eepromSize) { illegal_state("Size of file '%s' starting at %d exceeds %d", g_fileName, g_startAddr, g_eepromSize); }
		io_read(dataIn, g_dataBuf, g_dataSize);
		io_close(dataIn);
	}

	if (g_writeProtectDisable) {
		printf("Disabling write protection...\n");
		io_scanUntilPrompt(comm);
		io_send(comm, "D\r\n");
		io_expectOk(comm);
	}

	if (g_fileName) {
		writeFullData(comm, g_startAddr);
	}

	if (g_writeProtectEnable) {
		printf("Enabling write protection...\n");
		io_scanUntilPrompt(comm);
		io_send(comm, "N\r\n");
		io_expectOk(comm);
	}

	if (g_verify || g_outputFileName) {
		if (g_verify) {
			// When verifying, the idea is to read back all of the
			// data that was written and confirm it's the same.
			g_dataReadSize = g_dataSize;
		}
		readFullData(comm);
	}

	if (g_verify) {
		if (!verifyFullData()) {
			printf("Verification failed!\n");
			exit(1);
		}
	}

	if (g_outputFileName) {
		printf("Writing read data to '%s'\n", g_outputFileName);
		struct IO *dataOut = io_openFile(g_outputFileName, "wb");
		io_write(dataOut, g_dataReadBuf, g_dataReadSize);
		io_close(dataOut);
	}

	io_close(comm);

	printf("Done!\n");

	return 0;
}

// vim:ts=2: