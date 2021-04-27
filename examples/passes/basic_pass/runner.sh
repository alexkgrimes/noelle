
output="run/output.txt"
rm $output
touch $output
echo "test_name, malloc_time, jemalloc_time" &> $output

#go to tests folder
cd ../../../alex/basic;
output="../../examples/passes/basic_pass/run/output.txt"

for filename in *.c; do

    ex=${filename%.*}
    echo -n "$ex, " >> $output

    # compile program with malloc
    clang -o "$ex" "$filename" -pthread;

    # run program with malloc
    perf stat ./$ex 2>&1 >/dev/null | tail -n 6 | sed 's/ \+//' | sed 's/ /, /' | head -n1 | sed -e 's/\s.*$//'| awk '{printf("%s ", $0)}' >> $output

    # go to the compiler dir
    cd ../../examples/passes/basic_pass/run;
    output="output.txt";

    # compile to bitcode
    clang -emit-llvm -o "$ex".bc -c ../../../../alex/basic/"$filename" -pthread;

    # noelle
    noelle-norm "$ex".bc -o "$ex".bc;   
    noelle-load -load ~/CAT/lib/CAT.so -CAT -jemalloc "$ex".bc -o "$ex"-jemalloc.bc;

    # create object file
    llc -filetype=obj "$ex"-jemalloc.bc -o "$ex"-jemalloc.o;

    # link dependencies
    clang "$ex"-jemalloc.o -o "$ex"-jemalloc -I`jemalloc-config --includedir` \
    -L`jemalloc-config --libdir` -Wl,-rpath,`jemalloc-config --libdir` \
    -ljemalloc `jemalloc-config --libs`;

    perf stat ./"$ex"-jemalloc 2>&1 >/dev/null | tail -n 6 | sed 's/ \+//' | sed 's/ / ,/' | head -n1 | sed -e 's/\s.*$//' >> $output

    cd ../../../../alex/basic;
    output="../../examples/passes/basic_pass/run/output.txt"

done

cd ../../examples/passes/basic_pass/run;

# compile all programs to bitcode
# for filename in *.c; do
#     clang -emit-llvm -o "$filename".bc -c ../../../../alex/basic/"$filename" -pthread;
# done

# noelle-norm malloc_test.bc -o norm-ir.bc;
# noelle-load -load /home/akg434/CAT/lib/CAT.so -CAT norm-ir.bc -o jemalloc.bc;
# llc -filetype=obj jemalloc.bc -o jemalloc.o;
# clang jemalloc.o -o jemalloc -I`jemalloc-config --includedir` \
# -L`jemalloc-config --libdir` -Wl,-rpath,`jemalloc-config --libdir` \
# -ljemalloc `jemalloc-config --libs`;

# perf stat ./jemalloc;