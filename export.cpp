#include <vector>
#include <stdio.h>
using namespace std;


#include "ast.hpp"
#include "optimization/optimizer.hpp"
#include "codegen/codegen.hpp"
#include "ir.hpp"



pair<BYTE *, BYTE *> WriteEntryHeader(BuildResult *code, BYTE *header, BYTE *body, int64_t body_offset)
{
    (void)body_offset;
    (void)body;

    *header++ = GetHeaderId(HEADER_ENTRY_ID);
    int64_t entryId = -1;
    for (auto &[wk, id] : code->workers)
    {
        if (wk->attributes.contains("entry"))
        {
            entryId = GetExportWorkerId(code, id, "x64");
        }
    }
    if (entryId == -1)
    {
        printf("Error: no entry worker\n");
        return {header, body};
    }
    *(uint64_t *)header = entryId;
    header += 8;

    return {header, body};
}



int64_t ExportCode(BuildResult *code)
{
    /* generate workers for each provider */
    FILE *o = fopen("res.bin", "wb");
    

    BYTE *header = (BYTE *)malloc(1024 * 1024);
    BYTE *header_start = header;
    BYTE *body = (BYTE *)malloc(1024 * 1024);
    BYTE *body_start = body;


    /* generate prefix */
    *header++ = 'H'; *header++ = 'I'; *header++ = 'V'; *header++ = 'E';    
    *(uint64_t *)header = 3; // version 0.3
    header += 8;
    /* generate header */
    int64_t *header_size_writeback = (int64_t *)header;
    *(uint64_t *)header = 0xBEBEBEBEBEBEBEBE;
    header += 8;


    /* push entry id */
    tie(header, body) = WriteEntryHeader(code, header, body, body - body_start);

    
    for (auto &name : code->used_providers)
    {
        CodeAssembler *assembler;
        
        printf("Building for <%s>\n", name.c_str());
        if (name == "x64")
        { assembler = new_x64_Assembler(); }
        else if (name == "gpu")
        { assembler = new_gpu_Assembler(); }
        else if (name == "dll")
        { assembler = new_DLL_Assembler(); }
        else
        {
            printf("Error: UNKNOWN PROVIDER: %s\n", name.c_str());
            fclose(o);
            free(header_start);
            free(body_start);
            return 1;
        }
        tie(header, body) = assembler->Build(code, header, body, body - body_start);
        delete assembler;
    }


    /* fill header size */
    *header_size_writeback = header - header_start;

    int64_t totalBytes = (header - header_start) + (body - body_start);
    fwrite(header_start, 1, header - header_start, o);
    fwrite(body_start, 1, body - body_start, o);


    fclose(o);
    free(header_start);
    free(body_start);

    printf("%lld bytes written\n", totalBytes);
    printf("res.bin file generated\n");

    return 0;
}
