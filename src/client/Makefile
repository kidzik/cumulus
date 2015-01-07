all:
	$(MAKE) -C ../common/
	g++ -static scanner.cpp ../common/libtransfer.a -o client -lssl -lcrypto

