#ifndef _READER_H_INCLUDED_
#define _READER_H_INCLUDED_

#include <fstream>
#include <iostream>
#include <string>
using namespace std;

#include "Utils.h"

// A helper class for taking care of file I/O. This class takes
// care of opening the ptx file and supplying lines to the parser
// when requested
class Reader
{
	// Since I don't expect any other kind of reader, 
	// I'm specializing Reader to be FileReader instead of subclassing.
	public:
	Reader(string fn) throw (IOException);
	Reader(const Reader& r);
	~Reader();
	bool NextLine(string&);
	unsigned GetLineNum() const {return linenum;}

	static const short MAX_BUFFER_LENGTH = 256;

	private:
	string filename;
	ifstream *input;
	unsigned linenum;
};

#endif
