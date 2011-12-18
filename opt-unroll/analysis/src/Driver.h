#ifndef _DRIVER_H_INCLUDED_
#define _DRIVER_H_INCLUDED_

#include "Utils.h"
#include "Kernel.h"
#include "Reader.h"
#include "Parser.h"

// This is the driver program that is responsible for creating
// the appropriate high-level structures and starting off the
// parsing of the ptx file and subsequent analysis

class Driver
{
	public:
	Driver(int, char **) throw (IOException);
	~Driver();
	void PrintUsage() const;
	void Execute() throw();

	private:
	Kernel *kernel;
	Reader *reader;
	Parser *parser;

	// command line options
	union {
		struct {
			unsigned counts:1;
			unsigned ratios:1;
			unsigned cycles:1;
			unsigned loopinfo:1;
			unsigned loopcounts:1;
			unsigned loopratios:1;
			unsigned loopcycles:1;
			unsigned dumpbb:1;
			unsigned dumpcfg:1;
			unsigned dumpinst:1;
			unsigned dotcfg:1;
			unsigned unrolled:1;
			unsigned reserved:21;
		};
		unsigned int options; /* Support for 32 options, enough for now */
	};
	unsigned short nwarps;
	unsigned nthreads;
};

#endif
