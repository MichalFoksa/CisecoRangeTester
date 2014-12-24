#ifndef __StreamLogger_h__
#define __StreamLogger_h__

#include <stdio.h> // for size_t 
#include <Stream.h>
#include <Arduino.h>

class StreamLogger {
	private: 
		static StreamLogger *_instance ;
		Stream *_stream ;

	private:	
		StreamLogger () : _stream( &Serial ) {}

	public:
		static	StreamLogger* getInstance() ;
		void	setStream ( Stream *stream )			{ _stream = stream ; }
		size_t	print(const __FlashStringHelper *s )	{ _stream->print( s )  ; }
		size_t	print(const String &s )					{ _stream->print( s )  ; }
		size_t	rint(const char s[] )					{ _stream->print( s )  ; }
		size_t	print(char s )							{ _stream->print( s )  ; }
		size_t	print(unsigned char s, int i = DEC)		{ _stream->print( s , i )  ; }
		size_t	print(int s, int i = DEC)				{ _stream->print( s , i )  ; }
		size_t	print(unsigned int s, int i = DEC)		{ _stream->print( s , i )  ; }
		size_t	print(long s, int i = DEC)				{ _stream->print( s , i )  ; }
		size_t	print(unsigned long s, int i = DEC)		{ _stream->print( s , i )  ; }
		size_t	print(double s, int i = 2)				{ _stream->print( s , i )  ; }
		size_t	print(const Printable &s)				{ _stream->print( s )  ; }
		
		size_t println(const __FlashStringHelper *s )	{ _stream->println( s )  ; }
		size_t println(const String &s )				{ _stream->println( s )  ; }
		size_t println(const char s[] )					{ _stream->println( s )  ; }
		size_t println(char s )							{ _stream->println( s )  ; }
		size_t println(unsigned char s, int i = DEC)	{ _stream->println( s , i )  ; }
		size_t println(int s, int i = DEC)				{ _stream->println( s , i )  ; }
		size_t println(unsigned int s, int i = DEC)		{ _stream->println( s , i )  ; }
		size_t println(long s, int i = DEC)				{ _stream->println( s , i )  ; }
		size_t println(unsigned long s, int i = DEC)	{ _stream->println( s , i )  ; }
		size_t println(double s, int i = 2)				{ _stream->println( s , i )  ; }
		size_t println(const Printable &s)				{ _stream->println( s )  ; }
		size_t println(void)							{ _stream->println()  ; }

};
#endif

