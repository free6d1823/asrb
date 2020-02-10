TARGET_NAME = asrb
OBJS =   main.o asrb_reader.o asrb_writer.o client.o server.o  asrb_int.o \
         inifile.o

ifeq ($(BUILD_MODE),debug)
	CFLAGS += -g
else 
	CFLAGS += -O2
endif

CC:= $(CROSS_COMPILE)gcc
CXX :=$(CROSS_COMPILE)g++
INCLUDE_PATH = -I./
CFLAGS += -std=c++11 $(INCLUDE_PATH)

LIB_PATH = 
LIB_LINK = -lpthread
LFLAGS += $(LIB_PATH) $(LIB_LINK)  


all:	$(TARGET_NAME)

$(TARGET_NAME):	$(OBJS) asrb_int.h
	$(CXX) $^ -o $@ $(LFLAGS)	
	mkdir -p $(PROJECT_ROOT)bin
	cp $@ $(PROJECT_ROOT)bin/.

%.o:	$(PROJECT_ROOT)%.cpp
	$(CXX) -c $(CFLAGS) $(CXXFLAGS) $(CPPFLAGS) -o $@ $<

%.o:	$(PROJECT_ROOT)%.c
	$(CC) -c $(CFLAGS) $(CPPFLAGS) -o $@ $<

clean:
	rm -fr $(TARGET_NAME) $(OBJS) 
