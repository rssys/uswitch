#include <stdio.h>
#include <string.h>
#include "mkdio.h"

#include "RLBox_DynLib.h"
#include "rlbox.h"

using namespace rlbox;


int main(int argc, char **argv) {
  RLBoxSandbox<RLBox_DynLib>* sandbox = RLBoxSandbox<RLBox_DynLib>::createSandbox("", "./libmarkdown.so");
  tainted<MMIOT*, RLBox_DynLib> doc;
  char *text = "### hi\nwhat's up!";
  doc = sandbox_invoke(sandbox, mkd_string, sandbox->stackarr(text), strlen(text), 0);
  if ( !doc ) {
    printf("fuck\n");
    return -1;
  }
  sandbox_invoke(sandbox, mkd_compile, doc, 0);

  char *title = sandbox_invoke(sandbox, mkd_doc_title, doc).UNSAFE_Unverified();
  printf("title: %s\n", title);
  {
    tainted<char**, RLBox_DynLib> docStr = sandbox->template mallocInSandbox<char*>();
    int szdoc;

    szdoc = sandbox_invoke(sandbox, mkd_document, doc, docStr).UNSAFE_Unverified();
    if (szdoc) {
      fwrite(docStr->UNSAFE_Unverified(), szdoc, 1, stdout);
    }
  }
  return 0;
}
