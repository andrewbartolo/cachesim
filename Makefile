CXXFLAGS=-Wall -Werror -Ofast -fPIC
CXXSTD=c++1y
LDFLAGS=-lstdc++
SRCFILES=$(wildcard *.cpp)
BUILDDIR=build
# Every file in OBJFILES has the directory prepended to it
OBJFILES=$(SRCFILES:%.cpp=$(BUILDDIR)/%.o)
BINNAME=test

.PHONY: clean run

all: $(BINNAME)
$(BINNAME): $(BUILDDIR) $(OBJFILES)
	$(CXX) $(CXXFLAGS) -std=$(CXXSTD) -o $(BUILDDIR)/$(BINNAME) $(LDFLAGS) $(OBJFILES)
	ln -sf $(BUILDDIR)/$(BINNAME) $(BINNAME)

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

# TODO unify concept of below pattern rule and $(OBJFILES)
# TODO do .h check too (this currently fails for main.cpp, since it has no .h)
$(BUILDDIR)/%.o: %.cpp
	$(CXX) $(CXXFLAGS) -std=$(CXXSTD) -c -o $@ $<


run:
	$(BUILDDIR)/$(BINNAME)

clean:
	rm -rf $(BUILDDIR) $(BINNAME)
