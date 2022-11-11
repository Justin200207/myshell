CFLAGS = -g -Wall -Werror -fsanitize=address

all: mysh

mysh: mysh.o builtins.o commands.o variables.o io_helpers.o 
	gcc ${CFLAGS} -o $@ $^ 

%.o: %.c builtins.h commands.h variables.h io_helpers.h 
	gcc ${CFLAGS} -c $< 

clean:
	rm *.o mysh
