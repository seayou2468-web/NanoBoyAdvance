// Copyright (c) 2012- PPSSPP Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0 or later versions.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official git repository and contact information can be found at
// https://github.com/hrydgard/ppsspp and http://www.ppsspp.org/.

#include "../../common/chunk_file.h"
#include "../../common/file_util.h"
#include "../../common/utf8.h"

#include "directory_file_system.h"

#include <cctype>
#include <chrono>
#include <filesystem>
#include <system_error>

namespace fs = std::filesystem;

namespace {

std::time_t FileTimeToTimeT(const fs::file_time_type& file_time) {
	using namespace std::chrono;
	const auto as_system_clock = time_point_cast<system_clock::duration>(
		file_time - fs::file_time_type::clock::now() + system_clock::now());
	return system_clock::to_time_t(as_system_clock);
}

std::string ToLowerASCII(std::string value) {
	for (char& c : value) {
		c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	}
	return value;
}

} // namespace

#if HOST_IS_CASE_SENSITIVE
static bool FixFilenameCase(const std::string &path, std::string &filename)
{
	// Are we lucky?
	if (File::Exists(path + filename))
		return true;

	size_t filenameSize = filename.size();  // size in bytes, not characters
	const std::string lowered_filename = ToLowerASCII(filename);

	//TODO: lookup filename in cache for "path"

	bool retValue = false;
	std::error_code ec;
	for (const auto& entry : fs::directory_iterator(path, ec)) {
		if (ec) {
			break;
		}
		const std::string candidate = entry.path().filename().string();
		if (candidate.size() != filenameSize) {
			continue;
		}
		if (ToLowerASCII(candidate) != lowered_filename) {
			continue;
		}
		filename = candidate;
		retValue = true;
		break;
	}

	return retValue;
}

bool FixPathCase(std::string& basePath, std::string &path, FixPathCaseBehavior behavior)
{
	size_t len = path.size();

	if (len == 0)
		return true;

	if (path[len - 1] == '/')
	{
		len--;

		if (len == 0)
			return true;
	}

	std::string fullPath;
	fullPath.reserve(basePath.size() + len + 1);
	fullPath.append(basePath); 

	size_t start = 0;
	while (start < len)
	{
		size_t i = path.find('/', start);
		if (i == std::string::npos)
			i = len;

		if (i > start)
		{
			std::string component = path.substr(start, i - start);

			// Fix case and stop on nonexistant path component
			if (FixFilenameCase(fullPath, component) == false) {
				// Still counts as success if partial matches allowed or if this
				// is the last component and only the ones before it are required
				return (behavior == FPC_PARTIAL_ALLOWED || (behavior == FPC_PATH_MUST_EXIST && i >= len));
			}

			path.replace(start, i - start, component);

			fullPath.append(component);
			fullPath.append(1, '/');
		}

		start = i + 1;
	}

	return true;
}

#endif

std::string DirectoryFileHandle::GetLocalPath(std::string& basePath, std::string localpath)
{
	if (localpath.empty())
		return basePath;

	if (localpath[0] == '/')
		localpath.erase(0,1);
	return basePath + localpath;
}

bool DirectoryFileHandle::Open(std::string& basePath, std::string& fileName, FileAccess access)
{
#if HOST_IS_CASE_SENSITIVE
	if (access & (FILEACCESS_APPEND|FILEACCESS_CREATE|FILEACCESS_WRITE))
	{
		DEBUG_LOG(FILESYS, "Checking case for path %s", fileName.c_str());
		if ( ! FixPathCase(basePath, fileName, FPC_PATH_MUST_EXIST) )
			return false;  // or go on and attempt (for a better error code than just 0?)
	}
	// else we try fopen first (in case we're lucky) before simulating case insensitivity
#endif

	std::string fullName = GetLocalPath(basePath,fileName);
	INFO_LOG(FILESYS,"Actually opening %s", fullName.c_str());

	//TODO: tests, should append seek to end of file? seeking in a file opened for append?
	// Convert flags in access parameter to fopen access mode
	const char *mode = NULL;
	if (access & FILEACCESS_APPEND) {
		if (access & FILEACCESS_READ)
			mode = "ab+";  // append+read, create if needed
		else
			mode = "ab";  // append only, create if needed
	} else if (access & FILEACCESS_WRITE) {
		if (access & FILEACCESS_READ) {
			// FILEACCESS_CREATE is ignored for read only, write only, and append
			// because C++ standard fopen's nonexistant file creation can only be
			// customized for files opened read+write
			if (access & FILEACCESS_CREATE)
				mode = "wb+";  // read+write, create if needed
			else
				mode = "rb+";  // read+write, but don't create
		} else {
			mode = "wb";  // write only, create if needed
		}
	} else {  // neither write nor append, so default to read only
		mode = "rb";  // read only, don't create
	}

	hFile = fopen(fullName.c_str(), mode);
	bool success = hFile != 0;

#if HOST_IS_CASE_SENSITIVE
	if (!success &&
	    !(access & FILEACCESS_APPEND) &&
	    !(access & FILEACCESS_CREATE) &&
	    !(access & FILEACCESS_WRITE))
	{
		if ( ! FixPathCase(basePath,fileName, FPC_PATH_MUST_EXIST) )
			return 0;  // or go on and attempt (for a better error code than just 0?)
		fullName = GetLocalPath(basePath,fileName); 
		const char* fullNameC = fullName.c_str();

		DEBUG_LOG(FILESYS, "Case may have been incorrect, second try opening %s (%s)", fullNameC, fileName.c_str());

		// And try again with the correct case this time
		hFile = fopen(fullNameC, mode);
		success = hFile != 0;
	}
#endif

	return success;
}

size_t DirectoryFileHandle::Read(u8* pointer, s64 size)
{
	return fread(pointer, 1, size, hFile);
}

size_t DirectoryFileHandle::Write(const u8* pointer, s64 size)
{
	return fwrite(pointer, 1, size, hFile);
}

size_t DirectoryFileHandle::Seek(s32 position, FileMove type)
{
	int moveMethod = 0;
	switch (type) {
	case FILEMOVE_BEGIN:    moveMethod = SEEK_SET;  break;
	case FILEMOVE_CURRENT:  moveMethod = SEEK_CUR;  break;
	case FILEMOVE_END:      moveMethod = SEEK_END;  break;
	}
	fseek(hFile, position, moveMethod);
	return ftell(hFile);
}

void DirectoryFileHandle::Close()
{
	if (hFile != 0)
		fclose(hFile);
}

DirectoryFileSystem::DirectoryFileSystem(IHandleAllocator *_hAlloc, std::string _basePath) : basePath(_basePath) {
	File::CreateFullPath(basePath);
	hAlloc = _hAlloc;
}

DirectoryFileSystem::~DirectoryFileSystem() {
	for (auto iter = entries.begin(); iter != entries.end(); ++iter) {
		iter->second.hFile.Close();
	}
}

std::string DirectoryFileSystem::GetLocalPath(std::string localpath) {
	if (localpath.empty())
		return basePath;

	if (localpath[0] == '/')
		localpath.erase(0,1);
	return basePath + localpath;
}

bool DirectoryFileSystem::MkDir(const std::string &dirname) {
#if HOST_IS_CASE_SENSITIVE
	// Must fix case BEFORE attempting, because MkDir would create
	// duplicate (different case) directories

	std::string fixedCase = dirname;
	if ( ! FixPathCase(basePath,fixedCase, FPC_PARTIAL_ALLOWED) )
		return false;

	return File::CreateFullPath(GetLocalPath(fixedCase));
#else
	return File::CreateFullPath(GetLocalPath(dirname));
#endif
}

bool DirectoryFileSystem::RmDir(const std::string &dirname) {
	std::string fullName = GetLocalPath(dirname);

#if HOST_IS_CASE_SENSITIVE
	// Maybe we're lucky?
	if (File::DeleteDirRecursively(fullName))
		return true;

	// Nope, fix case and try again
	fullName = dirname;
	if ( ! FixPathCase(basePath,fullName, FPC_FILE_MUST_EXIST) )
		return false;  // or go on and attempt (for a better error code than just false?)

	fullName = GetLocalPath(fullName);
#endif

	return File::DeleteDirRecursively(fullName);
}

int DirectoryFileSystem::RenameFile(const std::string &from, const std::string &to) {
	std::string fullTo = to;

	// Rename ignores the path (even if specified) on to.
	size_t chop_at = to.find_last_of('/');
	if (chop_at != to.npos)
		fullTo = to.substr(chop_at + 1);

	// Now put it in the same directory as from.
	size_t dirname_end = from.find_last_of('/');
	if (dirname_end != from.npos)
		fullTo = from.substr(0, dirname_end + 1) + fullTo;

	// At this point, we should check if the paths match and give an already exists error.
	if (from == fullTo)
		return -1;//SCE_KERNEL_ERROR_ERRNO_FILE_ALREADY_EXISTS;

	std::string fullFrom = GetLocalPath(from);

#if HOST_IS_CASE_SENSITIVE
	// In case TO should overwrite a file with different case
	if ( ! FixPathCase(basePath,fullTo, FPC_PATH_MUST_EXIST) )
		return -1;  // or go on and attempt (for a better error code than just false?)
#endif

	fullTo = GetLocalPath(fullTo);

	std::error_code ec;
	fs::rename(fullFrom, fullTo, ec);
	bool retValue = !ec;

#if HOST_IS_CASE_SENSITIVE
	if (! retValue)
	{
		// May have failed due to case sensitivity on FROM, so try again
		fullFrom = from;
		if ( ! FixPathCase(basePath,fullFrom, FPC_FILE_MUST_EXIST) )
			return -1;  // or go on and attempt (for a better error code than just false?)
		fullFrom = GetLocalPath(fullFrom);

		ec.clear();
		fs::rename(fullFrom, fullTo, ec);
		retValue = !ec;
	}
#endif

	// TODO: Better error codes.
	return retValue ? 0 : -1;//SCE_KERNEL_ERROR_ERRNO_FILE_ALREADY_EXISTS;
}

bool DirectoryFileSystem::RemoveFile(const std::string &filename) {
	std::string fullName = GetLocalPath(filename);
	std::error_code ec;
	const bool removed = fs::remove(fullName, ec);
	bool retValue = removed && !ec;

#if HOST_IS_CASE_SENSITIVE
	if (! retValue)
	{
		// May have failed due to case sensitivity, so try again
		fullName = filename;
		if ( ! FixPathCase(basePath,fullName, FPC_FILE_MUST_EXIST) )
			return false;  // or go on and attempt (for a better error code than just false?)
		fullName = GetLocalPath(fullName);

		ec.clear();
		retValue = fs::remove(fullName, ec) && !ec;
	}
#endif

	return retValue;
}

u32 DirectoryFileSystem::OpenFile(std::string filename, FileAccess access, const char *devicename) {
	OpenFileEntry entry;
	bool success = entry.hFile.Open(basePath,filename,access);

	if (!success) {
		ERROR_LOG(FILESYS, "DirectoryFileSystem::OpenFile: FAILED, access = %i", (int)access);
		//wwwwaaaaahh!!
		return 0;
	} else {
		u32 newHandle = hAlloc->GetNewHandle();
		entries[newHandle] = entry;

		return newHandle;
	}
}

void DirectoryFileSystem::CloseFile(u32 handle) {
	EntryMap::iterator iter = entries.find(handle);
	if (iter != entries.end()) {
		hAlloc->FreeHandle(handle);
		iter->second.hFile.Close();
		entries.erase(iter);
	} else {
		//This shouldn't happen...
		ERROR_LOG(FILESYS,"Cannot close file that hasn't been opened: %08x", handle);
	}
}

bool DirectoryFileSystem::OwnsHandle(u32 handle) {
	EntryMap::iterator iter = entries.find(handle);
	return (iter != entries.end());
}

size_t DirectoryFileSystem::ReadFile(u32 handle, u8 *pointer, s64 size) {
	EntryMap::iterator iter = entries.find(handle);
	if (iter != entries.end())
	{
		size_t bytesRead = iter->second.hFile.Read(pointer,size);
		return bytesRead;
	} else {
		//This shouldn't happen...
		ERROR_LOG(FILESYS,"Cannot read file that hasn't been opened: %08x", handle);
		return 0;
	}
}

size_t DirectoryFileSystem::WriteFile(u32 handle, const u8 *pointer, s64 size) {
	EntryMap::iterator iter = entries.find(handle);
	if (iter != entries.end())
	{
		size_t bytesWritten = iter->second.hFile.Write(pointer,size);
		return bytesWritten;
	} else {
		//This shouldn't happen...
		ERROR_LOG(FILESYS,"Cannot write to file that hasn't been opened: %08x", handle);
		return 0;
	}
}

size_t DirectoryFileSystem::SeekFile(u32 handle, s32 position, FileMove type) {
	EntryMap::iterator iter = entries.find(handle);
	if (iter != entries.end()) {
		return iter->second.hFile.Seek(position,type);
	} else {
		//This shouldn't happen...
		ERROR_LOG(FILESYS,"Cannot seek in file that hasn't been opened: %08x", handle);
		return 0;
	}
}

FileInfo DirectoryFileSystem::GetFileInfo(std::string filename) {
	FileInfo x;
	x.name = filename;

	std::string fullName = GetLocalPath(filename);
	if (! File::Exists(fullName)) {
#if HOST_IS_CASE_SENSITIVE
		if (! FixPathCase(basePath,filename, FPC_FILE_MUST_EXIST))
			return x;
		fullName = GetLocalPath(filename);

		if (! File::Exists(fullName))
			return x;
#else
		return x;
#endif
	}
	x.type = File::IsDirectory(fullName) ? FILETYPE_DIRECTORY : FILETYPE_NORMAL;
	x.exists = true;

	if (x.type != FILETYPE_DIRECTORY)
	{
		std::error_code ec;
		x.size = fs::file_size(fullName, ec);
		if (ec) {
			x.size = 0;
		}

		const fs::file_status status = fs::status(fullName, ec);
		x.access = ec ? 0 : (static_cast<u32>(status.permissions()) & 0x1FF);

		const auto write_time = fs::last_write_time(fullName, ec);
		if (!ec) {
			const std::time_t time_value = FileTimeToTimeT(write_time);
			localtime_r(&time_value, &x.atime);
			localtime_r(&time_value, &x.ctime);
			localtime_r(&time_value, &x.mtime);
		}
	}

	return x;
}

bool DirectoryFileSystem::GetHostPath(const std::string &inpath, std::string &outpath) {
	outpath = GetLocalPath(inpath);
	return true;
}

std::vector<FileInfo> DirectoryFileSystem::GetDirListing(std::string path) {
	std::vector<FileInfo> myVector;
	std::string localPath = GetLocalPath(path);
	std::error_code ec;
	fs::directory_iterator dir_it(localPath, ec);
	fs::directory_iterator end_it;

#if HOST_IS_CASE_SENSITIVE
	if (ec && FixPathCase(basePath, path, FPC_FILE_MUST_EXIST)) {
		// May have failed due to case sensitivity, try again
		localPath = GetLocalPath(path);
		ec.clear();
		dir_it = fs::directory_iterator(localPath, ec);
	}
#endif

	if (ec) {
		ERROR_LOG(FILESYS,"Error opening directory %s\n",path.c_str());
		return myVector;
	}

	for (; dir_it != end_it; dir_it.increment(ec)) {
		if (ec) {
			break;
		}
		FileInfo entry;
		const auto& fs_entry = *dir_it;
		const auto status = fs_entry.status(ec);
		if (ec) {
			ec.clear();
			continue;
		}

		entry.type = fs::is_directory(status) ? FILETYPE_DIRECTORY : FILETYPE_NORMAL;
		entry.access = static_cast<u32>(status.permissions()) & 0x1FF;
		entry.name = fs_entry.path().filename().string();
		entry.size = entry.type == FILETYPE_DIRECTORY ? 0 : fs_entry.file_size(ec);
		if (ec) {
			ec.clear();
			entry.size = 0;
		}

		const auto write_time = fs_entry.last_write_time(ec);
		if (!ec) {
			const std::time_t time_value = FileTimeToTimeT(write_time);
			localtime_r(&time_value, &entry.atime);
			localtime_r(&time_value, &entry.ctime);
			localtime_r(&time_value, &entry.mtime);
		} else {
			ec.clear();
		}
		myVector.push_back(entry);
	}
	return myVector;
}

void DirectoryFileSystem::DoState(PointerWrap &p) {
	if (!entries.empty()) {
		p.SetError(p.ERROR_WARNING);
		ERROR_LOG(FILESYS, "FIXME: Open files during savestate, could go badly.");
	}
}
