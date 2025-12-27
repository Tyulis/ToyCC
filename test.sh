#!/bin/bash

TOYCC_ROOT=$(pwd)

# Build the compiler
mkdir -p build/
cd build/
cmake ..
make -j$(nproc)
cd ${TOYCC_ROOT}

# Run the test suite
TESTSUITE_DIR=${TOYCC_ROOT}/lib/c-testsuite/tests/single-exec
TEST_LOG=${TOYCC_ROOT}/build/tests.log
TEST_SUMMARY=${TOYCC_ROOT}/build/tests.summary
CC=${TOYCC_ROOT}/build/toycc
NOF_TESTS_RUN=0
NOF_TESTS_OK=0
NOF_TESTS_KO=0

echo "" > ${TEST_SUMMARY}
echo "" > ${TEST_LOG}
rm ${TESTSUITE_DIR}/*.bin
rm ${TESTSUITE_DIR}/*.out

for test_source in $(find ${TESTSUITE_DIR} -iname '*.c') ; do
    test_tags=${test_source}.tags
    test_compiled=${test_source}.bin
    test_output=${test_source}.out
    test_expected_output=${test_source}.expected

    if ! cat ${test_tags} | grep -qE "c89|c99|c11" ; then
        continue
    fi

    NOF_TESTS_RUN=$(($NOF_TESTS_RUN + 1))

    case $1 in
        --parse-ir)
            CMD="$CC --parse-ir -o ${test_compiled} ${test_source}"
            echo "" >> ${TEST_LOG}
            echo ${CMD} >> ${TEST_LOG}
            if ${CMD} >> ${TEST_LOG} 2>&1 && [ -f ${test_compiled} ] ; then
                echo "TEST CASE $(basename ${test_source}) : OK (generated IR)" | tee -a ${TEST_SUMMARY}
                NOF_TESTS_OK=$(($NOF_TESTS_OK + 1))
            else
                echo "TEST CASE $(basename ${test_source}) : KO (error)" | tee -a ${TEST_SUMMARY}
                NOF_TESTS_KO=$(($NOF_TESTS_KO + 1))
                rm ${test_compiled}
            fi
            ;;
    esac
done

echo "${NOF_TESTS_RUN} test cases run" | tee -a ${TEST_SUMMARY}
echo "${NOF_TESTS_OK} test cases OK" | tee -a ${TEST_SUMMARY}
echo "${NOF_TESTS_KO} test cases KO" | tee -a ${TEST_SUMMARY}

