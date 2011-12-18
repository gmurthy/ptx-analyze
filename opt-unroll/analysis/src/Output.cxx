#include "CFG.h"
#include "Kernel.h"

template <typename T>
static void DumpInfoFromBBs(T start, T end, DumpType type, string& msg) 
{
	// The eventual goal is to have a loop that checks for each bit set in
	// the type parameter and take appropriate action. Currently, only
	// counts and ratios are implemented, so we just check for the two
	
	unsigned long total_insts, ainsts, ginsts, sinsts, binsts, linsts;
	total_insts = ainsts = ginsts = sinsts = binsts = linsts = 0;
	for (T iter = start; iter != end; ++iter) {
		BasicBlock *bb = *iter;
		total_insts += bb->GetTotalOpCount();
		ainsts += bb->GetAluOpCount();
		ginsts += bb->GetGlobalOpCount();
		sinsts += bb->GetSharedOpCount();
		linsts += bb->GetLocalOpCount();
		binsts += bb->GetBranchOpCount();
	}
	if (type & DUMP_COUNTS) {
		cout << msg << "Instruction count summary: " << endl;
		cout << msg << "Total instructions = " << total_insts << endl;
		cout << msg << "  ALU instructions = " << ainsts << endl;
		cout << msg << "  Global mem instructions = " << ginsts << endl;
		cout << msg << "  Shared mem instructions = " << sinsts << endl;
		cout << msg << "  Local mem instructions = " << linsts << endl;
		cout << msg << "  Branch instructions = " << binsts << endl;
	}
	if (type & DUMP_RATIOS) {
		cout << msg << "#ALU instructions = " << ainsts << endl;
		cout << msg << "#Global instructions = " << ginsts << endl;
		if (ginsts > 0) 
			cout << msg << "Ratio of ALU ops to global ops = " << (double(ainsts))/ginsts << endl;
	}
}

// Dump loop information recursively
void Loop::DumpInfo(DumpType type) const
{
	// DUMP_INFO is implicit, so we do not check for it
	
	string tabs = "";
	for (unsigned i = 0; i < GetNestingLevel(); ++i) {
		tabs += "\t";
	}

	cout << tabs << "Loop index: " << Id() << ", Nesting level: " << GetNestingLevel() << endl;
	cout << tabs << "Instruction count: " << GetNumInstrs() << endl;
	cout << tabs << "Enclosing loop: ";
	if (GetEnclosingLoop() == 0) 
		cout << "None" << endl;
	else 
		cout << GetEnclosingLoop()->Id() << endl;

	// Dump instruction counts from the blocks in the nat-loop
	DumpInfoFromBBs<set<BasicBlock*>::const_iterator>(nat_loop.begin(), nat_loop.end(), type, tabs);

	cout << endl;

	if (HasInnerLoops()) {
		for (LoopListConstRevIter iter = InnerLoopsRBegin(), rend = InnerLoopsREnd(); iter != rend; ++iter) {
			Loop *inner = *iter;
			cout << tabs << "Inner loop details: " << endl;
			inner->DumpInfo(type);
			cout << endl;
		}
	}
}

// Walk through the list of outer loops and dump information
void CFG::DumpLoopInfo() const
{
	cout << "Detected " << loops->size() << " outer loop(s)" << endl;
	for (LoopListConstIter iter = loops->begin(), end = loops->end(); iter != end; ++iter) {
		Loop *loop = *iter;
		loop->DumpInfo(DUMP_INFO);
	}
}

// Walk through all the basic-blocks in the current cfg and dump inst counts
// This gives a count of the total number of instructions present in the current cfg
void CFG::DumpInstCounts() const
{
	string msg = "";
	DumpInfoFromBBs<BBListConstIter>(BlocksBegin(), BlocksEnd(), DUMP_COUNTS, msg);
}

void CFG::DumpRatios() const
{
	string msg = "";
	DumpInfoFromBBs<BBListConstIter>(BlocksBegin(), BlocksEnd(), DUMP_RATIOS, msg);
}

// Dump instruction count information for the various loops in the kernel
// Note that the information associated with each loop corresponds to the
// loop as well as all its inner loops
void CFG::DumpLoopInstCounts() const
{
	// Walk through the outer loops and recursively dump instr counts
	for (LoopListConstIter iter = loops->begin(), end = loops->end(); iter != end; ++iter) {
		Loop *loop = *iter;
		loop->DumpInfo(static_cast<DumpType>(DUMP_INFO | DUMP_COUNTS));
	}
}

void CFG::DumpLoopRatios() const
{
	// Walk through the outer loops and recursively dump instr counts
	for (LoopListConstIter iter = loops->begin(), end = loops->end(); iter != end; ++iter) {
		Loop *loop = *iter;
		loop->DumpInfo(static_cast<DumpType>(DUMP_INFO | DUMP_RATIOS));
	}
}

void CFG::DumpBasicBlocks() const
{
	for (BBListConstIter iter = BlocksBegin(); iter != BlocksEnd(); ++iter) {
		BasicBlock *bb = *iter;
		cout << "Basic Block # " << bb->Id() << " : " << endl;
		Instruction *inst = bb->GetFirstInst(), *end = bb->GetLastInst();
		while (inst != end) {
			cout << inst->GetAscii() << endl;
			inst = dynamic_cast<Instruction *>(inst->GetNext());
		}
		if (inst) cout << inst->GetAscii() << endl;
		cout << endl;
	}
}

void CFG::DumpCFG() const
{	
	for (BBListConstIter iter = BlocksBegin(); iter != BlocksEnd(); ++iter) {
		BasicBlock *bb = *iter;
		cout << "Basic Block # " << bb->Id() << " : " << endl;
		if (bb->IsLoopHeader()) cout << "LH " << endl;
		if (bb->IsLoopFooter()) cout << "LF " << endl;
		cout << "Successors: ";
		for (BBListConstIter iter = bb->SuccBegin(); iter != bb->SuccEnd(); ++iter) {
			cout << (*iter)->Id() << " ";
		}
		cout << endl;
		cout << "Predecessors: ";
		for (BBListConstIter iter = bb->PredBegin(); iter != bb->PredEnd(); ++iter) {
			cout << (*iter)->Id() << " ";
		}
		cout << endl << endl;
	}
}

// Debug routine for dumping the current instruction stream
void Kernel::DumpInstructionStream() const
{
	Instruction *instr = GetFirstInst();
	while (instr != 0) {
		cout << instr->GetAscii();
		if (instr->IsGlobalOp()) {
			cout << " : GLOBAL OP";
		}
		else if (instr->IsSharedOp()) {
			cout << " : SHARED OP";
		}
		else if (instr->IsLocalOp()) {
			cout << " : LOCAL OP";
		}
		cout << endl;
		instr = dynamic_cast<Instruction *>(instr->GetNext());
	}
}

void Kernel::DumpRatios() const
{
	cfg->DumpRatios();
	#if 0
	unsigned long global_count = 0, alu_count = 0;
	Instruction *instr = GetFirstInst();
	while (instr != 0) {
		if (instr->IsGlobalOp()) ++global_count;
		else if (!instr->IsSyncOp()) ++alu_count;
		instr = dynamic_cast<Instruction *>(instr->GetNext());
	}
	cout << "Global ops = " << global_count << ", ALU ops = " << alu_count << endl;
	cout << "Number of ALU ops per global op = " << (double)((double) alu_count) / global_count << endl;
	#endif
}

void DumpCFGToDot(CFG *cfg)
{
	ofstream dot_file("cfg.dot");
	dot_file << "digraph structs {" << endl;
	dot_file << "size = \"7.5, 10\";" << endl;
	dot_file << "node [shape=record];" << endl;

	for (BBListConstIter iter = cfg->BlocksBegin(); iter != cfg->BlocksEnd(); ++iter) {
		BasicBlock *bb = *iter;
		dot_file << "\t struct" << bb->Id() << "[shape=record, label=\"";

		if (bb->Id() == 65535) {
			dot_file << "Entry block \\n";
			dot_file << "\"];" << endl;
			continue;
		}

		if (bb->Id() == 65536) {
			dot_file << "Exit block \\n";
			dot_file << "\"];" << endl;
			continue;
		}

		dot_file << "BB " << bb->Id() << "\\n";
		dot_file << "(Instruction count: " << bb->GetTotalOpCount() << ")\\n";
		if (bb->IsLoopHeader()) {
			Loop *l = cfg->loop_header_map->find(bb)->second;
			dot_file << "Loop Header "; 
			dot_file << "(Nesting depth " << l->GetNestingLevel() << ")\\n";
		}
		if (bb->IsLoopFooter())
			dot_file << "Loop Footer" << "\\n";
		Instruction *inst = bb->GetFirstInst();
		while (inst != bb->GetLastInst()->GetNext()) {
			string str = inst->GetAscii();
			const string replace = "\\|";
			if (str.find_first_of("|") != str.npos) {
				str.replace(str.find_first_of("|"), 1, replace);
			}
			dot_file << str /* inst->GetAscii() */;
			if (inst->IsAluOp()) dot_file << " (A)\\n";
			else if (inst->IsBranchOp()) dot_file << " (B)\\n";
			else if (inst->IsLocalOp()) dot_file << " (L)\\n";
			else if (inst->IsSharedOp()) dot_file << " (S)\\n";
			else if (inst->IsGlobalOp()) dot_file << " (G)\\n";
			else if (inst->IsSyncOp()) dot_file << " (N)\\n";
			else dot_file << "\\n";
			dot_file << inst->cycles << "\\n";
			inst = dynamic_cast<Instruction *> (inst->GetNext());
		} 
		//dot_file << inst->GetAscii() << "\\n"; 
		dot_file << "\"];" << endl;
	}

	for (BBListConstIter iter = cfg->BlocksBegin(); iter != cfg->BlocksEnd(); ++iter) {
		BasicBlock *bb = *iter;
		for (BBListConstIter succ_iter = bb->SuccBegin(); succ_iter != bb->SuccEnd(); ++succ_iter) {
			BasicBlock *succ = *succ_iter;
			dot_file << "\t struct" << bb->Id() << " -> struct" << succ->Id();
			if (bb->IsLoopFooter() && succ->IsLoopHeader()) {
				if (bb->Id() == succ->Id())
					dot_file << " [dir=back]";
			} 
			dot_file << ";" << endl;
		}
	}
	dot_file << "}" << endl;
	dot_file.close();
}
