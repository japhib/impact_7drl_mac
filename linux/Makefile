TARGET = impact_bin

OFILES = ../ai.o \
	 ../buf.o \
	 ../builder.o \
	 ../chooser.o \
	 ../config.o \
	 ../dircontrol.o \
	 ../display.o \
	 ../disunion.o \
	 ../dpdf.o \
	 ../engine.o \
	 ../firefly.o \
	 ../gamedef.o \
	 ../gfxengine.o \
	 ../glbdef.o \
	 ../grammar.o \
	 ../indextree.o \
	 ../item.o \
	 ../level.o \
	 ../main.o \
	 ../map.o \
	 ../mob.o \
	 ../msg.o \
	 ../panel.o \
	 ../party.o \
	 ../rand.o \
	 ../scrpos.o \
	 ../speed.o \
	 ../text.o \
	 ../thesaurus.o \
	 ../thread.o \
	 ../thread_std.o

.cpp.o:
	g++ -O3 -DLINUX -I../lib/libtcod-1.15.1_build/include/libtcod -c $< -o $@

all: $(TARGET)

$(TARGET): $(OFILES)
	g++ -O3 -o $(TARGET) $(OFILES) -L../lib/libtcod-1.15.1_build/lib -ltcod -lpthread

clean:
	rm -f ../*.o
	rm -f $(TARGET)

premake:
	cd .. ; python ./support/enummaker/enummaker.py
