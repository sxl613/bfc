##
# bfc
# end
#
bf: bf.cpp
	g++ -o bf bf.cpp -O3

@PHONY: clean
clean:
	rm ./bf
