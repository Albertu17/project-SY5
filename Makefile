CC = gcc
CFLAGS = -Wall
EXEC = jsh

build:
	$(CC) -g $(CFLAGS) -c *.c
	$(CC) -g $(CFLAGS) -o $(EXEC) *.o -lreadline

test: build
	TEST_FOLDER_FILTER=jalon-1 ./test.sh
	# TEST_FOLDER_FILTER=jalon-2 ./test.sh
	# TEST_FOLDER_FILTER=jalon-2-A ./test.sh
	# TEST_FOLDER_FILTER=jalon-2-B ./test.sh
    # TEST_FOLDER_FILTER=rendu-final-A ./test.sh 
    # TEST_FOLDER_FILTER=rendu-final-B ./test.sh 
    # TEST_FOLDER_FILTER=rendu-final-C ./test.sh
    # TEST_FOLDER_FILTER=rendu-final-D ./test.sh 
    # TEST_FOLDER_FILTER=tests-extra ./test.sh

clean:
	rm -f *.o $(EXEC)