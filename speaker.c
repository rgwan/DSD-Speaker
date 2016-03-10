#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

/*
  WINIO_API bool _stdcall InitializeWinIo();
  WINIO_API void _stdcall ShutdownWinIo();
  WINIO_API PBYTE _stdcall MapPhysToLin(tagPhysStruct &PhysStruct);
  WINIO_API bool _stdcall UnmapPhysicalMemory(tagPhysStruct &PhysStruct);
  WINIO_API bool _stdcall GetPhysLong(PBYTE pbPhysAddr, PDWORD pdwPhysVal);
  WINIO_API bool _stdcall SetPhysLong(PBYTE pbPhysAddr, DWORD dwPhysVal);
  WINIO_API bool _stdcall GetPortVal(WORD wPortAddr, PDWORD pdwPortVal, BYTE bSize);
  WINIO_API bool _stdcall SetPortVal(WORD wPortAddr, DWORD dwPortVal, BYTE bSize);
  WINIO_API bool _stdcall InstallWinIoDriver(PWSTR pszWinIoDriverPath, bool IsDemandLoaded = false);
  WINIO_API bool _stdcall RemoveWinIoDriver();
*/
#define WINIO_API __attribute__((stdcall))
#define CLOCK_TICK_RATE 1193180 //计数时钟

bool WINIO_API (*InitializeWinIo)();
void WINIO_API (*ShutdownWinIo)();
bool WINIO_API (*GetPortVal)(WORD wPortAddr, uint32_t *pdwPortVal, BYTE bSize);
bool WINIO_API (*SetPortVal)(WORD wPortAddr, uint32_t dwPortVal, BYTE bSize);
bool WINIO_API (*InstallWinIoDriver)(PWSTR pszWinIoDriverPath, bool IsDemandLoaded);
bool WINIO_API (*RemoveWinIoDriver)();

HMODULE hdll;

int FreqTable[12]={
	262,277,293,311,329,349,369,392,415,440,466,493
};

int Score[]={
	60,200,
	0,1,
	60,200,
	0,1,
	67,200,
	0,1,
	67,200,
	0,1,
	69,200,
	0,1,
	69,200,
	0,1,
	67,400,
	0,1,
	65,200,
	0,1,
	65,200,
	0,1,
	64,200,
	0,1,
	64,200,
	0,1,
	62,200,
	0,1,
	62,200,
	0,1,
	60,400,
	0,400,
	
	60,600,
	62,600,
	64,600,
	
	0xff,0
};



int InitWinIO()
{
	hdll = LoadLibrary("WinIo32.dll");
	if(hdll == NULL)
	{
		fprintf(stderr, "Can't load WinIo! %d\r\n", (int)hdll);
		exit(0);
	}
	InitializeWinIo = (void *)GetProcAddress(hdll, "InitializeWinIo");
	ShutdownWinIo = (void *)GetProcAddress(hdll, "ShutdownWinIo");
	SetPortVal = (void *)GetProcAddress(hdll, "SetPortVal");
	GetPortVal = (void *)GetProcAddress(hdll, "GetPortVal");
	if(InitializeWinIo == NULL || ShutdownWinIo == NULL
		|| GetPortVal == NULL || SetPortVal == NULL)
	{
		fprintf(stderr, "Can 't get function pointer!\r\n", (int)hdll);
		exit(0);
	}
	bool ret = (*InitializeWinIo)();
	printf("InitializeWinIo %d\n", ret);
	if(!ret) exit(0);
}

int PlaySoundB(char MIDINote, int During)
{
	int Freq;
	int Count;
	int Octave;
	int value;
	printf("Note %d, Pitch %d\r\n", MIDINote, Freq);
	if(MIDINote == 0)
	{
		Sleep(During);
		return 0;
	}
	if(MIDINote >= 60)
	{
		Octave = (MIDINote - 60) / 12;
		Freq = FreqTable[(MIDINote - 60) % 12];
		if(Octave != 0)
			Freq *= 2 * Octave;
	}
	else
	{
		Octave = 0;
		while(MIDINote < 60)
		{
			Octave ++;
			MIDINote += 12;
		}
		Freq = FreqTable[(MIDINote - 60) % 12];
		Freq /= 2 * Octave;
	}
	
	(*SetPortVal)(0x43, 0xb6, 1);
	
	Count = CLOCK_TICK_RATE / Freq;
	(*SetPortVal)(0x42, Count & 0xff, 1);
	(*SetPortVal)(0x42, (Count >> 8) & 0xff, 1);
	
	(*GetPortVal)(0x61, &value, 1);
	(*SetPortVal)(0x61, value | 0x03, 1);	
	Sleep(During);
	(*GetPortVal)(0x61, &value, 1);
	(*SetPortVal)(0x61, value & 0xFC, 1);
	return 0;
}

int acc_1 = 0;//Integrator1
int acc_2 = 0;//Integrator 2
char DSD_ConvertSamples_2(short int input)
{//2 order Delta_Sigma Modulator
	int feedback;//which is 1-bit DAC
	if(input > 32767)
		input = 32767;
	if(input < -32767)
		input = -32767;
	if(acc_2 > 0)
		feedback = 32767;
	else
		feedback = -32767;
	acc_1 += input - feedback;
	acc_2 += acc_1 - feedback;
	if(acc_2 > 0)
		return 1;
	else
		return 0;	
}

int main()
{
	int i = 0;
	int value;
	int size;
	uint16_t *wavedata;
	InitWinIO();
	FILE *fp;
	fp = fopen("test.dat", "rb");
	if(fp == NULL)
	{
		printf("can't open file!\r\n");
		goto cleanup;
	}
	fseek(fp, 0, SEEK_END);
	size = ftell(fp);
	rewind(fp);
	wavedata = malloc(size);
	fread(wavedata, 1, size, fp);
	fclose(fp);
	
	printf("Read %d bytes\n", size);
	
	(*GetPortVal)(0x61, &value, 1);
	(*SetPortVal)(0x61, value & 0xFC, 1);
	
	(*SetPortVal)(0x43, 160, 1);
	(*SetPortVal)(0x42, 0, 1);
	(*SetPortVal)(0x43, 144, 1);
	(*SetPortVal)(0x42, 0, 1);
	
	(*GetPortVal)(0x61, &value, 1);
	value = (value & 12) | 1;
	while(i < (size / 2))
	{
		int j;
		for(j = 0; j < 18; j++)
			(*SetPortVal)(0x61, value | (DSD_ConvertSamples_2(wavedata[i]) << 1), 1);
		i+=1;
	}
	
cleanup:
	(*GetPortVal)(0x61, &value, 1);
	(*SetPortVal)(0x61, value & 0xFC, 1);
	free(wavedata);
	(*ShutdownWinIo)();
	FreeLibrary(hdll);
	printf("Cleaned up\r\n");
	return 0;
}
