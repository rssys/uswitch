#include <stdio.h>
#include "mylib.h"
#include "RLBox_DynLib.h"
#include "rlbox.h"

using namespace rlbox;

int main(int argc, char const *argv[]) {
  printf("Hello world!\n");

  // Create sandbox
  RLBoxSandbox<RLBox_DynLib>* sandbox =
      RLBoxSandbox<RLBox_DynLib>::createSandbox("", "./mylib.so");

  // call the library hello function
  sandbox_invoke(sandbox, hello);

  // call the add function and check the result:
  auto ok = sandbox_invoke(sandbox, add, 3, 4).copyAndVerify([](int ret){
      printf("Adding... 3+4 = %d\n", ret);
      return ret == 7;
  });
  printf("OK? = %d\n", ok);

  // call the library echo function
  sandbox_invoke(sandbox, echo, sandbox->stackarr("hi hi!"));

  return 0;
}
