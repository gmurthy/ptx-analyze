#include "CFG.h"
#include "Utils.h"
#include <fstream>
#include <algorithm>
using namespace std;

#define GLOBAL_MEM_LATENCY 500

extern bool exp_mode;

BasicBlock::BasicBlock(Instruction *b, Instruction *e, unsigned u)
	: begin_instr(b), end_instr(e), loop_header(false), loop_footer(false), 
	id(u), vi(COLOR_WHITE), alu_op_count(0), global_op_count(0), shared_op_count(0), 
	local_op_count(0), branch_op_count(0), sync_op_count(0), total_op_count(0) 
{
	for (Instruction *iter = b; 
			b && e && iter != e->GetNext(); 
			iter = (iter->GetNext())) {
		// check the instruction type and increment appropriate count
		if (iter->IsAluOp()) ++alu_op_count;
		else if (iter->IsBranchOp()) ++branch_op_count;
		else if (iter->IsSharedOp()) ++shared_op_count;
		else if (iter->IsLocalOp()) ++local_op_count;
		else if (iter->IsGlobalOp()) ++global_op_count;
		else {
			Assert(iter->IsSyncOp(), "Unknown op type");
			++sync_op_count;
		}
	}
	total_op_count = alu_op_count + global_op_count + shared_op_count +\
									 local_op_count + branch_op_count + sync_op_count;
}

void BasicBlock::AddSucc(BasicBlock *b)
{
	succ.push_back(b);
}

void BasicBlock::AddPred(BasicBlock *b)
{
	pred.push_back(b);
}

// This is the only tested way to construct a CFG for now
CFG::CFG(InstIter begin, InstIter end, bool unrolled) : entry(0), exit(0), constructed(0), has_loops(0), unrolled_loops(unrolled)
{
	block_map = new map<const Instruction *, BasicBlock *>();
	ComputeBasicBlocks(begin, end);
	ConstructCFG();
}

// The following 2 ctors need to be updated to ensure that all fields are inited/copied
CFG::CFG(BBList list) : constructed(0), has_loops(0)
{
	for (BBListIter iter = list.begin(); iter != list.end(); ++iter) {
		all_blocks.push_back(*iter);
	}
}

// See note above
CFG::CFG(const CFG& other) : entry(other.entry), exit(other.exit), constructed(other.constructed), has_loops(other.has_loops) 
{
	// deep-copy of basic-blocks
	for (BBListConstIter iter = other.BlocksBegin(); iter != other.BlocksEnd(); ++iter) {
		all_blocks.push_back(*iter);
	}
}

CFG::~CFG()
{
	delete block_map;
	for (BBListIter iter = BlocksBegin(), end = BlocksEnd(); iter != end; ++iter) {
		delete *iter;
	}
	all_blocks.clear();

	if (has_loops) {
		for (LoopListConstIter iter = loops->begin(), end = loops->end(); iter != end; ++iter) {
			delete *iter;
		}
		loops->clear();
		delete loops;
		loop_header_map->clear();
		delete loop_header_map;
	}
}

// This is where we look at a stream of instructions and build basic-blocks
// and also create references between the basic-blocks, thereby creating a CFG
void CFG::ComputeBasicBlocks(InstIter begin, InstIter end)
{
	Instruction *first, *last, *cur, *prev;
	BasicBlock *bb = 0;
	unsigned index = 0;

	first = last = cur = prev = 0;

	// create dummp entry and exit blocks
	entry = new BasicBlock(0, 0, 65535);
	AddBasicBlock(entry);

	for (InstIter iter = begin; iter != end; ++iter) {
		if ((*iter)->IsDeleted()) continue;

		// We use the standard algorithm to identify leader statements
		// and create basic-blocks
		prev = cur;
		cur = *iter;
		if (first == 0) {
			// This is an instruction following a branch instruction - this
			// is the first of a new basic-block as well
			first = cur;
		}

		if (cur->IsBranchTarget()) {
			// This is a branch target and therefore a new leader statement
			// Create a BasicBlock for the bb we've seen so far
			// However, if first == cur, it means that the previous block
			// already terminated because of a branch instr just prior to this
			// branch-target. In this case, we do not need to terminate the 
			// bb again
			if (first != cur) {
				last = prev;
				bb = new BasicBlock(first, last, index++);
				block_map->insert(pair<Instruction *, BasicBlock *>(first, bb));
				AddBasicBlock(bb);
				// start a new bb
				first = cur;
			}
		}

		if (cur->IsBranchOp()) {
			// We're seeing the last of a basic-block
			last = cur;
			bb = new BasicBlock(first, last, index++);
			block_map->insert(pair<Instruction *, BasicBlock *>(first, bb));
			AddBasicBlock(bb);
			first = 0;
		}
	}

	// make sure we've not left the instrs in the last basic-block
	// dangling in mid-air
	if (last != cur) {
		// we have a dangling bb - close it up
		bb = new BasicBlock(first, cur, index++);
		block_map->insert(pair<Instruction *, BasicBlock *>(first, bb));
		AddBasicBlock(bb);
	}
	exit = new BasicBlock(0, 0, 65536);
	AddBasicBlock(exit);
}

void CFG::ConstructCFG()
{
	// Walk through the list of basic-blocks. Look for successor blocks
	// based on the last instruction of each block - if the last instr
	// is a conditional branch, the block has 2 successors - the branch
	// target and the fall thru block, if the last instr is an uncond 
	// branch, the block has only one successor, if it's neither, then 
	// the block has only one successor - the fall through block
	BasicBlock *bb, *prev = 0, *branch_target_bb;
	const Instruction *terminator, *branch_target;

	prev = entry;

	for (BBListConstIter iter = BlocksBegin() + 1, end = BlocksEnd() - 1; iter != end; ++iter) {
		bb = *iter;
		// first check if the current block is a successor to the prev block
		if (prev) {
			prev->AddSucc(bb);
			bb->AddPred(prev);
			prev = 0;
		}
		terminator = bb->GetLastInst();
		if (terminator->IsBranchOp()) {
			branch_target = terminator->GetBranchTarget();
			if (branch_target == 0) {
				// this seems like a return statement
				Assert(terminator->GetLabelNumber() == -1, "Missing branch target for non-return stmt");
				bb->AddSucc(exit);
				exit->AddPred(bb);
			}
			else {
				Assert((block_map->find(branch_target) != block_map->end()), "Incorrect block map state");
				branch_target_bb = block_map->find(branch_target)->second;
				bb->AddSucc(branch_target_bb);
				branch_target_bb->AddPred(bb);
			}
			if (terminator->IsCondBranch()) {
				prev = bb;
			}
		}
		else {
			prev = bb;
		}
	}

	bb->AddSucc(exit);
	exit->AddPred(bb);
	
	// We're done using the block_map, clear it
	block_map->clear();
	constructed = 1;
}

void CFG::AddBasicBlock(BasicBlock *bb)
{
	all_blocks.push_back(bb);
}

void CFG::AddLoop(Loop *loop)
{
	if (has_loops == 0) {
		has_loops = 1;
		loops = new vector<Loop *>();
	}
	loops->push_back(loop);
}

unsigned CFG::DetectLoops()
{
	Assert((constructed == 1), "Detecting loops before CFG construction");
	loop_header_map = new map<BasicBlock *, Loop *>();

	BasicBlock *first = *(BlocksBegin());
	DoDFS(first);

	// all the loops have been identified, construct nat loops
	for (LoopListConstIter iter = loops->begin(), end = loops->end(); iter != end; ++iter) {
		Loop *loop = *iter;
		loop->ConstructNatLoop(loop_header_map);
	}

	// adjust nesting depths of the loops
	bool changed = true;
	while(changed) {
		changed = false;
		for (LoopListConstIter iter = loops->begin(), end = loops->end(); iter != end; ++iter) {
			Loop *loop = *iter;
			if (loop->GetEnclosingLoop() != 0) {
				unsigned short old_level = loop->GetNestingLevel();
				unsigned short new_level = loop->GetEnclosingLoop()->GetNestingLevel() + 1;
				if (old_level != new_level) changed = true;
				loop->SetNestingLevel(new_level);
			}
		}
	}

	// if the loops in the kernel are unrolled, read the unroll configurations
	// from the user and update the loop iterations accordingly
	if (unrolled_loops) {
		ifstream uconf_file("./.uconf");
		if (uconf_file.bad() || uconf_file.fail()) {
			cerr << "Error reading unroll config file. Using default loop iter count" << endl;
		}
		else {
			vector<unsigned> ufactors;
			uconf_file.flags(ios::skipws);
			for (unsigned i = 0; i < loops->size() && !(uconf_file.eof() || uconf_file.fail()); ++i) {
				unsigned tmp = 1;
				uconf_file >> tmp;
				ufactors.push_back(tmp);
			}
			if (ufactors.size() != loops->size()) {
				cerr << "Number of unroll factors != number of loops. Using default loop iter count" << endl;
			}
			else {
				for (unsigned i = 0; i < loops->size(); ++i) {
					Loop *loop = (*loops)[i];
					unsigned ufactor = ufactors[loop->Id()];
					if (ufactor == 0)
						loop->SetNumIters(0);
					else
						loop->SetNumIters(loop->GetNumIters() / ufactor);
				}
			}
		}
	}

	// walk through the list of loops and remove non-outermost loops
	// we dont want to track inner loops here, instead pointers to 
	// inner loops are maintained in each outer-loop
	vector <Loop *>::iterator iter = loops->begin(), end = loops->end();
	while (iter != loops->end()) {
		Loop *loop = *iter;
		if (loop->GetNestingLevel() != 0) {
			iter = loops->erase(iter);
		}
		else {
			++iter;
		}
	}
	return 0;
}

// A convenience routine to find the 'true' CFG successor of the loop footer
// i.e the successor which is not the loop header
static BasicBlock * FindLoopFooterSuccessor(Loop *loop)
{
	bool found_valid_succ = false;
	BasicBlock *succ, *footer = loop->GetFooter();

	// walk through the list of successors - ignore loop back edges
	for (BBListConstIter succ_iter = footer->SuccBegin();
			 succ_iter != footer->SuccEnd(); ++succ_iter) {
		succ = *succ_iter;
		if (succ != loop->GetHeader()) {
			found_valid_succ = true;
			break;
		}
	}

	// ensure we're in good shape
	Assert(found_valid_succ, "Loop footer has no valid successor");
	return succ;
}

static BasicBlock * FindBBSuccessor(BasicBlock *iter)
{
	// Figure out the way forward
	unsigned num_succ = iter->NumSucc();
	Assert((num_succ > 0 && num_succ < 3), "Invalid CFG node seen");
	if (iter->NumSucc() == 1) 
		iter = *(iter->SuccBegin());
	else {
		// num_succ == 2
		BBListConstIter succ_iter = iter->SuccBegin();
		BasicBlock *succ0 = *succ_iter;
		++succ_iter;
		BasicBlock *succ1 = *succ_iter;

		// Ensure that we take the path of the loop-body and not the loop exit
		// Possible cases:
		// 1. Successor has only 1 predecessor - then this succ is part of the loop body
		// 2. Successor has 2 predecessors and is a loop-header - then this is part of the body
		// 3. Successor has 2 predecessors - then this is the loop-exit part, choose the other
		Assert((succ1->NumPred() > 0 && succ0->NumPred() > 0), "CFG node with no preds seen");
		if (succ1->NumPred() == 1
				|| (succ1->NumPred() == 2 && succ1->IsLoopHeader())) {
			Assert((succ0->NumPred() > 1 || 
						(succ0->NumSucc() == 1 && (*succ0->SuccBegin())->NumPred() > 1)), "Ill-formed CFG (Conditionals in loop?)");
			iter = succ1;
		}
		else {
			Assert((succ0->NumPred() == 1 && succ1->NumPred() > 1), "Ill-formed CFG (Conditionals in loop?)");
			iter = succ0;
		}
	}
	return iter;
}

static void UpdateCyclesInMap(map<int, unsigned long long>& cmap, unsigned long long new_cycles)
{
	//map<int, unsigned long long> tmp_map;

	for (map<int, unsigned long long>::iterator iter = cmap.begin();
			iter != cmap.end(); 
			++iter) {
		int reg = iter->first;
		unsigned long long cycles = iter->second;
		cmap.erase(reg);
		cycles += new_cycles;
		cmap.insert(pair<int, unsigned long long>(reg, cycles));
	}
	// copy new values over to original map
	//cmap = tmp_map;
}

unsigned long long stall_cycles = 0;

unsigned long long
CFG::CountLoopCycles(const Loop *loop, const Device *device, unsigned num_warps) const
{
	unsigned long long total_cycles = 0, current_cycles = 0, loop_stall_cycles = 0;

	if (loop->HasInnerLoops()) {
		// this is not the inner-most loop, process the current loop
		// and all the inner loops recursively
		BasicBlock *bb_iter = loop->GetHeader();
		Instruction *inst_iter = loop->GetHeader()->GetFirstInst();
		Instruction *last_inst = (loop->GetFooter()->GetLastInst()->GetNext());

		map<int, unsigned long long> global_load_cycles;

		while (inst_iter != last_inst) {
			// walk the loop forwards
			Instruction *block_last_inst = bb_iter->GetLastInst()->GetNext();
			while (inst_iter != block_last_inst) {
			int src_regs[3];
			src_regs[0] = inst_iter->GetRegSrc0();
			src_regs[1] = inst_iter->GetRegSrc1();
			src_regs[2] = inst_iter->GetRegSrc2();

			if (exp_mode) {
				for (unsigned i = 0; i < 3; ++i) {
					int src = src_regs[i];
					if (global_load_cycles.find(src) != global_load_cycles.end()) {
						// We're seeing a use of global load
						unsigned long long cycles = global_load_cycles.find(src)->second;
						if (cycles < GLOBAL_MEM_LATENCY) {
							// The global mem load latency has not been hidden
							unsigned long long tmp_cycles = max<unsigned long long>((current_cycles * num_warps), GLOBAL_MEM_LATENCY - cycles);
							total_cycles += tmp_cycles;
							UpdateCyclesInMap(global_load_cycles, tmp_cycles);
							if ((current_cycles * num_warps) < (GLOBAL_MEM_LATENCY - cycles)) {
								loop_stall_cycles += (GLOBAL_MEM_LATENCY - cycles - (current_cycles * num_warps));
							}
							current_cycles = 0;
						}
						// This load has completed, delete the record
						global_load_cycles.erase(src);
					}
				}
			}

			switch(inst_iter->GetOpcode()) {
				case OPR_ALU:
				case OPR_BRANCH:
				case OPR_COND_BRANCH:
					if (!exp_mode) {
						current_cycles += 4;
						break;
					}
				case OPR_MEM:
					if (inst_iter->IsSharedOp() || (exp_mode && inst_iter->GetOpcode() != OPR_MEM)) {
						current_cycles += 4;
						if (exp_mode) {
							UpdateCyclesInMap(global_load_cycles, 4);
						}
					}
					else if (inst_iter->IsGlobalOp() || inst_iter->IsLocalOp()) {
						// need to take care of register dependences here
						current_cycles += 4; 

						if (exp_mode) {
							UpdateCyclesInMap(global_load_cycles, 4);
							if (inst_iter->IsMemLoad()) {
								int dst = inst_iter->GetRegDst();
								Assert(global_load_cycles.find(dst) == global_load_cycles.end(), "Multiple global loads to same register");
								global_load_cycles.insert(pair<int, unsigned long long>(dst, 4));
							}
							else {
								// This is a global store - we do not know very well how many cycles are spent on a store
								//current_cycles += 4;
								total_cycles += (current_cycles * num_warps); current_cycles = 0;
							}
						}
						
						// a global/local mem causes a warp-switch
						else {
							total_cycles += max<unsigned long long>((current_cycles * num_warps), GLOBAL_MEM_LATENCY);
							current_cycles = 0;
						}
					}
					else {
						Assert(false, "Unknown mem op" + inst_iter->GetAscii());
					}
					break;
				case OPR_SYNC:
					total_cycles += (current_cycles * num_warps);
					current_cycles = 0;
					break;
				case OPR_INVALID:
				default:
					Assert(false, "Unknown instruction opcode");
			}
			inst_iter = (inst_iter->GetNext());
			}

			if (inst_iter == last_inst) {
				total_cycles += (current_cycles * num_warps);
				current_cycles = 0;
				break;
			}

			//bb_iter = (*bb_iter->SuccBegin());
			bb_iter = FindBBSuccessor(bb_iter);

			if (bb_iter->IsLoopHeader()) {
				Loop *inner_loop = GetLoopFromHeader(bb_iter);
				total_cycles += current_cycles * num_warps; current_cycles = 0;
				unsigned long long tmp_cycles = CountLoopCycles(inner_loop, device, num_warps);
				cout << "Total cycles in inner loop " << inner_loop->Id() \
					<< " (Header bb: " << inner_loop->GetHeader()->Id() << ") = " << tmp_cycles << endl;
				total_cycles += tmp_cycles;
				bb_iter = FindLoopFooterSuccessor(inner_loop);
			}
			inst_iter->cycles = total_cycles;
			inst_iter = bb_iter->GetFirstInst();
		}
		Assert(global_load_cycles.empty(), "Global load unused at loop exit");
	}
	else {
		// this is an inner-most loop
		#if 0
		for (BBSetConstIter bb_iter = loop->NatLoopBegin(), bb_end = loop->NatLoopEnd();
				 bb_iter != bb_end; ++bb_iter) {
			cout << "Processing block number: " << (*bb_iter)->Id() << " (L)" << endl;
		}
		#endif

		bool blocking_inst_seen = false;
		unsigned long long later_cycles = 0;
		BasicBlock *bb_iter = loop->GetFooter();
		Instruction *inst_iter = bb_iter->GetLastInst(), *first_blocking_inst = 0;
		map<int, unsigned long long> global_load_cycles;

		// walk the loop backwards till we reach the first instr in the header
		while (inst_iter != loop->GetHeader()->GetFirstInst()->GetPrev()) {

			// walk each bb backwards till we reach the first inst in the block
			Instruction *block_first_inst = (bb_iter->GetFirstInst()->GetPrev());
			while (inst_iter != block_first_inst) {
				if (inst_iter->IsGlobalOp() || inst_iter->IsSyncOp() || inst_iter->IsLocalOp()) {
					// we've reached the last set of blocking instructions in the loop
					blocking_inst_seen = true;
					first_blocking_inst = inst_iter;
					later_cycles += 4;
					break;
				}
				else {
					later_cycles += 4;
				}
				inst_iter = (inst_iter->GetPrev());
			}
			// if we've seen the last blocking inst, we're done
			if (blocking_inst_seen) break;

			Assert((bb_iter->NumPred() == 1 || bb_iter->IsLoopHeader()), "Loop block with multiple preds");
			bb_iter = *(bb_iter->PredBegin());
			inst_iter = bb_iter->GetLastInst();
		}

		// We've now computed how many cycles are taken from the last set of
		// blocking insts in the loop body to the top of the loop; now compute
		// how many cycles are used up till the first set of blocking insts
		if (first_blocking_inst == 0) {
			// the loop body is full of ALU ops and no blocking insts; we've
			// already computed the total cycles into later_cycles
			stall_cycles += (loop_stall_cycles * loop->GetNumIters());
			return loop->GetNumIters() * later_cycles * num_warps;
		}
		inst_iter = loop->GetHeader()->GetFirstInst();
		current_cycles = later_cycles;

		while (true) {

			int src_regs[3];
			src_regs[0] = inst_iter->GetRegSrc0();
			src_regs[1] = inst_iter->GetRegSrc1();
			src_regs[2] = inst_iter->GetRegSrc2();

			for (unsigned i = 0; i < 3; ++i) {
				int src = src_regs[i];
				if (exp_mode && global_load_cycles.find(src) != global_load_cycles.end()) {
					// We're seeing a use of global load
					unsigned long long cycles = global_load_cycles.find(src)->second;
					if (cycles < GLOBAL_MEM_LATENCY) {
						// The global mem load latency has not been hidden
						unsigned long long tmp_cycles = max<unsigned long long>((current_cycles * num_warps), GLOBAL_MEM_LATENCY - cycles);
						total_cycles += tmp_cycles;
						UpdateCyclesInMap(global_load_cycles, tmp_cycles);
						if ((current_cycles * num_warps) < (GLOBAL_MEM_LATENCY - cycles)) {
							loop_stall_cycles += (GLOBAL_MEM_LATENCY - cycles - (current_cycles * num_warps));
						}
						current_cycles = 0;
					}
					global_load_cycles.erase(src);
				}
			}

			switch(inst_iter->GetOpcode()) {
				case OPR_ALU:
				case OPR_BRANCH:
				case OPR_COND_BRANCH:
					if (!exp_mode) {
						current_cycles += 4;
						break;
					}
				case OPR_MEM:
					if (inst_iter->IsSharedOp() || (exp_mode && inst_iter->GetOpcode() != OPR_MEM)) {
						current_cycles += 4;
						if (exp_mode) {
							UpdateCyclesInMap(global_load_cycles, 4);
						}
					}
					else if (inst_iter->IsGlobalOp() || inst_iter->IsLocalOp()) {
						current_cycles += 4;
						// a global/local mem causes a warp-switch
						
						if (exp_mode) {
							UpdateCyclesInMap(global_load_cycles, 4);
							if (inst_iter->IsMemLoad()) {
								int dst = inst_iter->GetRegDst();
								Assert(global_load_cycles.find(dst) == global_load_cycles.end(), "Multiple global loads to same register");
								global_load_cycles.insert(pair<int, unsigned long long>(dst, 4));
							}
							else {
								total_cycles += max<unsigned long long>((current_cycles *num_warps), GLOBAL_MEM_LATENCY);
								current_cycles = 0;
								//current_cycles += 4;
							}
						}

						else {
							while (inst_iter->GetNext() && 
									(inst_iter->GetNext()->IsGlobalOp() || inst_iter->GetNext()->IsLocalOp())) {
								current_cycles += 4;
								inst_iter = inst_iter->GetNext();
							}
							total_cycles += max<unsigned long long>((current_cycles * num_warps), GLOBAL_MEM_LATENCY);
							current_cycles = 0;
						}
						if (inst_iter == first_blocking_inst) {
							// We've covered the entire loop; return
							stall_cycles += (loop_stall_cycles * loop->GetNumIters());
							total_cycles += (current_cycles * num_warps);
							return loop->GetNumIters() * total_cycles;
						}
					}
					else {
						Assert(false, "Unknown mem op" + inst_iter->GetAscii());
					}
					break;
				case OPR_SYNC:
					total_cycles += (current_cycles * num_warps);
					current_cycles = 0;
					if (inst_iter == first_blocking_inst) {
						stall_cycles += (loop_stall_cycles * loop->GetNumIters());
						return loop->GetNumIters() * total_cycles;
					}
					break;
				case OPR_INVALID:
				default:
					Assert(false, "Unknown instruction opcode");
			}
			inst_iter->cycles = total_cycles;
			inst_iter = (inst_iter->GetNext());
		}
	}
	stall_cycles += (loop_stall_cycles * loop->GetNumIters());
	total_cycles += current_cycles * num_warps;
	return loop->GetNumIters() * total_cycles;
}

unsigned long long 
CFG::CountCycles(const Device *device, unsigned num_warps) const
{
	Assert(constructed == 1, "CFG not constructed");
	BasicBlock *iter = entry;
	unsigned long long total_cycles = 0, current_cycles = 0;
	map<int, unsigned long long> global_load_cycles;

	// Walk through all the bbs in the kernel and compute
	// the total number of cycles
	while (true) {
		// We've reached the end of the CFG
		if (iter->Id() == exit->Id()){
			// flush the counters
			total_cycles += (current_cycles * num_warps);
			current_cycles = 0;
			break;
		}

		if (iter->IsLoopHeader()) {
			Loop *loop = GetLoopFromHeader(iter);
			Assert(loop != 0, "Loop-header map broken");

			// Process the loop and compute the number of cycles
			total_cycles += (current_cycles * num_warps); current_cycles = 0;
			unsigned long long loop_cycles = CountLoopCycles(loop, device, num_warps);
			total_cycles += loop_cycles;
			cout << "Total cycles in loop " << loop->Id() \
					 << " (Header bb: " << loop->GetHeader()->Id() << ") = " << loop_cycles << endl;

			iter = FindLoopFooterSuccessor(loop);
			Assert(iter != 0, "Loop with multiple footers seen");
			continue;
		}
		else {
		#ifdef DEBUG
			cout << "Processing block number: " << iter->Id() << endl;
		#endif

			// Walk through the insts in the current block
			Instruction *inst_iter = iter->GetFirstInst();
			while (inst_iter && inst_iter != iter->GetLastInst()->GetNext()) {

			int src_regs[3];
			src_regs[0] = inst_iter->GetRegSrc0();
			src_regs[1] = inst_iter->GetRegSrc1();
			src_regs[2] = inst_iter->GetRegSrc2();

			for (unsigned i = 0; i < 3; ++i) {
				int src = src_regs[i];
				if (exp_mode && global_load_cycles.find(src) != global_load_cycles.end()) {
					// We're seeing a use of global load
					unsigned long long cycles = global_load_cycles.find(src)->second;
					if (cycles < GLOBAL_MEM_LATENCY) {
						// The global mem load latency has not been hidden
						unsigned long long tmp_cycles = max<unsigned long long>((current_cycles * num_warps), GLOBAL_MEM_LATENCY - cycles);
						total_cycles += tmp_cycles;
						UpdateCyclesInMap(global_load_cycles, tmp_cycles);
						if ((current_cycles * num_warps) < (GLOBAL_MEM_LATENCY - cycles)) {
							stall_cycles += (GLOBAL_MEM_LATENCY - cycles - (current_cycles * num_warps));
						}
						current_cycles = 0;
					}
					global_load_cycles.erase(src);
				}
			}

			switch(inst_iter->GetOpcode()) {
				case OPR_ALU:
				case OPR_BRANCH:
				case OPR_COND_BRANCH:
					if (!exp_mode) {
						current_cycles += 4;
						break;
					}
				case OPR_MEM:
					if (inst_iter->IsSharedOp() || (exp_mode && inst_iter->GetOpcode() != OPR_MEM)) {
						current_cycles += 4;
						if (exp_mode) {
							UpdateCyclesInMap(global_load_cycles, 4);
						}
					}
					else if (inst_iter->IsGlobalOp() || inst_iter->IsLocalOp()) {
						// a global/local mem causes a warp-switch

						if (exp_mode) {
							UpdateCyclesInMap(global_load_cycles, 4);
							if (inst_iter->IsMemLoad()) {
								int dst = inst_iter->GetRegDst();
								Assert(global_load_cycles.find(dst) == global_load_cycles.end(), "Multiple global loads to same register");
								global_load_cycles.insert(pair<int, unsigned long long>(dst, 4));
							}
							else {
								total_cycles += max<unsigned long long>((current_cycles * num_warps), GLOBAL_MEM_LATENCY);
								current_cycles = 0;
								//current_cycles += 4;
							}
						}
						else {
							while (inst_iter && (inst_iter->IsGlobalOp() || inst_iter->IsLocalOp())) {
								current_cycles += 4;
								inst_iter = inst_iter->GetNext();
							}
							total_cycles += max<unsigned long long>((current_cycles * num_warps), GLOBAL_MEM_LATENCY);
							current_cycles = 0;
						}
					}
					else {
						Assert(false, "Unknown mem op" + inst_iter->GetAscii());
					}
					break;
				case OPR_SYNC:
					total_cycles += (current_cycles * num_warps);
					current_cycles = 0;
					break;
				case OPR_INVALID:
				default:
					Assert(false, "Unknown instruction opcode");
			}
			inst_iter->cycles = total_cycles;
			inst_iter = inst_iter ? (inst_iter->GetNext()) : 0;
			}
		}

		iter = FindBBSuccessor(iter);

	}
	cout << "Total stall cycles = " << stall_cycles << endl;
	return total_cycles;
}

Loop::Loop(BasicBlock *h, BasicBlock *f) : id(global_loop_index++), header(h), footer(f), enclosing_loop(0), /*num_iters(64)*/ num_iters(256), num_instrs(0), nesting_level(0), multiple_footers(0), has_inner_loops(0) {}

Loop::~Loop()
{
	if (has_inner_loops) {
		for (vector <Loop *>::iterator iter = inner_loops->begin(), end = inner_loops->end(); iter != end; ++iter) {
			delete *iter;
		}
		inner_loops->clear();
		delete inner_loops;
	}
	if (multiple_footers)
		delete footers;
}

void Loop::AddFooter(BasicBlock *bb)
{
	if (multiple_footers == 0) {
		multiple_footers = 1;
		footers = new vector<BasicBlock *>();
	}
	footers->push_back(bb);
}

void Loop::AddInnerLoop(Loop *inner)
{
	if (has_inner_loops == 0) {
		has_inner_loops = 1;
		inner_loops = new vector<Loop *>();
	}
	inner_loops->push_back(inner);
}

// Initialize the static variable
unsigned short Loop::max_nesting_level = 0;
unsigned Loop::global_loop_index = 0;

void Loop::ConstructNatLoop(map<BasicBlock *, Loop *> *lh_map)
{
	// We use the straightforward technique described in the dragon
	// book. Start with the footer and recursively add all the preds
	// till we reach the header
	stack <BasicBlock *> bb_stack;
	unsigned num_instrs = 0;

	nat_loop.insert(header);
	num_instrs += header->GetNumInstrs();

	if (nat_loop.find(footer) == nat_loop.end()) {
		nat_loop.insert(footer);
		bb_stack.push(footer);
		num_instrs += footer->GetNumInstrs();
	}

	while(!bb_stack.empty()) {
		BasicBlock *bb = bb_stack.top(); bb_stack.pop();
		// identify nested loops
		if (bb->IsLoopHeader()) {
			Assert((bb != header), "Inconsistent nat-loop state");
			Assert((lh_map->find(bb) != lh_map->end()), "Loop for header not found in map");
			Loop *inner = lh_map->find(bb)->second;
			if (inner->GetEnclosingLoop() == 0) {
				AddInnerLoop(inner);
				inner->SetEnclosingLoop(this);
			}
		}
		for (BBListIter iter = bb->PredBegin(), end = bb->PredEnd(); iter != end; ++iter) {
			BasicBlock *pred = *iter;
			if (nat_loop.find(pred) == nat_loop.end()) {
				nat_loop.insert(pred);
				bb_stack.push(pred);
				num_instrs += pred->GetNumInstrs();
			}
		}
	}
	SetNumInstrs(num_instrs);
}

void CFG::DoDFS(BasicBlock *bb)
{
	Assert((!bb->GetFullyVisited()), "Invalid CFG edge detected");

	if (bb->GetPartiallyVisited())
		// we're the target of a back-edge, mostly a loop-header
		return;

	if (bb->GetNotVisited())
		bb->SetPartiallyVisited();

	for (BBListIter iter = bb->SuccBegin(), end = bb->SuccEnd(); iter != end; ++iter) {
		BasicBlock *succ = *iter;
		if (succ->GetPartiallyVisited()) {
			// this is a CFG back-edge. so we're the loop-footer and the successor 
			// is the loop-header. Mark the blocks and create a loop structure and 
			// attach to CFG
			if (!succ->IsLoopHeader()) {
				succ->SetLoopHeader();
				Loop *loop = new Loop(succ, bb);
				AddLoop(loop);
				loop_header_map->insert(pair<BasicBlock *, Loop *>(succ, loop));
			}
			else {
				// this bb is a footer of a loop that has already been discovered
				// multiple footers could exist in situations such as continue
				// statements and trailing if conditions and so on
				Assert((loop_header_map->find(succ) != loop_header_map->end()), "Invalid loop information");
				Loop *loop = loop_header_map->find(succ)->second;
				loop->AddFooter(bb);
			}
			bb->SetLoopFooter();
		}
		if (succ->GetNotVisited()) {
			DoDFS(succ);
		}
	}
	// Finish visting this node
	bb->SetFullyVisited();
}

