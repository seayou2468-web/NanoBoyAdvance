
#include "abstract_file.h"
#include "Std_File_Reader.h"

#include "blargg_config.h"

#include <assert.h>
#include <string.h>
#include <stdlib.h>

/* Copyright (C) 2005-2006 Shay Green. Permission is hereby granted, free of
charge, to any person obtaining a copy of this software module and associated
documentation files (the "Software"), to deal in the Software without
restriction, including without limitation the rights to use, copy, modify,
merge, publish, distribute, sublicense, and/or sell copies of the Software, and
to permit persons to whom the Software is furnished to do so, subject to the
following conditions: The above copyright notice and this permission notice
shall be included in all copies or substantial portions of the Software. THE
SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

blargg_err_t Data_Writer::write( const void*, long ) { return 0; }

void Data_Writer::satisfy_lame_linker_() { }

// Std_File_Writer

Std_File_Writer::Std_File_Writer() : file_( 0 ) {
}

Std_File_Writer::~Std_File_Writer() {
	close();
}

blargg_err_t Std_File_Writer::open( const char* path )
{
	close();
	file_ = fopen( path, "wb" );
	if ( !file_ )
		return "Couldn't open file for writing";
		
	// to do: increase file buffer size
	//setvbuf( file_, 0, _IOFBF, 32 * 1024L );
	
	return 0;
}

blargg_err_t Std_File_Writer::write( const void* p, long s )
{
	long result = (long) fwrite( p, 1, s, file_ );
	if ( result != s )
		return "Couldn't write to file";
	return 0;
}

void Std_File_Writer::close()
{
	if ( file_ ) {
		fclose( file_ );
		file_ = 0;
	}
}

// Mem_Writer

Mem_Writer::Mem_Writer( void* p, long s, int b )
{
	data_ = (char*) p;
	size_ = 0;
	allocated = s;
	mode = b ? ignore_excess : fixed;
}

Mem_Writer::Mem_Writer()
{
	data_ = 0;
	size_ = 0;
	allocated = 0;
	mode = expanding;
}

Mem_Writer::~Mem_Writer()
{
	if ( mode == expanding )
		free( data_ );
}

blargg_err_t Mem_Writer::write( const void* p, long s )
{
	long remain = allocated - size_;
	if ( s > remain )
	{
		if ( mode == fixed )
			return "Tried to write more data than expected";
		
		if ( mode == ignore_excess )
		{
			s = remain;
		}
		else // expanding
		{
			long new_allocated = size_ + s;
			new_allocated += (new_allocated >> 1) + 2048;
			void* p = realloc( data_, new_allocated );
			if ( !p )
				return "Out of memory";
			data_ = (char*) p;
			allocated = new_allocated;
		}
	}
	
	assert( size_ + s <= allocated );
	memcpy( data_ + size_, p, s );
	size_ += s;
	
	return 0;
}

// Null_Writer

blargg_err_t Null_Writer::write( const void*, long )
{
	return 0;
}

// Auto_File_Reader

using StdAutoFileWriter = Std_File_Writer;
using StdAutoFileReader = Std_File_Reader;
#ifdef HAVE_ZLIB_H
using StdAutoFileCompWriter = Gzip_File_Writer;
#else
using StdAutoFileCompWriter = Std_File_Writer;
#endif

const char* Auto_File_Reader::open()
{
	#ifdef DISABLE_AUTO_FILE
		return 0;
	#else
		if ( data )
			return 0;
		StdAutoFileReader* d = new StdAutoFileReader;
		if ( !d )
			return "Out of memory";
		data = d;
		return d->open( path );
	#endif
}

Auto_File_Reader::~Auto_File_Reader()
{
	if ( path )
		delete data;
}

// Auto_File_Writer

const char* Auto_File_Writer::open()
{
	#ifdef DISABLE_AUTO_FILE
		return 0;
	#else
		if ( data )
			return 0;
		StdAutoFileWriter* d = new StdAutoFileWriter;
		if ( !d )
			return "Out of memory";
		data = d;
		return d->open( path );
	#endif
}

const char* Auto_File_Writer::open_comp( int level )
{
	#ifdef DISABLE_AUTO_FILE
		return 0;
	#else
		if ( data )
			return 0;
		StdAutoFileCompWriter* d = new StdAutoFileCompWriter;
		if ( !d )
			return "Out of memory";
		data = d;
		return d->open( path, level );
	#endif
}

Auto_File_Writer::~Auto_File_Writer()
{
	#ifndef DISABLE_AUTO_FILE
		if ( path )
			delete data;
	#endif
}

#ifdef HAVE_ZLIB_H

#include "zlib.h"

static const char* get_gzip_eof( FILE* file, long* eof )
{
	unsigned char buf [4];
	if ( !fread( buf, 2, 1, file ) )
		return "Couldn't read from file";
	
	if ( buf [0] == 0x1F && buf [1] == 0x8B )
	{
		if ( fseek( file, -4, SEEK_END ) )
			return "Couldn't seek in file";
		
		if ( !fread( buf, 4, 1, file ) )
			return "Couldn't read from file";
		
		*eof = buf [3] * 0x1000000L + buf [2] * 0x10000L + buf [1] * 0x100L + buf [0];
	}
	else
	{
		if ( fseek( file, 0, SEEK_END ) )
			return "Couldn't seek in file";
		
		*eof = ftell( file );
	}
	
	return 0;
}

const char* get_gzip_eof( const char* path, long* eof )
{
	FILE* file = fopen( path, "rb" );
	if ( !file )
		return "Couldn't open file";
	const char* error = get_gzip_eof( file, eof );
	fclose( file );
	return error;
}

// Gzip_File_Writer

Gzip_File_Writer::Gzip_File_Writer() : file_( 0 )
{
}

Gzip_File_Writer::~Gzip_File_Writer()
{
	close();
}

Gzip_File_Writer::error_t Gzip_File_Writer::open( const char* path, int level )
{
	close();
	
	char mode [4] = { 'w', 'b', 0, 0 };
	if ( level >= 0 )
		mode [2] = level + '0';
	file_ = gzopen( path, mode );
	if ( !file_ )
		return "Couldn't open file for writing";
	
	return 0;
}

Gzip_File_Writer::error_t Gzip_File_Writer::write( const void* p, long s )
{
    long result = (long) gzwrite( (gzFile) file_ , (void*) p, s );
	if ( result != s )
		return "Couldn't write to file";
	return 0;
}

void Gzip_File_Writer::close()
{
	if ( file_ )
	{
        gzclose( (gzFile) file_ );
		file_ = 0;
	}
}

#endif
