CXX = g++
CXXFLAGS = -g -Wall

SRCFILES = Parser.cxx Reader.cxx Kernel.cxx Statement.cxx Driver.cxx Utils.cxx CFG.cxx Output.cxx
BINFILE = ptx-analyze

all:
	$(CXX) $(CXXFLAGS) $(SRCFILES) -o $(BINFILE)

clean:
	rm -f *.o $(BINFILE)
