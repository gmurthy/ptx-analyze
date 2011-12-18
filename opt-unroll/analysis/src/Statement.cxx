#include "Statement.h"
#include "Utils.h"
#include "Parser.h"
#include <limits.h>

// Implementation of the Statement class
Statement::Statement(unsigned l, std::string a)
: linenum(l), ascii(a) {}

Statement::Statement(const Statement& s)
: linenum(s.linenum), ascii(s.ascii) {}

// Implementation of the Instruction class
Instruction::Instruction(unsigned l, std::string a, Instruction *p, Instruction *n)
: Statement(l, a), prev(p), next(n), branch_target(0), is_branch_target(false), reg_src0(-1), reg_src1(-1), reg_src2(-1), reg_dst(-1), \
  memop_type(MEM_UNKNOWN), deleted(0), alu_op(0), mem_op(0), sync_op(0), global_op(0), shared_op(0), local_op(0), branch_op(0), cond_branch(0), call_op(0), ret_op(0), cycles(0) {}

Instruction::Instruction(const Instruction& i)
: Statement(i), prev(i.prev), next(i.next), branch_target(i.branch_target), is_branch_target(i.is_branch_target), \
  reg_src0(i.reg_src0), reg_src1(i.reg_src1), reg_src2(i.reg_src2), reg_dst(i.reg_dst), memop_type(i.memop_type),
	deleted(i.deleted), alu_op(i.alu_op), mem_op(i.mem_op), sync_op(i.sync_op), global_op(i.global_op), shared_op(i.shared_op),  \
	local_op(i.local_op), branch_op(i.branch_op), cond_branch(i.cond_branch), call_op(i.call_op) , ret_op(i.ret_op), cycles(i.cycles) {}

bool Instruction::reset_fields = true;

// Given an instruction string, call the parser to parse the contents, and create
// the instruction object
Instruction * Instruction::CreateInstruction(const string& str, unsigned linenum)
{
	static Instruction *prev = 0;

	if (reset_fields) {
		reset_fields = false;
		prev = 0;
	}

	//string instbuf = (Parser::IsLabel(str)) ? Parser::GetInstructionBufferFromLabel(str) : str;
	const string& instbuf = str;

	Instruction *instr = new Instruction(linenum, instbuf, prev, 0);
	instr->SetPrev(prev);
	if (prev) {
		prev->SetNext(instr);
	}
	prev = instr;
	instr->Classify();
	return instr;
}

// This is where we parse the contents of the instruction buffer and populate
// the various fields of the instr object
void Instruction::Classify()
{
	const string& instrbuf = GetAscii();
	Parser::ParseRegs(instrbuf, reg_dst, reg_src0, reg_src1, reg_src2);
	opc = Parser::ParseOpCode(instrbuf);
	switch (opc) {
		case OPR_ALU:
			alu_op = 1;
			break;
		case OPR_COND_BRANCH:
			cond_branch = 1;
			/* fall through */
		case OPR_BRANCH:
			branch_op = 1;
			label_number = Parser::IsRet(instrbuf) ? -1 : Parser::ParseLabelNumber(instrbuf);
			if (Parser::IsCall(instrbuf)) call_op = 1;
			if (Parser::IsRet(instrbuf)) ret_op = 1;
			break;
		case OPR_MEM:
			mem_op = 1;
			Parser::ParseMemOp(instrbuf, memop_type);
			if (Parser::IsGlobalOp(instrbuf)) {
				global_op = 1;
			}
			else if (Parser::IsSharedOp(instrbuf)) {
				shared_op = 1;
			}
			else if (Parser::IsLocalOp(instrbuf)) {
				local_op = 1;
			}
			else {
				// Could be a reg-reg mov/cvt op
				opc = OPR_ALU;
				alu_op = 1;
			}
			break;
		case OPR_SYNC:
			sync_op = 1;
			break;
		default:
			Assert(false, "Invalid opcode");
	}
	op_count = Parser::ParseOpCount(instrbuf);
	return;
}

// Implementation of the Label class
Label * Label::CreateLabel(const string& str, unsigned linenum)
{
	Label *label = new Label(linenum, str, 0);
	label->SetNumber(Parser::ParseLabelNumber(str));
	return label;
}

Label::Label(unsigned l, std::string a, Label *p, Label *n, Instruction *i)
: Statement(l, a), prev(p), next(n), next_inst(i), number(INT_MAX) {}

Label::Label(const Label& l)
: Statement(l), prev(l.prev), next(l.next), next_inst(l.next_inst), number(l.number) {}

// Implementation of the Directive class
Directive * Directive::CreateDirective(const string& str, unsigned linenum)
{
	Directive *d = new Directive(linenum, str);
	return d;
}

Directive::Directive(unsigned l, const string a) : Statement(l, a) {}

Directive::Directive(const Directive& d) : Statement(d) {}
