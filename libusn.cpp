
#include <Windows.h>

#include <stdio.h>

#include <string>

#include "libusn.h"
#define BUFFER_SIZE (1024 * 1024)

using namespace std;

HANDLE hDrive;
USN maxusn;

void show_record(USN_RECORD * record)
{
	void * buffer;
	MFT_ENUM_DATA mft_enum_data;
	DWORD bytecount = 1;
	USN_RECORD * parent_record;

	WCHAR * filename;
	WCHAR * filenameend;

	printf("=================================================================\n");
	printf("RecordLength: %u\n", record->RecordLength);
	printf("MajorVersion: %u\n", (DWORD)record->MajorVersion);
	printf("MinorVersion: %u\n", (DWORD)record->MinorVersion);
	printf("FileReferenceNumber: %lu\n", record->FileReferenceNumber);
	printf("ParentFRN: %lu\n", record->ParentFileReferenceNumber);
	printf("USN: %lu\n", record->Usn);
	printf("Timestamp: %lu\n", record->TimeStamp);
	printf("Reason: %u\n", record->Reason);
	printf("SourceInfo: %u\n", record->SourceInfo);
	printf("SecurityId: %u\n", record->SecurityId);
	printf("FileAttributes: %x\n", record->FileAttributes);
	printf("FileNameLength: %u\n", (DWORD)record->FileNameLength);

	filename = (WCHAR *)(((BYTE *)record) + record->FileNameOffset);
	filenameend = (WCHAR *)(((BYTE *)record) + record->FileNameOffset + record->FileNameLength);

	printf("FileName: %.*ls\n", filenameend - filename, filename);

	buffer = VirtualAlloc(NULL, BUFFER_SIZE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

	if (buffer == NULL)
	{
		printf("VirtualAlloc: %u\n", GetLastError());
		return;
	}

	mft_enum_data.StartFileReferenceNumber = record->ParentFileReferenceNumber;
	mft_enum_data.LowUsn = 0;
	mft_enum_data.HighUsn = maxusn;

	if (!DeviceIoControl(hDrive, FSCTL_ENUM_USN_DATA, &mft_enum_data, sizeof(mft_enum_data), buffer, BUFFER_SIZE, &bytecount, NULL))
	{
		printf("FSCTL_ENUM_USN_DATA (show_record): %u\n", GetLastError());
		return;
	}

	parent_record = (USN_RECORD *)((USN *)buffer + 1);

	if (parent_record->FileReferenceNumber != record->ParentFileReferenceNumber)
	{
		printf("=================================================================\n");
		printf("Couldn't retrieve FileReferenceNumber %u\n", record->ParentFileReferenceNumber);
		return;
	}

	show_record(parent_record);
}

void check_record(USN_RECORD * record)
{
	WCHAR * filename;
	WCHAR * filenameend;

	filename = (WCHAR *)(((BYTE *)record) + record->FileNameOffset);
	filenameend = (WCHAR *)(((BYTE *)record) + record->FileNameOffset + record->FileNameLength);

	if (filenameend - filename != 8) return;

	if (wcsncmp(filename, L"test.txt", 8) != 0) return;

	show_record(record);
}

bool hasAnyChange(const wchar_t* pDirectory)
{
	if (pDirectory == nullptr)
		return false;
	MFT_ENUM_DATA mft_enum_data;
	DWORD bytecount = 1;
	void * buffer;
	USN_RECORD * record;
	USN_RECORD * recordend;
	USN_JOURNAL_DATA * journal;
	DWORDLONG nextid = 0;
	DWORDLONG filecount = 0;
	DWORD starttick, endtick;

	starttick = GetTickCount();

	printf("Allocating memory.\n");

	buffer = VirtualAlloc(NULL, 
		BUFFER_SIZE, 
		MEM_RESERVE | MEM_COMMIT,
		PAGE_READWRITE);

	if (buffer == NULL)
	{
		printf("VirtualAlloc: %u\n", GetLastError());
		return 0;
	}

	printf("Opening volume.\n");
	wstring strDrive;
	if (wcslen(pDirectory) > 4 &&
		pDirectory[0] == L'\\' && pDirectory[1] == L'\\' && pDirectory[2] == L'?' && pDirectory[3] == L'\\')
	{
		strDrive = pDirectory;
	}
	else
	{
		strDrive = L"\\\\.\\";
		strDrive += pDirectory;
	}
	hDrive = CreateFile(strDrive.c_str(),
		GENERIC_READ, 
		//FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, 
		FILE_SHARE_READ | FILE_SHARE_WRITE, 
		NULL,
		OPEN_ALWAYS, 
		FILE_FLAG_NO_BUFFERING,
		NULL);

	if (hDrive == INVALID_HANDLE_VALUE)
	{
		printf("CreateFile: %u\n", GetLastError());
		return 0;
	}

	printf("Calling FSCTL_QUERY_USN_JOURNAL\n");

	if (!DeviceIoControl(hDrive, 
		FSCTL_QUERY_USN_JOURNAL, 
		NULL, 
		0,
		buffer, 
		BUFFER_SIZE, 
		&bytecount, 
		NULL))
	{
		printf("FSCTL_QUERY_USN_JOURNAL: %u\n", GetLastError());
		return 0;
	}

	journal = (USN_JOURNAL_DATA *)buffer;

	printf("UsnJournalID: %lu\n", journal->UsnJournalID);
	printf("FirstUsn: %lu\n", journal->FirstUsn);
	printf("NextUsn: %lu\n", journal->NextUsn);
	printf("LowestValidUsn: %lu\n", journal->LowestValidUsn);
	printf("MaxUsn: %lu\n", journal->MaxUsn);
	printf("MaximumSize: %lu\n", journal->MaximumSize);
	printf("AllocationDelta: %lu\n", journal->AllocationDelta);

	maxusn = journal->MaxUsn;

	mft_enum_data.StartFileReferenceNumber = 0;
	mft_enum_data.LowUsn = 0;
	mft_enum_data.HighUsn = maxusn;

	for (;;)
	{
		//      printf("=================================================================\n");
		//      printf("Calling FSCTL_ENUM_USN_DATA\n");

		if (!DeviceIoControl(hDrive, 
			FSCTL_ENUM_USN_DATA,
			&mft_enum_data,
			sizeof(mft_enum_data),
			buffer,
			BUFFER_SIZE,
			&bytecount,
			NULL))
		{
			printf("=================================================================\n");
			printf("FSCTL_ENUM_USN_DATA: %u\n", GetLastError());
			printf("Final ID: %lu\n", nextid);
			printf("File count: %lu\n", filecount);
			endtick = GetTickCount();
			printf("Ticks: %u\n", endtick - starttick);
			return 0;
		}

		//      printf("Bytes returned: %u\n", bytecount);

		nextid = *((DWORDLONG *)buffer);
		//      printf("Next ID: %lu\n", nextid);

		record = (USN_RECORD *)((USN *)buffer + 1);
		recordend = (USN_RECORD *)(((BYTE *)buffer) + bytecount);

		while (record < recordend)
		{
			filecount++;
			check_record(record);
			record = (USN_RECORD *)(((BYTE *)record) + record->RecordLength);
		}

		mft_enum_data.StartFileReferenceNumber = nextid;
	}
}