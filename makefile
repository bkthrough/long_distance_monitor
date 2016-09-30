ALL:cli ser
cli:cli.cpp utili.cpp be_controlled.cpp
	g++ cli.cpp utili.cpp be_controlled.cpp -o cli -g
ser:ser.cpp utili.cpp control.cpp
	g++ ser.cpp utili.cpp control.cpp -o ser -pthread -g

.PHONY:clean
clean:
	rm -f cli ser
