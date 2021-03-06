
#include <Windows.h>

#include <stdio.h>

#include <string>

#include "../lsMisc/stdosd/stdosd.h"

#include "libusn.h"
#define BUFFER_SIZE (1024 * 1024)

using namespace std;
using namespace Ambiesoft::stdosd;

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
	printf("FileReferenceNumber: %llu\n", record->FileReferenceNumber);
	printf("ParentFRN: %llu\n", record->ParentFileReferenceNumber);
	printf("USN: %llu\n", record->Usn);
	printf("Timestamp: %lld\n", record->TimeStamp.QuadPart);
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
		printf("Couldn't retrieve FileReferenceNumber %llu\n", record->ParentFileReferenceNumber);
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

DWORD GetMaxComponentLength(LPCTSTR pDrive)
{
	DWORD maxCompLen = 0;
	GetVolumeInformation(
		pDrive,
		NULL,
		0,
		NULL,
		&maxCompLen,
		NULL,
		NULL,
		0
	);
	return maxCompLen;
}
bool hasAnyChange(const wchar_t* pDirectory)
{
	if (pDirectory == nullptr)
		return false;
	wstring strDrive(pDirectory);
	if (strDrive.size() < 2)
		return false;
	if (strDrive[1] != L':')
		return false;

	if(hasStarting(strDrive.c_str(), L"\\\\?\\"))
	{
		strDrive[2] = L'.';
	}

	wchar_t driveletter = 0;
	if(hasStarting(strDrive.c_str(), L"\\\\.\\"))
	{
		driveletter = strDrive[4];
	}
	else
	{
		driveletter = strDrive[0];
	}

	if (!iswalpha(driveletter))
		return false;

	wstring strDriveT;
	strDriveT += driveletter;
	strDriveT += L":";
	const DWORD maxComponentLen = GetMaxComponentLength(strDriveT.c_str());

	strDrive = wstring(L"\\\\.\\") + driveletter + L":";


	DWORD bytecount = 1;
	void * buffer;
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

	if (!DeviceIoControl(
		hDrive, 
		FSCTL_QUERY_USN_JOURNAL, 
		NULL, // not used
		0, // not used
		buffer, 
		BUFFER_SIZE, 
		&bytecount, 
		NULL))
	{
		printf("FSCTL_QUERY_USN_JOURNAL: %u\n", GetLastError());
		return 0;
	}

	journal = (USN_JOURNAL_DATA *)buffer;

	printf("UsnJournalID: %llu\n", journal->UsnJournalID);
	printf("FirstUsn: %llu\n", journal->FirstUsn);
	printf("NextUsn: %llu\n", journal->NextUsn);
	printf("LowestValidUsn: %llu\n", journal->LowestValidUsn);
	printf("MaxUsn: %llu\n", journal->MaxUsn);
	printf("MaximumSize: %llu\n", journal->MaximumSize);
	printf("AllocationDelta: %llu\n", journal->AllocationDelta);

	maxusn = journal->MaxUsn;

	MFT_ENUM_DATA med = { 0 };
	med.StartFileReferenceNumber = 0;
	med.LowUsn = 0;
	med.HighUsn = maxusn;
	med.MinMajorVersion = journal->MinSupportedMajorVersion;
	med.MaxMajorVersion = journal->MaxSupportedMajorVersion;
	for (;;)
	{
		//      printf("=================================================================\n");
		//      printf("Calling FSCTL_ENUM_USN_DATA\n");

		ZeroMemory(buffer, BUFFER_SIZE);
		if (!DeviceIoControl(
			hDrive, 
			FSCTL_ENUM_USN_DATA,
			&med,
			sizeof(med),
			buffer,
			BUFFER_SIZE,
			&bytecount,
			NULL))
		{
			DWORD dwLastError = GetLastError();
			printf("=================================================================\n");
			printf("FSCTL_ENUM_USN_DATA: %u\n", dwLastError);
			printf("Final ID: %llu\n", nextid);
			printf("File count: %llu\n", filecount);
			endtick = GetTickCount();
			printf("Ticks: %u\n", endtick - starttick);
			return 0;
		}

		printf("Bytes returned: %u\n", bytecount);
		USN_RECORD* record = (USN_RECORD*)buffer;
		DWORD MaximumChangeJournalRecordSize =
			(maxComponentLen * sizeof(WCHAR)
				+ sizeof(USN_RECORD) - sizeof(WCHAR));
		USN_RECORD* recordend = (USN_RECORD *)(((BYTE *)buffer) + MaximumChangeJournalRecordSize);

		//nextid = *((DWORDLONG *)buffer);
		nextid = record->FileReferenceNumber;

		printf("Next ID: %llu\n", nextid);

		// record = (USN_RECORD *)((USN *)buffer + 1);
		// recordend = (USN_RECORD *)(((BYTE *)buffer) + bytecount);

		
		while (record < recordend)
		{
			filecount++;
			check_record(record);
			record = (USN_RECORD *)(((BYTE *)record) + record->RecordLength);
		}

		med.StartFileReferenceNumber = nextid;
	}
}