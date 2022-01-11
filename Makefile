OBJS=src/main.c src/pargser.c
FLAGS=-Wall -Wextra -Wpedantic

./bin/main: ./bin $(OBJS)
	gcc $(FLAGS) $(OBJS) -o bin/main -lpthread

./bin:
	mkdir bin

run: ./bin/main
	./bin/main -d testdir -n 2

clean:
	rm -rf bin