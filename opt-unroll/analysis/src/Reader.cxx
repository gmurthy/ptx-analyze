#include "Reader.h"
#include "Utils.h"

// Given a filename, open an input file stream and initialize
Reader::Reader(string fn) throw (IOException)
{
	filename = fn;
	input = new ifstream(filename.c_str());	
	linenum = 0;
	if (input->fail()) throw IOException();
}

// copy ctor
Reader::Reader(const Reader& r)
: filename(r.filename), input(r.input), linenum(r.linenum) {}

Reader::~Reader()
{
	input->close();
	delete input;
}

// This is the meat of the Reader. Read the next line from the
// input file stream and fill it into the caller-supplied buffer
bool Reader::NextLine(string& line)
{
	char buffer[Reader::MAX_BUFFER_LENGTH];

	Assert(!(input->bad() || input->eof() || input->fail()), "Reading past EOF");

	input->getline(buffer, Reader::MAX_BUFFER_LENGTH);
	line = buffer;
	++linenum;

	// Peek ahead to check if we've another line to process
	input->peek();

	if (input->bad() || input->eof() || input->fail()) 
		return false;
	return true;
}
