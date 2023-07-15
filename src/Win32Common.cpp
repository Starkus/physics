HANDLE g_hStdout;

typedef HANDLE FileHandle;

typedef HANDLE Win32FileHandle;

void Log(const char *format, ...)
{
	char buffer[2048];
	va_list args;
	va_start(args, format);

	StringCbVPrintfA(buffer, sizeof(buffer), format, args);
	OutputDebugStringA(buffer);

	// Stdout
	DWORD bytesWritten;
	WriteFile(g_hStdout, buffer, (DWORD)strlen(buffer), &bytesWritten, nullptr);

#if USING_IMGUI
	// Imgui console
	g_imguiLogBuffer->appendfv(format, args);
#endif

	va_end(args);
}

inline bool Win32FileExists(const char *filename)
{
	DWORD attrib = GetFileAttributes(filename);
	return attrib != INVALID_FILE_ATTRIBUTES && attrib != FILE_ATTRIBUTE_DIRECTORY;
}

inline bool Win32GetLastWriteTime(const char *filename, FILETIME *lastWriteTime)
{
	WIN32_FILE_ATTRIBUTE_DATA data;
	const bool success = GetFileAttributesEx(filename, GetFileExInfoStandard, &data);
	if (success)
	{
		*lastWriteTime = data.ftLastWriteTime;
	}

	return success;
}

inline FILETIME Win32GetLastWriteTime(const char *filename)
{
	FILETIME lastWriteTime = {};

	WIN32_FILE_ATTRIBUTE_DATA data;
	if (GetFileAttributesEx(filename, GetFileExInfoStandard, &data))
	{
		lastWriteTime = data.ftLastWriteTime;
	}

	return lastWriteTime;
}

DWORD Win32ReadEntireFile(const char *filename, u8 **fileBuffer, DWORD *fileSize,
		void *(*allocFunc)(u64,int))
{
	HANDLE file = CreateFileA(
			filename,
			GENERIC_READ,
			FILE_SHARE_READ,
			nullptr,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL,
			nullptr
			);
	DWORD error = GetLastError();
	//ASSERT(file != INVALID_HANDLE_VALUE);

	if (file == INVALID_HANDLE_VALUE)
	{
		*fileBuffer = nullptr;
	}
	else
	{
		*fileSize = GetFileSize(file, nullptr);
		ASSERT(*fileSize);
		error = GetLastError();

		*fileBuffer = (u8 *)allocFunc(*fileSize, 1);
		DWORD bytesRead;
		bool success = ReadFile(
				file,
				*fileBuffer,
				*fileSize,
				&bytesRead,
				nullptr
				);
		ASSERT(success);
		ASSERT(bytesRead == *fileSize);

		CloseHandle(file);
	}

	return error;
}

bool PlatformFileExists(const char *filename)
{
	return Win32FileExists(filename);
}

bool PlatformReadEntireFile(const char *filename, u8 **fileBuffer, u64 *fileSize,
		void *(*allocFunc)(u64, int))
{
	char fullname[MAX_PATH];
	DWORD written = GetCurrentDirectory(MAX_PATH, fullname);
	fullname[written++] = '/';
	strcpy(fullname + written, filename);

	DWORD sizeDWord;
	DWORD error = Win32ReadEntireFile(filename, fileBuffer, &sizeDWord, allocFunc);
	ASSERT(error == ERROR_SUCCESS);
	*fileSize = (u64)sizeDWord;

	return error == ERROR_SUCCESS;
}

FileHandle PlatformOpenForWrite(const char *filename)
{
	FileHandle file = CreateFileA(
			filename,
			GENERIC_WRITE,
			0, // Share
			nullptr,
			CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL,
			nullptr
			);
	ASSERT(file != INVALID_HANDLE_VALUE);

	return file;
}

void PlatformCloseFile(FileHandle file)
{
	CloseHandle(file);
}

u64 PlatformWriteToFile(FileHandle file, const void *buffer, u64 size)
{
	DWORD writtenBytes;
	WriteFile(
			file,
			buffer,
			(DWORD)size,
			&writtenBytes,
			nullptr
			);
	ASSERT(writtenBytes == size);

	return (u64)writtenBytes;
}

u64 PlatformFileSeek(FileHandle file, s64 shift, int mode)
{
	LARGE_INTEGER lInt;
	LARGE_INTEGER lIntRes;
	lInt.QuadPart = shift;

	SetFilePointerEx(file, lInt, &lIntRes, mode);

	return lIntRes.QuadPart;
}

bool PlatformIsDirectory(const char *filename)
{
	return GetFileAttributesA(filename) & FILE_ATTRIBUTE_DIRECTORY;
}

bool PlatformCanReadMemory(const void *ptr)
{
	MEMORY_BASIC_INFORMATION info;
	if (VirtualQuery(ptr, &info, sizeof(info)) == 0)
		return false;

	if (info.State != MEM_COMMIT)
		return false;

	if (info.Protect == PAGE_NOACCESS || info.Protect == PAGE_EXECUTE)
		return false;

	return true;
}
