
SOURCES = main \
	  bit_operation

OBJS=$(addsuffix .o, $(SOURCES))

CFLAGS += -mcpu=cortex-a9 

CXXFLAGS += 

TARGET=bit_oper

CC = $(CROSS_COMPILE)gcc
CXX = $(CROSS_COMPILE)g++
AS = $(CROSS_COMPILE)as

.SUFFIXES : .o .c
%.o : %.c
	$(CC) $(CFLAGS) $(LDLIBS) -c -o $@ $<

.SUFFIXES : .o .cpp
%.o : %.cpp
	$(CXX) $(CXXFLAGS) $(LDLIBS) -c -o $@ $<

.SUFFIXES : .o .S
%.o : %.S
	$(CC) $(CFLAGS) $(LDLIBS) -c -o $@ $<

all : $(TARGET)

$(TARGET) : $(OBJS)
	$(CC) $(OBJS) -o $@

clean:
	rm -f $(OBJS) $(TARGET)
