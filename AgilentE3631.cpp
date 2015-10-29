#include <time.h>    // detecting timeouts
#include <windows.h> // Windows system calls
#include <iostream>  // We use cout, etc.
#include <cstdio>
#include <tchar.h>
using namespace std;

HANDLE hSerPort; // Global variables
int iSerOK;
void SerMessage(TCHAR* msg1, TCHAR* msg2)
// Displays a message to the user.
// msg1 = main message
// msg2 = message caption
// Change this to your preferred way of displaying msgs.
{
	MessageBox(NULL, msg1, msg2, MB_OK);
}
void SerCrash(TCHAR* msg1, TCHAR* msg2)
// Like SerMessage, but ends the program.
{
	SerMessage(msg1, msg2);
	iSerOK = 0;
}
int SerOK()
// Returns nonzero if most recent operation succeeded,
// or 0 if there was an error
{
	return iSerOK;
}
void SerOpen(TCHAR* portname, char* handshake)
// Opens the serial port.
// portname = "COM1", "COM2", etc.
// handshake = "RTS" or "DTR"
{
	//
	// Open the port using a handle
	//
	hSerPort = CreateFile(
		portname, GENERIC_READ | GENERIC_WRITE, 0, NULL,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hSerPort == INVALID_HANDLE_VALUE)
		SerCrash(_T("Can't open comm port"), portname);
	//
	// Set baud rate and other attributes 
	//
	DCB dcbSerPort;
	GetCommState(hSerPort, &dcbSerPort);
	dcbSerPort.BaudRate = CBR_9600;
	dcbSerPort.fBinary = TRUE;
	dcbSerPort.fParity = FALSE;
	// Both handshaking modes are sensitive to DSR and DTR...
	//dcbSerPort.fDsrSensitivity = TRUE;
	dcbSerPort.fDsrSensitivity = FALSE;
	dcbSerPort.fDtrControl = DTR_CONTROL_HANDSHAKE;
	//dcbSerPort.fDtrControl = DTR_CONTROL_DISABLE;
	dcbSerPort.fOutxDsrFlow = TRUE;

	// .. but not XON/XOFF.
	dcbSerPort.fOutX = FALSE;
	dcbSerPort.fInX = FALSE;
	// Now choose the handshaking mode...
	if ((handshake[0] == 'r') || (handshake[0] == 'R'))
	{
		dcbSerPort.fOutxCtsFlow = TRUE;
		dcbSerPort.fRtsControl = RTS_CONTROL_HANDSHAKE;
	}
	else
	{
		dcbSerPort.fOutxCtsFlow = FALSE;
		dcbSerPort.fRtsControl = RTS_CONTROL_ENABLE;
		//dcbSerPort.fRtsControl = RTS_CONTROL_DISABLE;
	}
	dcbSerPort.fNull = FALSE;
	dcbSerPort.fAbortOnError = FALSE;
	dcbSerPort.ByteSize = 8;
	dcbSerPort.Parity = NOPARITY;
	//dcbSerPort.StopBits = ONESTOPBIT;
	dcbSerPort.StopBits = TWOSTOPBITS;
	iSerOK = SetCommState(hSerPort, &dcbSerPort);
	if (!iSerOK) SerCrash(_T("Bad parameters for port"), portname);
	//
	// Disable Windows' timeouts; we keep track of our own
	//

	COMMTIMEOUTS t = { MAXDWORD, 0, 0, 0, 0 };
	SetCommTimeouts(hSerPort, &t);
}
void SerClose()
// Closes the serial port.
{
	iSerOK = CloseHandle(hSerPort);
	if (!iSerOK) SerCrash(_T("Problem closing serial port"), _T(""));
}
void SerPut(char* data)
// Transmits a line followed by CR and LF. 
// Caution! Will wait forever, without registering an error,
// if handshaking signals tell it not to transmit.
{
	DWORD nBytes;
	iSerOK = WriteFile(hSerPort, data, strlen(data), &nBytes, NULL);
	iSerOK &= WriteFile(hSerPort, "\r\n", 2, &nBytes, NULL);
	//iSerOK &= WriteFile(hSerPort,"\n",1,&nBytes,NULL);

	if (!iSerOK) SerCrash(_T("Problem writing to serial port"), _T(""));
}
void SerGetChar(char* c, int* success)
// Receives a character from the serial port,
// or times out after 10 seconds.
// success = 0 if timeout, 1 if successful
{
	time_t finish;
	finish = time(NULL) + 10;
	*success = 0;
	DWORD nBytes = 0;
	while ((*success == 0) && (time(NULL) < finish)) {
		ReadFile(hSerPort, c, 1, &nBytes, NULL);
		*success = nBytes;
	}
	if (*success == 0)
		SerCrash(_T("Timed out waiting for serial input"), _T(""));
}
void SerGet(char* buf, int bufsize)
// Inputs a line from the serial port, stopping at
// end of line, carriage return, timeout, or buffer full.
{
	int bufpos = 0;
	buf[bufpos] = 0; // initialize empty string
	int bufposmax = bufsize - 1; // maximum subscript
	char c = 0;
	int success = 0;
	//
	// Eat any Return or Line Feed characters
	// left over from end of previous line
	//
	do { SerGetChar(&c, &success); } while ((success == 1) && (c <= 31));
	//
	// We have now read the first character or timed out.
	// Read characters until any termination condition is met.
	//

	while (1) {
		if (success == 0) return; // timeout
		if (c == 13) return; // carriage return
		if (c == 10) return; // line feed
		if (bufpos >= bufposmax) return; // buffer full
		buf[bufpos] = c;
		bufpos++;
		buf[bufpos] = 0; // guarantee validly terminated string
		SerGetChar(&c, &success);
	}
}
void SerGetNum(double* x)
// Reads a floating-point number from the serial port.
{
	char buf[100];
	SerGet(buf, sizeof(buf));
	iSerOK = sscanf(buf, "%lf", x); // "%lf" = "long float" = double
	if (iSerOK < 1) {
		SerCrash(_T("Problem converting string to number"), _T(""));
		*x = 0.0;
	}
}
void clear(void)
{
	while (getchar() != '\n');
}

int main()
{
	char b[100];
	double x;
	int opt = 0, dc = 0;
	char set_volt[100] = { 0 };
	double volt = 0;

	char Menu[] = "\
				  [1]:OUTP ON;\n\
				  [2]:OUTP OFF;\n\
				  [3]:INST P6V;\n\
				  [4]:INST P25V;\n\
				  [5]:Set Voltage;\n\
				  [6]:SYST:ERR?;\n\
				  [7]:*IDN?;\n\
				  [8]:MEAS:VOLT:DC?;\n\
				  [9]:MEAS:CURR:DC?;\n\
				  [10]:READ RESPONSE(number);\n\
				  [11]:READ RESPONSE(string);\n";

	SerOpen(_T("COM1"), "DTR"); // note DTR handshaking
	SerPut("SYST:REM");
	SerPut("*RST");
	SerPut("*CLS");
	//SerPut("APPL P6V, 5.0, 3.0");
	SerPut("OUTP OFF");
	while (1)
	{
		fprintf(stdout, "%s", Menu);
		scanf("%d", &opt);
		clear();
		switch (opt)
		{
		case 1:
			SerPut("OUTP ON");
			break;
		case 2:
			SerPut("OUTP OFF");
			break;
		case 3:
			SerPut("INST P6V");
			break;
		case 4:
			SerPut("INST P25V");
			break;
		case 5:
			fprintf(stdout, "(0)P6V/(1)P25V?:");
			scanf("%d", &dc);
			clear();
			fprintf(stdout, "set Volatge to:");
			scanf("%lf", &volt);
			clear();
			if (dc)
				sprintf(set_volt, "APPL P25V, %.2f, 1.0", volt);
			else
				sprintf(set_volt, "APPL P6V, %.2f, 1.0", volt);
			SerPut(set_volt);
			break;
		case 6:
			SerPut("SYST:ERR?");
			break;
		case 7:
			SerPut("*IDN?");
			break;
		case 8:
			SerPut("SYST:REM");
			SerPut("MEAS:VOLT:DC?");
			break;
		case 9:
			SerPut("MEAS:CURR:DC?");
			break;
		case 10:
			SerGetNum(&x);
			cout << "CURR (as number) = " << x << "\n";
			break;
		case 11:
			SerGet(b, sizeof(b));
			cout << "IDN (as string) = " << b << "\n";
			break;
		default:
			cout << "wrong options!\n";
			break;
		}
	}
	SerClose();
	return 0;
}
