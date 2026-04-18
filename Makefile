TARGET = Zipcord.exe
CC = ccache gcc

CFLAGS = -flto -fno-plt -ffunction-sections -fdata-sections -fno-ident -fstack-protector-strong -std=c17 -O3 -DSQLITE_THREADSAFE=0 -DSQLITE_DEFAULT_MEMSTATUS=0 -mwindows
SRC = main.c sqlite3.c SHA512.c
OBJS = $(SRC:.c=.o) 5.obj 4.obj
LIBS = -lgdi32 -luser32 -lshell32 -lmsimg32 -lssp
LDFLAGS = -Wl,--gc-sections -static-libgcc -static-libstdc++
all: $(TARGET)
$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS) $(LIBS)
	strip $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	@if exist $(TARGET) del /q $(TARGET)
	@if exist *.o del /q *.o

.PHONY: all run clean
