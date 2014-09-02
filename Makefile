PROGRAMS := loadgen ZombieMaster.jar 
DOCS     := loadgen.html zombiemaster.html

OBJDIR   := objs
CLASSDIR := classes
DIRS	 := $(OBJDIR) $(CLASSDIR)

ALL  := $(DIRS) $(PROGRAMS) $(DOCS)

.PHONY:	all
all:	$(ALL)

#
# loadgen is C++
#
CSRCDIR  := cpp_src
CSOURCES := \
	main.cpp		\
	copydata.cpp		\
	createdata.cpp		\
	verifydata.cpp		\
	threadstatus.cpp	\
	report.cpp		\
	command.cpp		\
	pattern.cpp		\
	timedio.cpp		\
	checkdir.cpp

make_objs = \
	$(addprefix $(OBJDIR)/,$(patsubst %.cpp,%.o,$(patsubst %.c,%.o,$(1))))

OBJS := $(call make_objs,$(CSOURCES))

INCLUDE := 
CFLAGS := -Werror -Wall -g $(INCLUDE)
$(OBJDIR)/%.o: $(CSRCDIR)/%.cpp
	g++ -c $(CFLAGS) -o$@ $<

$(OBJDIR):
	mkdir $@

loadgen: $(OBJS)
	g++ -o $@ $^ -lpthread
	@echo ... successfully built $@
	@echo

#
# Zombiemaster is Java
#
JAVAC := javac
JAR   := jar

JSRCDIR  := java_src
JSOURCES := \
	$(JSRCDIR)/ZombieMaster.java	\
	$(JSRCDIR)/Console.java		\
	$(JSRCDIR)/ZombieListener.java	\
	$(JSRCDIR)/Zombie.java		\
	$(JSRCDIR)/Horde.java		\
	$(JSRCDIR)/PlotPoints.java	\
	$(JSRCDIR)/BarGraph.java	\
	$(JSRCDIR)/Argument.java	\
	$(JSRCDIR)/Options.java		\
	$(JSRCDIR)/ZombieApplet.java

JIMAGES := \
	$(JSRCDIR)/zombie.jpg

HORDE := \
	$(JSRCDIR)/horde.cfg

$(CLASSDIR):
	mkdir $@

make_classes = \
	$(addprefix $(CLASSDIR)/,$(patsubst %.java,%.class,$(notdir $(1))))

CLASSES := $(call make_classes,$(JSOURCES))

$(CLASSES): $(JSOURCES) classes $(JIMAGES)
	$(JAVAC) -d $(CLASSDIR) $(JSOURCES)
	cp $(JIMAGES) $(CLASSDIR)
	cp $(HORDE) $(CLASSDIR)

#
# If I were more clever and had a little more time I'd figure out how
# to write a generic rule to do this
#
#%.class: %.java
#	javac -s $(SRC) -d $(BIN) $<

ZombieMaster.jar: $(CLASSES) $(JIMAGES)
	$(JAR) cvfm $@ $(JSRCDIR)/manifest.txt -C $(CLASSDIR) .
	@echo ... successfully built $@
	@echo


DOCDIR := Docs
%.html: $(DOCDIR)/%.1
	groff -man -Thtml $^ > $@

.PHONY: test
test:
	cd Tests; bash RunTests.sh

.PHONY: clean
clean:
	rm -f $(OBJS) $(CLASSDIR)/*

.PHONY :clobber
clobber:
	rm -f $(PROGRAMS) $(DOCS)
	rm -rf $(DIRS)
