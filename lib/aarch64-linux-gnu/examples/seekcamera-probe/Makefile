INCFLAG   += -I./include -I/usr/local/include/seekcamera/
DEBUGFLAG = -g -O0 -Wall
CFLAGS    += $(DEBUGFLAG) $(INCFLAG) `pkg-config --cflags`
LDFLAGS   += -lseekcamera
CXXFLAGS  += $(CFLAGS)
CPPFLAGS  += $(CFLAGS)

TARGET  = seekcamera-probe
OBJECTS	= objs/seekcamera-probe.o

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) -o $@ $(OBJECTS) $(LDPATH) $(LDFLAGS)

objs/%.o:	src/%.cpp
	@if test ! -e objs; then \
		mkdir objs  ;\
	fi;
	$(CXX) $(CXXFLAGS) -c $< -o $@

objs/%.o:	src/%.c
	@if test ! -e objs; then \
		mkdir objs  ;\
	fi;
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	@rm -rf core *~  $(TARGET) $(OBJECTS) $(LIB) $(LIBOBJ) src/*~ include/*~

