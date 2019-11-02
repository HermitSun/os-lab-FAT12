# compiler(for C++11)
cxx = g++ -g -Wall -std=c++11
# src codes
main_code = main.cpp
print_code = my_print.asm
# linked objects
print_obj = my_print.o
# generate
generate_files = $(print_obj)
generate_files += $(main_code)
# target
target = main

$(target): $(main_code) $(print_obj)
	$(cxx) -m32 -o $(target) $(main_code) $(print_obj)
$(print_obj): $(print_code)
	nasm -f elf32 -o $(print_obj) $(print_code)

.PHONY : clean
clean: 
	-rm $(print_obj) $(target)