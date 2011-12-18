#include "Driver.h"
#include <cstdlib>

// temporary flag to turn on experimental features
bool exp_mode = false;

// The set of options that need to be supported by the analyzer
// -counts : counts of various types of instructions in each kernel
// -ratios : ratio of low-latency ops to high-latency ops in each kernel
// -loopinfo : information related to loops in each kernel
// -loopcounts : instruction counts in various loop bodies
// -loopratios : ratio of low-latency ops to high-latency ops in each kernel

// Given the name of the ptx file, create the appropriate
// reader, parser and kernel for analysis
Driver::Driver(int argc, char **argv) throw (IOException) : options(0), nwarps(32), nthreads(0)
{
	if (argc < 2) {
		PrintUsage();
		exit(-1);
	}

	// process command line options, ignorning argv[0]
	// TODO: Replace this implementation with getopt
	bool fname_processed = false;

	for (int i = 1; i < argc; ++i) {
		string option = argv[i];
		if (option[0] == '-') {
			option = argv[i] + 1;
			if (option == "counts") counts = 1;
			else if (option == "ratios") ratios = 1;
			else if (option == "loopinfo") loopinfo = 1;
			else if (option == "loopcounts") loopcounts = 1;
			else if (option == "loopratios") loopratios = 1;
			else if (option == "dumpbb") dumpbb = 1;
			else if (option == "dumpcfg") dumpcfg = 1;
			else if (option == "dumpinst") dumpinst = 1;
			else if (option == "dotcfg") dotcfg = 1;
			else if (option == "cycles") cycles = 1;
			else if (option == "loopcycles") loopcycles = 1;
			else if (option == "unrolled") unrolled = 1;
			else if (option == "exp") exp_mode = true;
			else if (option.find_first_of("warps") == 0) {
				unsigned idx = option.find_first_of("=");
				Assert(idx != (unsigned) option.npos, "Invalid warp count option");
				const string& wcount = option.substr(idx + 1, option.size() - idx);
				nwarps = atoi(wcount.c_str());
			}
			else {
				cout << "Unknown option " << option << ". Ignored..." << endl;
			}
		}
		else {
			Assert((fname_processed == false), "Multiple input files seen!");
			fname_processed = true;
			reader = new Reader(option);
			parser = new Parser(reader);
		}
	}
	if (!fname_processed) {
		PrintUsage();
		exit(-1);
	}
}

Driver::~Driver()
{
	delete reader;
	delete parser;
}

// This is where all the action begins
void Driver::Execute() throw()
{
	while (parser->HasMoreKernels()) {
		parser->Reinit();
		// build the kernel
		kernel = new Kernel(parser);

		kernel->SetNumWarps(nwarps);

		kernel->Construct();
		kernel->BuildCFG(unrolled);

		if (counts)
			kernel->DumpInstCounts();

		if (ratios)
			kernel->DumpRatios();

		if (loopratios)
			kernel->DumpLoopRatios();

		if (loopinfo)
			kernel->DumpLoopInfo();

		if (loopcounts)
			kernel->DumpLoopInstCounts();

		if (dumpinst)
			kernel->DumpInstructionStream();

		if (dumpcfg)
			kernel->DumpCFG();

		if (dumpbb)
			kernel->DumpBBs();

		if (cycles)
			kernel->DumpCycles(0);

		if (loopcycles)
			kernel->DumpLoopCycles(0);

		if (dotcfg)
			DumpCFGToDot(kernel->GetCFG());

		delete kernel;
	}
}

void Driver::PrintUsage() const
{
	cout << "Usage: ptx-analyze [options] ptx-file" << endl;
	cout << "where options is one or more of: " << endl;
	cout << " -counts" << endl;
	cout << " -ratios" << endl;
	cout << " -loopinfo" << endl;
	cout << " -loopratios" << endl;
	cout << " -loopcounts" << endl;
	cout << " -dumpinst" << endl;
	cout << " -dumpcfg" << endl;
	cout << " -dotcfg" << endl;
	cout << " -cycles" << endl;
}

// The entry point for the analyzer program
int main(int argc, char **argv)
{
	try {
		Driver driver(argc, argv);
		driver.Execute();
	} catch (IOException& ioe) {
		cout << "Input file not found" << endl;
		exit(-1);
	}
	catch (...) {
		cout << "Driver aborted" << endl;
		exit(-1);
	}

	return 0;
}
