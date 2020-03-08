CXXFLAGS=-Wall -Werror -Ofast -fPIC
CXXSTD=c++11
LDFLAGS=-lstdc++
SRCFILES=$(wildcard *.cpp)
# Every file in OBJFILES has the directory prepended to it
OBJFILES=$(SRCFILES:%.cpp=$(BUILDDIR)/%.o)
BUILDDIR=build
LIBDIR=lib
SHAREDLIB=Cache
BINNAME=test

.PHONY: clean run

all: $(BINNAME) $(SHAREDLIB)
$(BINNAME): $(BUILDDIR) $(OBJFILES)
	$(CXX) $(CXXFLAGS) -std=$(CXXSTD) -o $(BUILDDIR)/$(BINNAME) $(LDFLAGS) $(OBJFILES)
	ln -sf $(BUILDDIR)/$(BINNAME) $(BINNAME)

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

# TODO unify concept of below pattern rule and $(OBJFILES)
# TODO do .h check too (this currently fails for main.cpp, since it has no .h)
$(BUILDDIR)/%.o: %.cpp
	$(CXX) $(CXXFLAGS) -std=$(CXXSTD) -c -o $@ $<

$(SHAREDLIB): $(LIBDIR)
	$(CXX) -shared $(CXXFLAGS) -std=$(CXXSTD) -o $(LIBDIR)/lib$(SHAREDLIB).so $(BUILDDIR)/$(SHAREDLIB).o

$(LIBDIR):
	mkdir -p $(LIBDIR)


run:
	$(BUILDDIR)/$(BINNAME)

clean:
	rm -rf $(BUILDDIR) $(BINNAME) $(LIBDIR)
