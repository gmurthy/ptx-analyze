#ifndef _STATEMENT_INCLUDED_
#define _STATEMENT_INCLUDED_

#include <string>
#include <vector>
#include <list>
using namespace std;

// todo: using macros is bad practice; replace asap
#define SPACE_CHAR	' '
#define COLON_CHAR	':'
#define DOT_CHAR		'.'
#define AT_CHAR			'@'

// A convenient type to track different types of interesting opcodes
typedef enum opcode_t 
{
	OPR_INVALID = -1,
	OPR_ALU,
	OPR_BRANCH,
	OPR_COND_BRANCH,
	OPR_MEM,
	OPR_SYNC
} Opcode;

typedef enum memop_t
{
	MEM_UNKNOWN = -1,
	MEM_LOAD,
	MEM_STORE
} MemOp;

// The Statement is an abstraction of each statement in a ptx file. It is a very
// minimal base class that is intended to be subclassed appropriately to specialize
// for different constructs such as instructions, comments, directives and labels
class Statement
{
	public:
	// ctors and dtors
	Statement(unsigned l, std::string a);
	Statement(const Statement& s);
	virtual ~Statement() {}

	// member functions
	inline unsigned GetLineNum() const {return linenum;}
	inline void SetLineNum(unsigned l) {linenum = l;}
	inline const string& GetAscii() const {return ascii;}
	inline void SetAscii(std::string a) {ascii = a;}

	private:
	unsigned linenum;
	string ascii; // each statement maintains the ascii representation of the statement
};

// The Instruction class subclasses from Statement and represents a ptx instruction.
// It contains all the information about the instruction that is necessary for analysis
class Instruction : public Statement
{
	public:
	Instruction(unsigned l, string a, Instruction *p, Instruction *n = 0);
	Instruction(const Instruction&);
	inline Instruction * GetPrev() const {return prev;}
	inline Instruction * GetNext() const {return next;}
	inline void SetPrev(Instruction *p) {prev = p;}
	inline void SetNext(Instruction *n) {next = n;}
	inline bool IsAluOp() const {return alu_op;}
	inline bool IsMemOp() const {return mem_op;}
	inline bool IsSyncOp() const {return sync_op;}
	inline bool IsGlobalOp() const {return mem_op && global_op;}
	inline bool IsBranchOp() const {return branch_op;}
	inline bool IsSharedOp() const {return shared_op;}
	inline bool IsLocalOp() const {return local_op;}
	inline bool IsCondBranch() const {return cond_branch;}
	inline bool IsCall() const {return call_op;}
	inline bool IsRet() const {return ret_op;}
	inline bool IsDeleted() const {return deleted;}
	inline void Delete() {deleted = 1;}
	inline const unsigned GetOpCount() const {return op_count;}
	inline const int GetLabelNumber() const {return label_number;}
	inline const Instruction * GetBranchTarget() const {return branch_target;}
	inline void SetBranchTarget(Instruction *i) {branch_target = i;}
	inline bool IsBranchTarget() const {return is_branch_target;}
	inline void SetIsBranchTarget(bool b = true) {is_branch_target = b;}
	inline Opcode GetOpcode() const {return opc;}
	inline int GetRegDst() const {return reg_dst;}
	inline int GetRegSrc0() const {return reg_src0;}
	inline int GetRegSrc1() const {return reg_src1;}
	inline int GetRegSrc2() const {return reg_src2;}
	inline int GetMemOpType() const {return memop_type;}
	inline bool IsMemLoad() const {return memop_type == MEM_LOAD;}
	inline bool IsMemStore() const {return memop_type == MEM_STORE;}
	void Classify();

	static Instruction * CreateInstruction(const string&, unsigned);
	static bool reset_fields;

	~Instruction() {}

	private:
	Instruction *prev, *next;
	unsigned op_count;
	int label_number;
	Opcode opc;
	Instruction *branch_target;
	bool is_branch_target;
	int reg_src0, reg_src1, reg_src2, reg_dst;
	MemOp memop_type;

	/* Various flags representing the type of operation */
	unsigned deleted:1;
	unsigned alu_op:1;
	unsigned mem_op:1;
	unsigned sync_op:1;
	unsigned global_op:1;
	unsigned shared_op:1;
	unsigned local_op:1;
	unsigned branch_op:1;
	unsigned cond_branch:1;
	unsigned call_op:1;
	unsigned ret_op:1;

	public:
	// For debugging: a snapshot of the cycle counter while processing this instr
	unsigned long long cycles;
};

typedef list<Instruction *>::iterator InstIter;

// Class Label subclasses from class Statement and represents a ptx label
// Labels are very crucial to identifying basic-blocks and loops. Each label
// contains a pointer to the target instruction, as you might expect
class Label : public Statement
{
	public:
	Label(unsigned l, std::string a, Label *p, Label *n = 0, Instruction *i = 0);
	Label(const Label& l);
	inline void SetNextInst(Instruction *inst) {next_inst = inst;}
	inline void SetNext(Label *l) {next = l;}
	inline void SetPrev(Label *l) {prev = l;}
	inline Instruction * GetNextInst() const {return next_inst;}
	inline unsigned GetNumber() const {return number;}
	inline void SetNumber(unsigned n) {number = n;}
	static Label * CreateLabel(const string&, unsigned);
	~Label() {}

	private:
	Label *prev, *next;
	Instruction *next_inst;
	unsigned number;
};

// Class Directive also subclasses from Statement. Directives are useful for
// determining the reg usage, smem usage and so on. They can probably be parsed
// once, and thrown away after the useful information has been gleaned
class Directive : public Statement
{
	public:
	Directive(unsigned l, const std::string a);
	Directive(const Directive& d);
	static Directive * CreateDirective(const string&, unsigned);
	~Directive() {}
};

template <class T>
bool IsA (Statement *ptr)
{
	T tptr = dynamic_cast<T>(ptr);
	return tptr != 0;
}

#endif
