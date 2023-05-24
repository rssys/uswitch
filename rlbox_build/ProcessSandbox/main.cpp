#include <stdint.h>
#include <stdio.h>
#include <chrono>
#include "ProcessSandbox.h"
#include "myHelpers.h"
#include "timing.h"

// our sample dynamic library
#include "library.h"

//#define RDTSC_FREQ_MHZ 2200
#define NUM_RUNS 1000

#define ASSERT(cond, ...) \
  if(!(cond)) { \
    printf("Assertion failed: "); \
    printf(__VA_ARGS__); \
    exit(1); \
  }

__attribute__((noinline)) static int myCallback0(int x) {
  return x * 100;
}

__attribute__((noinline)) static void myCallback1(char a, char b) {
  volatile int c = a * b;
}

__attribute__((noinline)) static void* myCallback2(int* i) {
  return (void*) (i+143);
}

int main(int argc, char* argv[]) {
  if(argc < 5) ERROR("Got too few arguments, expected four:\n  (1) the path to the ProcessSandbox_otherside executable\n  (2) the core to pin the main process to\n  (3) the core to pin the sandbox process(es) to\n  (4) number of sandboxes to use\n\n")
  if(argc > 5) ERROR("Got too many arguments, expected four:\n  (1) the path to the ProcessSandbox_otherside executable\n  (2) the core to pin the main process to\n  (3) the core to pin the sandbox process(es) to\n  (4) number of sandboxes to use\n\n")
	unsigned maincore, sbcore, numSboxes;
	if(!sscanf(argv[2], "%u", &maincore)) ERROR("Bad argument 2\n")
	if(!sscanf(argv[3], "%u", &sbcore)) ERROR("Bad argument 3\n")
  if(!sscanf(argv[4], "%u", &numSboxes)) ERROR("Bad argument 4\n")
  if(numSboxes == 0) ERROR("number of sandboxes cannot be 0\n")

  std::vector<int64_t> sandboxCreationTimes;
  std::vector<int64_t> baselineTimes;
  std::vector<int64_t> inactiveTimes;
  std::vector<int64_t> activeTimes;
  std::vector<int64_t> withActivationTimes;
  std::vector<int64_t> consistentlyActiveTimes;
  std::vector<int64_t> consistentlyInactiveTimes;

    for(unsigned i = 0; i < 1000; i++) {
    std::chrono::steady_clock::time_point start = startTimer();
    auto junksb = new DummyProcessSandbox(argv[1], maincore, sbcore);
    sandboxCreationTimes.push_back(diffInNanoseconds(start, stopTimer()));
    junksb->destroySandbox();
   }


  DummyProcessSandbox** sb =  new DummyProcessSandbox*[numSboxes];  // array to hold pointers to the sandboxes
  for(unsigned i = 0; i < 10; i++) static auto junk = std::chrono::steady_clock::now();  // warmup timer
  for(unsigned i = 0; i < numSboxes; i++) {
    sb[i] = new DummyProcessSandbox(argv[1], maincore, sbcore);
  }

/*
  LIB::CB_TYPE_0 myCallback0_registered = sb->registerCallback<LIB::CB_TYPE_0>(myCallback0, NULL);
  LIB::CB_TYPE_1 myCallback1_registered = sb->registerCallback<LIB::CB_TYPE_1>(myCallback1, NULL);
  LIB::CB_TYPE_2 myCallback2_registered = sb->registerCallback<LIB::CB_TYPE_2>(myCallback2, NULL);
*/

  for(unsigned i = 0; i < NUM_RUNS; i++) {
    printf("\rProgress: Step 1 %4.1f%% ...", 100.0*(float)i/(float)NUM_RUNS); fflush(stdout);
    std::chrono::steady_clock::time_point start;

    int retvalZero_base, retvalZero_sb;
    start = startTimer();
    retvalZero_base = APIFuncZero(3, 45);
    baselineTimes.push_back(diffInNanoseconds(start, stopTimer()));
    for(unsigned j = 0; j < numSboxes; j++) {
      start = startTimer();
      retvalZero_sb = sb[j]->inv_APIFuncZero(3, 45);
      inactiveTimes.push_back(diffInNanoseconds(start, stopTimer()));
      ASSERT(retvalZero_base == retvalZero_sb, "FuncZero: base %i != sb %i\n", retvalZero_base, retvalZero_sb)

      sb[j]->makeActiveSandbox();
      start = startTimer();
      retvalZero_sb = sb[j]->inv_APIFuncZero(3, 45);
      activeTimes.push_back(diffInNanoseconds(start, stopTimer()));
      ASSERT(retvalZero_base == retvalZero_sb, "FuncZero: base %i != sb %i\n", retvalZero_base, retvalZero_sb)
      sb[j]->makeInactiveSandbox();

      start = startTimer();
      sb[j]->makeActiveSandbox();
      retvalZero_sb = sb[j]->inv_APIFuncZero(3, 45);
      sb[j]->makeInactiveSandbox();
      withActivationTimes.push_back(diffInNanoseconds(start, stopTimer()));
      ASSERT(retvalZero_base == retvalZero_sb, "FuncZero: base %i != sb %i\n", retvalZero_base, retvalZero_sb)
    }

  /*
    float retvalOne_base, retvalOne_sb;
    start = startTimer();
    retvalOne_base = APIFuncOne(5);
    baselineTimes.push_back(diffInNanoseconds(start, stopTimer()));
    start = startTimer();
    retvalOne_sb = sb->inv_APIFuncOne(5);
    times.push_back(diffInNanoseconds(start, stopTimer()));
    ASSERT(retvalOne_base == retvalOne_sb, "FuncOne: base %f != sb %f\n", retvalOne_base, retvalOne_sb)

    void *retvalTwo_base, *retvalTwo_sb;
    start = startTimer();
    retvalTwo_base = APIFuncTwo(7, -23);
    baselineTimes.push_back(diffInNanoseconds(start, stopTimer()));
    start = startTimer();
    retvalTwo_sb = sb->inv_APIFuncTwo(7, -23);
    times.push_back(diffInNanoseconds(start, stopTimer()));
    ASSERT(retvalTwo_base == retvalTwo_sb, "FuncTwo: base %p != sb %p\n", retvalTwo_base, retvalTwo_sb)

    int* arr = (int*)sb->mallocInSandbox(3*sizeof(int));
    if(!arr) ERROR("Not enough space in sandbox\n")
    arr[0] = 99;
    arr[1] = 98;
    arr[2] = 97;

    bool retvalThree_base, retvalThree_sb;
    start = startTimer();
    retvalThree_base = APIFuncThree(arr);
    baselineTimes.push_back(diffInNanoseconds(start, stopTimer()));
    ASSERT(arr[0] == arr[2], "APIFuncThree")
    arr[2] = 13;
    start = startTimer();
    retvalThree_sb = sb->inv_APIFuncThree(arr);
    times.push_back(diffInNanoseconds(start, stopTimer()));
    ASSERT(arr[0] == arr[2], "inv_APIFuncThree")
    arr[2] = 13;
    ASSERT(retvalThree_base == retvalThree_sb, "FuncThree: base %i != sb %i\n", retvalThree_base, retvalThree_sb)

    sb->freeInSandbox(arr);

    int retvalFour_base, retvalFour_sb;
    start = startTimer();
    retvalFour_base = APIFuncFour(2, myCallback0);
    baselineTimes.push_back(diffInNanoseconds(start, stopTimer()));
    start = startTimer();
    retvalFour_sb = sb->inv_APIFuncFour(2, myCallback0_registered);
    times.push_back(diffInNanoseconds(start, stopTimer()));
    ASSERT(retvalFour_base == retvalFour_sb, "FuncFour: base %i != sb %i\n", retvalFour_base, retvalFour_sb)

    start = startTimer();
    APIFuncFive(123, myCallback1);
    baselineTimes.push_back(diffInNanoseconds(start, stopTimer()));
    start = startTimer();
    sb->inv_APIFuncFive(123, myCallback1_registered);
    times.push_back(diffInNanoseconds(start, stopTimer()));

    void *retvalSix_base, *retvalSix_sb;
    start = startTimer();
    retvalSix_base = APIFuncSix(myCallback2);
    baselineTimes.push_back(diffInNanoseconds(start, stopTimer()));
    start = startTimer();
    retvalSix_sb = sb->inv_APIFuncSix(myCallback2_registered);
    times.push_back(diffInNanoseconds(start, stopTimer()));
    ASSERT(retvalSix_base == retvalSix_sb, "FuncSix: base %p != sb %p\n", retvalSix_base, retvalSix_sb)
  */
  }

  for(unsigned i = 0; i < NUM_RUNS; i++) {
    printf("\rProgress: Step 2 %4.1f%% ...", 100.0*(float)i/(float)NUM_RUNS); fflush(stdout);
    std::chrono::steady_clock::time_point start;
    int retvalZero_sb;

    start = startTimer();
    retvalZero_sb = sb[0]->inv_APIFuncZero(3, 45);
    consistentlyInactiveTimes.push_back(diffInNanoseconds(start, stopTimer()));
  }

  sb[0]->makeActiveSandbox();
  for(unsigned i = 0; i < NUM_RUNS; i++) {
    printf("\rProgress: Step 3 %4.1f%% ...", 100.0*(float)i/(float)NUM_RUNS); fflush(stdout);
    std::chrono::steady_clock::time_point start;
    int retvalZero_sb;

    start = startTimer();
    retvalZero_sb = sb[0]->inv_APIFuncZero(3, 45);
    consistentlyActiveTimes.push_back(diffInNanoseconds(start, stopTimer()));
  }
  sb[0]->makeInactiveSandbox();

  printf("\r\n");
  for(unsigned i = 0; i < numSboxes; i++) sb[i]->destroySandbox();


  summary<int64_t> sandboxCreationSummary = summarize(&sandboxCreationTimes);
  summary<int64_t> baseSummary = summarize(&baselineTimes);
  summary<int64_t> inactiveSummary = summarize(&inactiveTimes);
  summary<int64_t> activeSummary = summarize(&activeTimes);
  summary<int64_t> withActivationSummary = summarize(&withActivationTimes);
  summary<int64_t> consistentlyInactiveSummary = summarize(&consistentlyInactiveTimes);
  summary<int64_t> consistentlyActiveSummary = summarize(&consistentlyActiveTimes);

#define MICROS(val) (double)(val)/(double)1000
  printf("\n\nRESULTS WITH **%u** SANDBOXES\n(All times in microseconds)\n\n", numSboxes);
  printf("Sandbox creation:\n");
  printf("Min  %-12.2f\n", MICROS(sandboxCreationSummary.min));
  printf("1Q   %-12.2f\n", MICROS(sandboxCreationSummary.firstQ));
  printf("Med  %-12.2f\n", MICROS(sandboxCreationSummary.median));
  printf("Mean %-12.2f\n", MICROS(sandboxCreationSummary.sum/sandboxCreationTimes.size()));
  printf("3Q   %-12.2f\n", MICROS(sandboxCreationSummary.thirdQ));
  printf("Max  %-12.2f\n", MICROS(sandboxCreationSummary.max));
  printf("\n");
  printf("Case A: Without sandbox (baseline). All other cases are sandboxed.\n");
  printf("Case B: All inactive, round-robin\n");
  printf("Case C: Round-robin, each sandbox is activated before timer is started\n");
  printf("Case D: Round-robin, time includes activation + deactivation of the sandbox\n");
  printf("Case E: All inactive, one sandbox receives all the calls\n");
  printf("Case F: One *active* sandbox receives all the calls, others inactive\n");
  printf("     Case A       | Case B\n");
  printf("Min  %-12.2f | %-12.2f\n",
      MICROS(baseSummary.min),
      MICROS(inactiveSummary.min));
  printf("1Q   %-12.2f | %-12.2f\n",
      MICROS(baseSummary.firstQ),
      MICROS(inactiveSummary.firstQ));
  printf("Med  %-12.2f | %-12.2f\n",
      MICROS(baseSummary.median),
      MICROS(inactiveSummary.median));
  printf("Mean %-12.2f | %-12.2f\n",
      MICROS(baseSummary.sum/baselineTimes.size()),
      MICROS(inactiveSummary.sum/inactiveTimes.size()));
  printf("3Q   %-12.2f | %-12.2f\n",
      MICROS(baseSummary.thirdQ),
      MICROS(inactiveSummary.thirdQ));
  printf("Max  %-12.2f | %-12.2f\n",
      MICROS(baseSummary.max),
      MICROS(inactiveSummary.max));
  printf("     Case C       | Case D\n");
  printf("Min  %-12.2f | %-12.2f\n",
      MICROS(activeSummary.min),
      MICROS(withActivationSummary.min));
  printf("1Q   %-12.2f | %-12.2f\n",
      MICROS(activeSummary.firstQ),
      MICROS(withActivationSummary.firstQ));
  printf("Med  %-12.2f | %-12.2f\n",
      MICROS(activeSummary.median),
      MICROS(withActivationSummary.median));
  printf("Mean %-12.2f | %-12.2f\n",
      MICROS(activeSummary.sum/activeTimes.size()),
      MICROS(withActivationSummary.sum/withActivationTimes.size()));
  printf("3Q   %-12.2f | %-12.2f\n",
      MICROS(activeSummary.thirdQ),
      MICROS(withActivationSummary.thirdQ));
  printf("Max  %-12.2f | %-12.2f\n",
      MICROS(activeSummary.max),
      MICROS(withActivationSummary.max));
  printf("     Case E       | Case F\n");
  printf("Min  %-12.2f | %-12.2f\n",
      MICROS(consistentlyInactiveSummary.min),
      MICROS(consistentlyActiveSummary.min));
  printf("1Q   %-12.2f | %-12.2f\n",
      MICROS(consistentlyInactiveSummary.firstQ),
      MICROS(consistentlyActiveSummary.firstQ));
  printf("Med  %-12.2f | %-12.2f\n",
      MICROS(consistentlyInactiveSummary.median),
      MICROS(consistentlyActiveSummary.median));
  printf("Mean %-12.2f | %-12.2f\n",
      MICROS(consistentlyInactiveSummary.sum/consistentlyInactiveTimes.size()),
      MICROS(consistentlyActiveSummary.sum/consistentlyActiveTimes.size()));
  printf("3Q   %-12.2f | %-12.2f\n",
      MICROS(consistentlyInactiveSummary.thirdQ),
      MICROS(consistentlyActiveSummary.thirdQ));
  printf("Max  %-12.2f | %-12.2f\n",
      MICROS(consistentlyInactiveSummary.max),
      MICROS(consistentlyActiveSummary.max));

  return 0;
}
