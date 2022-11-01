CFLAGS = -g

echo: echo.o

.PHONY: clean
clean:
	rm -f *.o echo
