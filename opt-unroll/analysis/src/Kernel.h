#ifndef _KERNEL_H_INCLUDED_
#define _KERNEL_H_INCLUDED_

#include "Statement.h"
#include "Parser.h"
#include "CFG.h"
#include "Device.h"

#include <list>
#include <vector>
using namespace std;

// The Kernel class is an abstraction of a GPGPU kernel. It contains
// a stream of instructions and labels, among other stuff. The driver
// creates a parser for the given ptx file and initiates the construction
// of the kernel.

class Kernel
{
	public:
	Kernel(Parser *);
	Kernel(const Kernel&);
	~Kernel();

	inline Instruction * GetFirstInst() const {return inst_stream->front();}
	inline Instruction * GetLastInst() const {return inst_stream->back();}
	InstIter InstBegin() const {return inst_stream->begin();}
	InstIter InstEnd() const {return inst_stream->end();}
	inline const unsigned GetNumWarps() const {return num_warps;}
	inline void SetNumWarps(unsigned short nwarps) {num_warps = nwarps;}
	void AddInstruction(Instruction *inst);
	void AddLabel(Label *label);
	void AddDirective(Directive *dir);
	bool Construct();
	void DumpInstructionStream() const;
	void DumpRatios() const;
	void DumpInstCounts() const;
	void DumpLoopInfo() const;
	void DumpLoopRatios() const;
	void DumpLoopInstCounts() const;
	void DumpCycles(const Device *) const;
	void DumpLoopCycles(const Device *) const;
	void DumpBBs() const;
	CFG * GetCFG() const {return cfg;}
	void BuildCFG(bool unrolled = false);
	void DumpCFG() const;

	private:
	list <Instruction *> *inst_stream;
	vector <Label *> *label_stream;
	vector <Directive *> *directive_stream;
	Parser *parser;
	CFG *cfg;
	unsigned num_warps;
};
#endif
