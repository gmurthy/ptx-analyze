#include "Parser.h"
#include "Utils.h"

#include <cstdlib>
#include <iostream>
using namespace std;

// Define the constant helper objects
const string& Parser::_SENTINEL_ = "_SENTINEL_";
const string& Parser::GLOBAL_OP_STR = "g[";
const string& Parser::SHARED_OP_STR = "s[";
const string& Parser::LOCAL_OP_STR = "l[";

// todo: right now, all arithmetic instructions are classified as ALU ops,
// and assigned equal number of cycles, but some ops like div take longer
// than others - so we need to classify those ops separately to get better results
string Parser::alu_opcs[] = {
	"add", "sub", "addc", "subc", "mul", "mad", "mul24", "mad24", "sad", "div", "rem", "subr", \
	"abs", "neg", "min", "max", "pre", "ex2", /* Integer arithmetic instructions */ \
	"set", "setp", "selp", "slct", /* Compare and set instructions */ \
	"and", "or", "xor", "not", "cnot", "shl", "shr", /* Logical and shift instructions */ \
	"rcp", "sqrt", "rsqrt", "sin", "cos", "lg2", "ex2" /* FP instructions */ \
	"trap", "brkpt", "nop", "join", /* Misc instructions */
	_SENTINEL_ /* Sentinel */
};

string Parser::branch_opcs[] = {
	"bra", "call", "ret", "exit", "return", _SENTINEL_ /* CF instructions */
};

string Parser::mem_opcs[] = {
	"mov", "ld", "st", "cvt", "tex", "movsh", _SENTINEL_ /* Mem instructions */
};

string Parser::sync_opcs[] = {
	"bar", "atom", "red", "vote", /*"join",*/ _SENTINEL_ /* Synchronization operations */
};

// Initialize the fields
Parser::Parser(Reader *r)
: reader(r), done(false), end(false) {}

// Copy ctor
Parser::Parser(const Parser& p)
: reader(p.reader), done(p.done), end(p.end) {}

Parser::~Parser()
{
	reader = 0;
}

// The main parsing routine. This routine is expected to be called by a higher
// level driver that is responsible for constructing the kernel. The driver
// repeatedly calls Parse() and passes the returned Statement object to the
// kernel, thereby transforming the ptx text into an in-memory representation
Statement * Parser::Parse()
{
	// We need to handle labels specially, since a label definition and
	// the succeeding instruction both appear on the same line (in decuda o/p)
	static bool label_active = false;
	static Label *current_label = 0;

	static unsigned linenum = 0;

	Assert(!done, "No more lines to parse");

	// Special handling of labels
	if (label_active) {
		label_active = false;
		// check if we have an instruction in the same buffer
		if (Parser::IsInstruction(buffer)) {
			Instruction *tmp = Instruction::CreateInstruction(buffer, linenum);	
			tmp->SetIsBranchTarget();
			current_label->SetNextInst(tmp);
			current_label = 0;
			return tmp;
		}
	}

	end = !(reader->NextLine(buffer));
	linenum = reader->GetLineNum();

#ifdef DEBUG
	cout << buffer << endl;
#endif

#if 0
	while (Parser::IsComment(buffer) && !done) {
		done = !(reader->NextLine(buffer));
		linenum = reader->GetLineNum();
	}
#endif

	if (Parser::IsComment(buffer)) {
		if (buffer.find_first_of("{") != buffer.npos) {
			paren_stack.push(1);
		}
		else if (buffer.find_first_of("}") != buffer.npos) {
			paren_stack.pop();
			if (paren_stack.empty()) {
				// we've reached the end of the kernel
				done = true;
			}
		}
		// We should be returning Comment objects here
		return Directive::CreateDirective(buffer, linenum);
	}

	if (Parser::HasInlineComment(buffer)) {
		Parser::StripInlineComment(buffer);
	}

	if (Parser::IsLabel(buffer)) {
		label_active = true;
		Label *tmp = Label::CreateLabel(GetLabelBuffer(buffer), linenum);
		// cache the label so that we can set the target instruction
		// when we parse it
		current_label = tmp;
		return tmp;
	}
	else if (Parser::IsDirective(buffer)) {
		// if this is an entry directive, display the kernel name
		if (buffer.find("entry") == 1) {
			cout << "Processing kernel: " << buffer.substr(buffer.find_first_of(" ") + 1) << endl;
			cout << "----------------------------------" << endl;
		}
		return Directive::CreateDirective(buffer, linenum);
	}
	// if it's not a label or a directive, it has to be an instr
	else {
		Assert(Parser::IsInstruction(buffer), "Unknown Statement object seen");
		return Instruction::CreateInstruction(buffer, linenum);
	}
	return 0;
}

bool Parser::HasInlineComment(const string& str)
{
	return (str.find("//") != str.npos);
}

void Parser::StripInlineComment(string& str)
{
	Assert(Parser::HasInlineComment(str), "Expecting inline comment, but found none");
	str = str.substr(0, str.find_first_of("//"));
}

// Given an instruction string, count the number of operands
const unsigned Parser::ParseOpCount(const string& str)
{
	unsigned spaces = 0, index = 0;
	while ((index = str.find_first_of(SPACE_CHAR, index + 1)) != (unsigned) str.npos) {
		++spaces;
	}
	return spaces;
}

// Given an instruction string, check if it is a global operation
bool Parser::IsGlobalOp(const string& buf)
{
	return FindOpInBuffer(Parser::GLOBAL_OP_STR, buf);
}

// Given an instruction string, check if it is a shared operation
bool Parser::IsSharedOp(const string& buf)
{
	return (FindOpInBuffer(Parser::SHARED_OP_STR, buf) ||
				 FindOpInBuffer("movsh", buf));
}

// Given an instruction string, check if it is a local operation
bool Parser::IsLocalOp(const string& buf)
{
	return FindOpInBuffer(Parser::LOCAL_OP_STR, buf);
}

// Todo: Clean this up later
bool Parser::IsRet(const string& buf)
{
	return (buf.find("ret") != buf.npos);
}

bool Parser::IsCall(const string& buf)
{
	return (buf.find("call") != buf.npos);
}

// Search for a given key in the given string
bool Parser::FindOpInBuffer(const string& key, const string& buf)
{
	unsigned op_count = ParseOpCount(buf);
	for (unsigned i = 0; i < op_count; ++i) {
		string operand = Parser::GetOperandAt(buf, i);
		if (operand.find(key) != operand.npos) 
			return true;
	}
	return false;
}

// Given an instruction string and an index, return the operand
// at the index. For example if buf == "add $r1, $r2, $r3", and
// index == 0, then GetOperandAt, returns "$r1"
string Parser::GetOperandAt(const string& buf, unsigned index)
{
	unsigned op_start = 0, op_end;
	
	// Move past the opcode
	op_start = buf.find_first_of(SPACE_CHAR, op_start);
	++op_start;

	for (unsigned i = 0; i < index; ++i) {
		op_start = buf.find_first_of(SPACE_CHAR, op_start);
		++op_start;
	}
	op_end = buf.find_first_of(SPACE_CHAR, op_start);
	return buf.substr(op_start, (op_end - op_start));
}

// Given an instruction string, figure out what the opcode is
Opcode Parser::ParseOpCode(const string& buf)
{
	bool predicated = false;
	string opcode = (Parser::IsLabel(buf)) ? Parser::GetInstructionBufferFromLabel(buf) : buf;

	if (opcode[0] == AT_CHAR) {
		// this is a predicated instruction, find the actual opcode
		opcode = opcode.substr(opcode.find_first_of(SPACE_CHAR) + 1, opcode.size());
		predicated = true;
	}
	opcode = opcode.substr(0, opcode.find_first_of(SPACE_CHAR));
	opcode = opcode.substr(0, opcode.find_first_of(DOT_CHAR));

	// handle buggy decuda output
	if (opcode.find('?') != opcode.npos)
		opcode = opcode.substr(0, opcode.find_first_of('?'));

	//if (opcode[0] == AT_CHAR) {
		// we're seeing a predicate based branch instr
		//return OPR_COND_BRANCH;
	//}

	if (Parser::SearchOpcode(opcode, alu_opcs))
		return OPR_ALU;

	if (Parser::SearchOpcode(opcode, branch_opcs)) {
		if (predicated) 
			return OPR_COND_BRANCH;
		return OPR_BRANCH;
	}

	if (Parser::SearchOpcode(opcode, mem_opcs))
		return OPR_MEM;

	if (Parser::SearchOpcode(opcode, sync_opcs))
		return OPR_SYNC;

	string msg("Invalid opcode: ");
	msg += opcode;
	Assert(false, msg);
	return OPR_INVALID;
}

// Given a branch instruction or a label, figure out what the label
// corresponding label number is
unsigned Parser::ParseLabelNumber(const string& buf)
{
	const string label = "label";
	const unsigned op_count = ParseOpCount(buf);
	
	// if op_count == 0, then we're seeing a label def
	string label_op = (op_count > 0) ? GetOperandAt(buf, op_count - 1) : buf;

	Assert(label_op.find(label) == 0, "Unexpected format of label");
	string tmp = label_op.substr(label.size());
	return atoi(tmp.c_str());
}

// Given a key opcode and an array of possible opcodes, check if
// the key opcode belongs to the array.
bool Parser::SearchOpcode(const string& opc, string opc_list[])
{
	unsigned i = 0;
	while (opc_list[i] != _SENTINEL_) {
		if (opc == opc_list[i++])
			return true;
	}
	return false;
}

// Again, special handling of labels. A single ptx string could contain
// the label definition, followed by the target instruction. Return the
// instruction that sits beyond the label definition
string Parser::GetInstructionBufferFromLabel(const string& label)
{
	Assert(Parser::IsInstruction(label), "Attempting to extract instruction from empty label");
	return label.substr(GetInstPos(label));
}

string Parser::GetLabelBuffer(const string& buf)
{
	Assert(Parser::IsLabel(buf), "Attempting to extract label from non-label");
	return buf.substr(0, buf.find_first_of(COLON_CHAR));
}

// Given a combined label + instruction string, return the position at which the 
// instruction starts
const unsigned Parser::GetInstPos(const string& str)
{
	Assert(Parser::IsLabel(str), "Trying to fetch instruction from non-label statement");
	unsigned index = str.find_first_of(COLON_CHAR) + 1;
	while (str[index] == SPACE_CHAR) ++index;
	return index;
}

// Given a statement string, check if it is a label
bool Parser::IsLabel(const string& str)
{
	if (Parser::IsDirective(str)) return false;
	
	unsigned index = str.find_first_of(COLON_CHAR, 0);
	if (index != (unsigned) str.npos) {
		Assert(str.find_last_of(COLON_CHAR, str.size()) == index, "Malformed label statement");
		return true;
	}
	return false;
}

// Given a statement string, check if it is an instruction. There seems to be no
// easy and efficient of determining this - so the current approach is 'proof by
// contradiction'. If a statement is neither a directive, nor a comment,
// it _has_ to be an instruction. Of course, this is very brittle and needs to be
// improved - but this works for now :) Standard engineering excuse!
bool Parser::IsInstruction(const string& str)
{
	if (Parser::IsDirective(str)) return false;
	if (Parser::IsComment(str)) return false;

	string instbuf = (Parser::IsLabel(str)) ? str.substr(Parser::GetInstPos(str)) : str;

	unsigned opcount = Parser::ParseOpCount(instbuf);
	return (opcount >= 1 && opcount <= 5);
}

// currently using this as a hack to drop all statements that are not interesting
bool Parser::IsComment(const string& buf)
{
	if (buf.find("//", 0) == 0)
		return true;
	if (buf.find("{") != buf.npos || buf.find("}") != buf.npos)
		return true;
	if (buf.find("#") != buf.npos)
		return true;
	return false;
}

// Given a statement string, check if it is a directive
bool Parser::IsDirective(const string& str)
{
	unsigned index = str.find_first_of(DOT_CHAR);
	if (index == 0) {
		Assert(str.find_last_of(DOT_CHAR, 0) == index, "Malformed directive statement");
		return true;
	}
	return false;
}

void Parser::ParseMemOp(const string& str, MemOp& optype)
{
	unsigned count = ParseOpCount(str);
	string op_str;

	if (IsGlobalOp(str)) 
		op_str = GLOBAL_OP_STR;
	else if (IsSharedOp(str))
		op_str = SHARED_OP_STR;
	else if (IsLocalOp(str)){
		op_str = LOCAL_OP_STR;
	}
	else return;

	for (unsigned i = 0; i < count; ++i) {
		string tmp = GetOperandAt(str, i);
		if (tmp.find_first_of(op_str) != tmp.npos) {
			switch (i) {
				case 0: 
					optype = MEM_STORE;
					break;
				default:
					optype = MEM_LOAD;
			}
			return;
		}
	}
}

void Parser::ParseRegs(const string& buf, int& dst, int& src0, int& src1, int& src2)
{
	const string& str = (Parser::IsLabel(buf)) ? buf.substr(Parser::GetInstPos(buf)) : buf;
	unsigned count = ParseOpCount(str), begin_idx, end_idx, len, reg;

	for (unsigned i = 0; i < count; ++i) {
		const string& tmp = GetOperandAt(str, i);
		begin_idx = tmp.find_first_of("r");

		// This operand is a register operand
		if (begin_idx != (unsigned) tmp.npos) {
			end_idx = tmp.find_first_of(".");
			if (end_idx == (unsigned) tmp.npos)
				end_idx = tmp.find_first_of("]");
			if (end_idx == (unsigned) tmp.npos)
				end_idx = tmp.find_first_of(",");
			if (end_idx == (unsigned) tmp.npos) 
				end_idx = tmp.size();

			len = end_idx - begin_idx - 1;
			const string& reg_str = tmp.substr(begin_idx + 1, len);
			reg = atoi(reg_str.c_str());

			switch (i) {
				case 0:
					dst = reg;
					break;
				case 1:
					src0 = reg;
					break;
				case 2:
					if (src0 == -1) src0 = reg;
					else src1 = reg;
					break;
				default:
					if (src0 == -1) src0 = reg;
					else if (src1 == -1) src1 = reg;
					else {
						Assert(src2 == -1, "Multiple src operands in instr");
						src2 = reg;
					}
			}
		}
	}
}
