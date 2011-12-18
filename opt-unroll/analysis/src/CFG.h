#ifndef _CFG_H_INCLUDED_
#define _CFG_H_INCLUDED_

#include "Statement.h"
#include "Utils.h"
#include "Device.h"
#include <iostream>
#include <vector>
#include <map>
#include <set>
#include <stack>
using namespace std;

typedef enum {COLOR_WHITE, COLOR_GRAY, COLOR_BLACK} VisitState;
typedef enum {DUMP_INFO = 1, DUMP_COUNTS = 2, DUMP_RATIOS = 4} DumpType;

class VisitInfo
{
	public:
	VisitInfo(VisitState v = COLOR_WHITE, int idx = -1) : vs(v), v_idx(idx) {}
	VisitInfo& operator=(const VisitInfo& vi) {vs = vi.vs; v_idx = vi.v_idx; return *this;}

	private:
	VisitState vs;
	int v_idx;
	friend class BasicBlock;
};

// Forward decls
class BasicBlock;
class Loop;
class CFG;
class Kernel;

typedef vector<BasicBlock *> BBList;
typedef BBList::iterator BBListIter;
typedef BBList::const_iterator BBListConstIter;

typedef set<BasicBlock *> BBSet;
typedef BBSet::iterator BBSetIter;
typedef BBSet::const_iterator BBSetConstIter;

typedef vector<Loop *> LoopList;
typedef LoopList::iterator LoopListIter;
typedef LoopList::const_iterator LoopListConstIter;
typedef LoopList::reverse_iterator LoopListRevIter;
typedef LoopList::const_reverse_iterator LoopListConstRevIter;

void DumpCFGToDot(CFG *);

class BasicBlock
{
	public:
	BasicBlock(Instruction *b, Instruction *end, unsigned);
	void AddSucc(BasicBlock *);
	void AddPred(BasicBlock *);
	inline void SetLoopHeader() {loop_header = true;}
	inline void SetLoopFooter() {loop_footer = true;}
	inline bool IsLoopHeader() const  {return loop_header;}
	inline bool IsLoopFooter() const {return loop_footer;}
	inline BBListIter PredBegin() {return pred.begin();}
	inline BBListIter PredEnd() {return pred.end();}
	inline BBListConstIter PredBegin() const {return pred.begin();}
	inline BBListConstIter PredEnd() const {return pred.end();}
	inline BBListIter SuccBegin() {return succ.begin();}
	inline BBListIter SuccEnd() {return succ.end();}
	inline BBListConstIter SuccBegin() const {return succ.begin();}
	inline BBListConstIter SuccEnd() const {return succ.end();}
	inline unsigned NumPred() const {return pred.size();}
	inline unsigned NumSucc() const {return succ.size();}
	inline Instruction * GetFirstInst() const {return begin_instr;}
	inline Instruction * GetLastInst() const {return end_instr;}
	inline unsigned Id() const {return id;}
	inline unsigned GetAluOpCount() const {return alu_op_count;}
	inline unsigned GetSharedOpCount() const {return shared_op_count;}
	inline unsigned GetBranchOpCount() const {return branch_op_count;}
	inline unsigned GetLocalOpCount() const {return local_op_count;}
	inline unsigned GetTotalOpCount() const {return total_op_count;}
	inline unsigned GetGlobalOpCount() const {return global_op_count;}

	inline void SetVisitInfo(VisitInfo v) {vi = v;}
	inline void SetPartiallyVisited() {vi.vs = COLOR_GRAY;}
	inline void SetFullyVisited() {vi.vs = COLOR_BLACK;}
	inline bool GetPartiallyVisited() const {return vi.vs == COLOR_GRAY;}
	inline bool GetFullyVisited() const {return vi.vs == COLOR_BLACK;}
	inline bool GetNotVisited() const {return vi.vs == COLOR_WHITE;}
	inline const VisitState& GetVisitState() const {return vi.vs;}
	inline int GetVisitIndex() const {return vi.v_idx;}
	inline void SetVisitIndex(int idx) {vi.v_idx = idx;}
	inline unsigned GetNumInstrs() const {return total_op_count;}

	private:
	Instruction *begin_instr, *end_instr;
	BBList succ, pred;
	bool loop_header, loop_footer;
	unsigned id;
	VisitInfo vi;
	unsigned alu_op_count, global_op_count, shared_op_count, local_op_count, branch_op_count, sync_op_count, total_op_count;
};

class CFG
{
	public:
	CFG(BBList);
	CFG(InstIter, InstIter, bool unrolled = false);
	CFG(const CFG&);
	~CFG();

	inline BBListIter BlocksBegin() {return all_blocks.begin();}
	inline BBListIter BlocksEnd() {return all_blocks.end();}
	inline BBListConstIter BlocksBegin() const {return all_blocks.begin();}
	inline BBListConstIter BlocksEnd() const {return all_blocks.end();}
	inline LoopListIter LoopsBegin() {return loops->begin();}
	inline LoopListIter LoopsEnd() {return loops->end();}
	inline LoopListConstIter LoopsBegin() const {return loops->begin();}
	inline LoopListConstIter LoopsEnd() const {return loops->end();}
	void AddBasicBlock(BasicBlock *);
	unsigned DetectLoops();
	inline void AddLoop(Loop *l);
	inline Loop * GetLoopFromHeader(BasicBlock *h) const {return ((loop_header_map->find(h) == loop_header_map->end())
																																? 0: loop_header_map->find(h)->second);}

	void DumpBasicBlocks() const;
	void DumpCFG() const;
	void DumpLoopInfo() const;
	void DumpInstCounts() const;
	void DumpLoopInstCounts() const;
	void DumpRatios() const;
	void DumpLoopRatios() const;
	unsigned long long CountCycles(const Device *, unsigned) const;
	unsigned long long CountLoopCycles(const Loop *, const Device *, unsigned) const;

	private:
	BBList all_blocks;
	BasicBlock *entry, *exit;
	map <const Instruction *, BasicBlock *> *block_map;
	map <BasicBlock *, Loop *> *loop_header_map;
	LoopList *loops;
	unsigned constructed:1;
	unsigned has_loops:1;
	unsigned unrolled_loops:1;

	void ComputeBasicBlocks(InstIter, InstIter);
	void ConstructCFG();
	void DoDFS(BasicBlock *);

	friend void ::DumpCFGToDot(CFG *);
};

class Loop
{
	public:
	Loop(BasicBlock *, BasicBlock *);
	~Loop();

	inline void SetEnclosingLoop(Loop *l) {enclosing_loop = l;}
	inline Loop * GetEnclosingLoop() const {return enclosing_loop;}
	inline unsigned short GetNestingLevel() const {return nesting_level;}
	inline void SetNestingLevel(unsigned short nl) {nesting_level = nl;}
	inline unsigned short GetMaxNestingLevel() const {return max_nesting_level;}
	inline unsigned Id() const {return id;}
	inline LoopListIter InnerLoopsBegin() {Assert(HasInnerLoops(), "No inner loops"); return inner_loops->begin();}
	inline LoopListIter InnerLoopsEnd() {Assert(HasInnerLoops(), "No inner loops"); return inner_loops->end();}
	inline LoopListConstIter InnerLoopsBegin() const {Assert(HasInnerLoops(), "No inner loops"); return inner_loops->begin();}
	inline LoopListConstIter InnerLoopsEnd() const {Assert(HasInnerLoops(), "No inner loops"); return inner_loops->end();}
	inline LoopListConstRevIter InnerLoopsRBegin() const {Assert(HasInnerLoops(), "No inner loops"); return inner_loops->rbegin();}
	inline LoopListConstRevIter InnerLoopsREnd() const {Assert(HasInnerLoops(), "No inner loops"); return inner_loops->rend();}
	inline BBSetConstIter NatLoopBegin() const {return nat_loop.begin();}
	inline BBSetConstIter NatLoopEnd() const {return nat_loop.end();}
	inline BasicBlock * GetHeader() const {return header;}
	inline BasicBlock * GetFooter() const {return (multiple_footers == 1) ? 0 : footer;}
	inline unsigned GetNumIters() const {return num_iters;}
	inline void SetNumIters(unsigned n) {num_iters = n;}
	inline unsigned GetNumInstrs() const {return num_instrs;}
	inline void SetNumInstrs(unsigned num) {num_instrs = num;}
	inline bool HasInnerLoops() const {return has_inner_loops == 1;}
	void AddFooter(BasicBlock *);
	void ConstructNatLoop(map<BasicBlock *, Loop *> *);
	void AddInnerLoop(Loop *);
	void DumpInfo(DumpType) const;
	
	private:
	unsigned id;
	BasicBlock *header, *footer;
	Loop *enclosing_loop;
	vector <Loop *> *inner_loops;
	// A loop can have multiple footer blocks, for example
	// due to continue statements
	vector <BasicBlock *> *footers;
	set <BasicBlock *> nat_loop;
	unsigned num_iters;
	unsigned num_instrs;
	unsigned short nesting_level;
	unsigned multiple_footers:1;
	unsigned has_inner_loops:1;

	static unsigned short max_nesting_level;
	static unsigned global_loop_index;
};
#endif
