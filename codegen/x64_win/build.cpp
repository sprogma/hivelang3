#include "codegen.hpp"

pair<BYTE *, BYTE *> WinX64Assembler::Build(BuildResult *input, BYTE *header, BYTE *body, int64_t bodyOffset)
{
    ir = input;
    /* init build context */
    nextLabelId = 0;
    addressTable.clear();

    /* initializate string */
    assemblyAlloc = 0;
    assemblyCode = assemblyEnd = (BYTE *)malloc(1024 * 1024);

    /* build each worker */
    for (auto &[fn, id] : ir->workers) { idToWorker[id] = fn; }
    for (auto &[fn, id] : ir->workers) { if (fn->used_providers.contains("x64")){BuildFn(fn, id);} }

    // add terminating zero
    printf("Code addressTable.\n");
    if (!assemblyEnd)
    {
        return {NULL, NULL};
    }

    for (BYTE *x = assemblyCode; x < assemblyEnd; ++x)
    {
        printf(" %02X", *x);
    }
    printf("\n");

    return ExportToFile(header, body, bodyOffset);
}

