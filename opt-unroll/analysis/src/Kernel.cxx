#include "Kernel.h"
#include "Utils.h"

#include <map>
#include <stack>
#include <set>
using namespace std;

// create the various streams and set the parser
Kernel::Kernel(Parser *p) : parser(p), cfg(0), num_warps(32)
{
	inst_stream = new list<Instruction *>();
	label_stream = new vector<Label *>();
	directive_stream = new vector<Directive *> ();
}

// clean up and release memory
Kernel::~Kernel()
{
	if (cfg) delete cfg;

	for (InstIter iter = inst_stream->begin();
				iter != inst_stream->end();
				++iter) {
		delete *iter;
	}

	for (vector<Label *>::iterator iter = label_stream->begin();
				iter != label_stream->end();
				++iter) {
		delete *iter;
	}

	for (vector<Directive *>::iterator iter = directive_stream->begin();
				iter != directive_stream->end();
				++iter) {
		delete *iter;
	}

	inst_stream->clear();
	label_stream->clear();
	directive_stream->clear();

	delete inst_stream;
	delete label_stream;
	delete directive_stream;
}

// Append an instruction to the inst stream and set up prev and next ptrs
void Kernel::AddInstruction(Instruction *inst)
{
	if (inst_stream->size() > 0) {
		GetLastInst()->SetNext(inst);
		inst->SetPrev(GetLastInst());
	}
	inst_stream->push_back(inst);
	inst->SetNext(0);
}

// Add a label to the label stream; note that the work
// of creating and populating the fields of the label
// object are done as part of CreateLabel
void Kernel::AddLabel(Label *label)
{
	label_stream->push_back(label);
}

// Add a new directive to the directive stream
void Kernel::AddDirective(Directive *dir)
{
	directive_stream->push_back(dir);
}

bool operator < (const InstIter& x, const InstIter& y) 
{
	return (*x)->GetLineNum() < (*y)->GetLineNum();
}

// This is where we build the kernel, parsing the ptx file line by line
bool Kernel::Construct()
{
	map<unsigned, Label *> branch_targets;

	Instruction::reset_fields = true;

	while (!parser->Done()) {
		Statement *stmt = parser->Parse();

		// if the parser choked on a line, just continue
		if (stmt == 0) continue;

		if (IsA<Instruction *> (stmt)) {
			AddInstruction(dynamic_cast<Instruction *>(stmt));
		}
		else if (IsA<Label *> (stmt)) {
			Label *label = dynamic_cast<Label *>(stmt);
			branch_targets.insert(std::pair<unsigned, Label *>(label->GetNumber(), label));
			AddLabel(dynamic_cast<Label *>(stmt));
		}
		else {
			// has to be a directive - no use for directives yet
			Assert(IsA<Directive *> (stmt), "Unknown Statement object seen");
			AddDirective(dynamic_cast<Directive *>(stmt));
		}
	}

	// A lot of maps to keep track of call-sites and function entry/exit points
	map <InstIter, InstIter> fn_entry_exit_map, fn_cs_entry_map;
	map <Instruction *, InstIter> fn_entry_cs_map;
	set <Instruction *> fn_entries;
	set <InstIter> call_sites;
	stack <InstIter> function_stack;

	// Make a pass over the instructions and patch branch targets correctly
	for (InstIter iter = InstBegin(); iter != InstEnd(); ++iter) {
		Instruction *inst = *iter;
		Label *tmp_label;

		if (inst->IsBranchTarget()) {
			if (fn_entries.find(inst) != fn_entries.end()) {
				// We're seeing the start of a function body
				// Add the entry point onto a stack to match the return
				InstIter cs = fn_entry_cs_map.find(inst)->second;
				fn_cs_entry_map.insert(pair<InstIter, InstIter>(cs, iter));
				function_stack.push(iter);
			}
		}
		if (inst->IsBranchOp()) {
			if (inst->IsRet()) {
				// Match function return with entry
				if (!function_stack.empty()) {
					InstIter entry = function_stack.top();
					function_stack.pop();
					// store the entry/exit so that the exit can be extracted based on the entry
					fn_entry_exit_map.insert(pair<InstIter, InstIter>(entry, iter));
				}
			}
			int label_number = inst->GetLabelNumber();
			if (label_number == -1) {
				// leave return statements alone
				inst->SetBranchTarget(0);
				continue; 
			}
			Assert((branch_targets.find(label_number) != branch_targets.end()), "Unseen label being referenced");
			tmp_label = branch_targets.find(label_number)->second;
			inst->SetBranchTarget(tmp_label->GetNextInst());
			// At call-sites, we note the label of the called function and mark the corresponding
			// instruction as the start of a function body, so that the entry and exit can be matched
			// and inlined later
			if (inst->IsCall()) {
				fn_entries.insert(tmp_label->GetNextInst());	
				fn_entry_cs_map.insert(pair<Instruction *, InstIter>(tmp_label->GetNextInst(), iter));
				// keep a list of all call-sites to inline later
				call_sites.insert(iter);
			}
		}
	}

	// Inline function at the point of the call-site
	for (set <InstIter>::iterator iter = call_sites.begin(); iter != call_sites.end(); ++iter) {
		InstIter cs = *iter;
		InstIter entry = fn_cs_entry_map.find(cs)->second;
		InstIter exit = fn_entry_exit_map.find(entry)->second;

		// Setup the prev and next pointers for inline
		(*entry)->SetPrev((*cs));
		(*exit)->SetNext((*cs)->GetNext());
		(*cs)->GetNext()->SetPrev((*exit));
		(*cs)->SetNext(*entry);
		(*exit)->SetBranchTarget((*exit)->GetNext());
		// Inline the function - move the instructions ranging from the
		// function entry to the function exit right next to the call-site
		inst_stream->splice(++cs, *inst_stream, entry, ++exit);
	}

	return true;
}

void Kernel::BuildCFG(bool unrolled)
{
	cfg = new CFG(InstBegin(), InstEnd(), unrolled);
	cfg->DetectLoops();
}

void Kernel::DumpCFG() const
{
	// for now, dump the list of all basic-blocks
	cfg->DumpCFG();
}

void Kernel::DumpBBs() const
{
	cfg->DumpBasicBlocks();
}

void Kernel::DumpLoopInfo() const
{
	cfg->DumpLoopInfo();
}

void Kernel::DumpInstCounts() const
{
	cfg->DumpInstCounts();
}

void Kernel::DumpLoopInstCounts() const
{
	cfg->DumpLoopInstCounts();
}

void Kernel::DumpLoopRatios() const
{
	cfg->DumpLoopRatios();
}

void Kernel::DumpCycles(const Device *device) const
{
	unsigned long long cycles = cfg->CountCycles(device, GetNumWarps());
	cout << "Total number of cycles = " << cycles << endl;
}

void Kernel::DumpLoopCycles(const Device *device) const
{
}
