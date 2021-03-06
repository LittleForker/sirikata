# load files from the tests directory and run each of them

ECHO="echo"
DIFF="diff"
RM="rm"


EMERSON_EXEC="../../../../../build/cmake/emerson -v -f"
TEST_DIR="./tests"
REF_DIR="./refs"

TESTS=`ls $TEST_DIR`


$RM *.em.out
$RM *.em.diff

for file in $TESTS
 do
   $EMERSON_EXEC "$TEST_DIR/$file" 2> "$file.out"
   $DIFF "$file.out" "$REF_DIR/$file.ref" > "$file.diff" 
   size=`stat -c%s "$file.diff"`
   if [ $size -ne 0 ]
      then
         $ECHO " $file FAILED" 
    else
      $ECHO "$file PASSED"
    fi

 done 


