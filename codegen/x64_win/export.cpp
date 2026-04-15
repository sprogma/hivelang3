#include <print>
#include <chrono>
#include "codegen.hpp"

pair<BYTE *, BYTE *> WinX64Assembler::ExportToFile(BYTE *header, BYTE *body, int64_t bodyOffset)
{
    if (config.contains("x64-debug") || config.contains("debug"))
    {
        /* generate debug info file */
        FILE *f = fopen("./debug.x64.hdb", "wb");
        if (f == NULL)
        {
            printf("ERROR: can't open ./debug.x64.hdb to write debug info\n");
            exit(1);
        }
        print(f, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
        print(f, "<database>\n");
        print(f, "    <time>{:%FT%TZ}</time>\n", chrono::system_clock::now());
        print(f, "    <file>{}</file>\n", xml_encode(ir->filename));
        print(f, "    <source><![CDATA[{}]]></source>\n", ir->code);
        print(f, "    <addressToLine>\n");
        for (auto &[addr, line] : addrToLine)
        {
            print(f, "        <mapping>\n");
            print(f, "            <address start=\"{}\" end=\"{}\" />\n", addr.first + bodyOffset, addr.second + bodyOffset);
            print(f, "            <line start=\"{}\" end=\"{}\" />\n", line.start, line.end);
            print(f, "        </mapping>\n");
        }
        print(f, "    </addressToLine>\n");
        print(f, "    <workers>\n");
        for (auto &[id, pos] : resultWorkerPositions)
        {
            WorkerDeclarationContext *wk = idToWorker[id];
            print(f, "        <worker>\n");
            print(f, "            <address start=\"{}\" end=\"{}\" />\n", pos + bodyOffset, pos + resultWorkerSize[id] + bodyOffset);
            print(f, "            <line start=\"{}\" end=\"{}\" />\n", wk->code_start, wk->code_end);
            print(f, "        </worker>\n");
        }
        print(f, "    </workers>\n");
        print(f, "</database>\n");
        fclose(f);
    }

    /* add relocations */
    for (auto &[id, value] : runtimeApiHeader)
    {
        *header++ = id;
        *(uint64_t *)header = value.size();
        header += 8;
        for (auto &pos : value)
        {
            *(uint64_t *)header = (pos.position + bodyOffset);
            header += 8;
        }
    }
    /* add x64 workers positions */
    {
        *header++ = GetHeaderId(HEADER_X64_WORKERS);
        *(uint64_t *)header = resultWorkerPositions.size();
        header += 8;
        for (auto &[id, pos] : resultWorkerPositions)
        {
            printf("Export worker %lld(export id:%lld) of size %lld with offset %016llx\n", id, GetExportWorkerId(ir, id, "x64"), resultWorkerSize[id], pos + bodyOffset);
            /* export id */
            *(uint64_t *)header = GetExportWorkerId(ir, id, "x64");
            header += 8;
            /* export position */
            *(uint64_t *)header = pos + bodyOffset;
            header += 8;
            /* export size */
            *(uint64_t *)header = resultWorkerSize[id];
            header += 8;
            /* export input table size */
            *(uint64_t *)header = GetWorkerInputTableSize(id);
            header += 8;
            /* export affinity */
            *(uint64_t *)header = idToWorker[id]->attributes.contains("affinity") &&
                                  holds_alternative<string>(idToWorker[id]->attributes["affinity"]) ? 
                                    stoi(get<string>(idToWorker[id]->attributes["affinity"])) : -1;
            header += 8;
        }
    }
    /* add string table */
    {
        *header++ = GetHeaderId(HEADER_STRINGS_TABLE);
        // insert strings count
        *(int64_t *)header = ir->strings.size();
        header += 8;
        // insert encoding [0x0=RAW]
        *header++ = 0x0;
        // push data using encoding:
        int64_t total_size = 0;
        for (auto &k : ir->strings)
        {
            total_size += k.size();
            total_size += (8-(total_size)%8)%8;
        }
        *(int64_t *)header = total_size;
        header += 8;
        // table [offset+size] + raw strings
        int64_t of = 0;
        for (auto &k : ir->strings)
        {
            *(int64_t *)header = of;
            header += 8;
            *(int64_t *)header = k.size();
            header += 8;
            of += k.size();
            of += (8-(of)%8)%8;
        }
        BYTE *begin = header;
        for (auto &k : ir->strings)
        {
            memcpy(header, k.data(), k.size());
            header += k.size();
            header += (8-(header-begin)%8)%8;
        }
        printf("Exported %lld strings used in sum %lld bytes\n", ir->strings.size(), of);
    }
    
    memcpy(body, assemblyCode, assemblyEnd - assemblyCode);
    body += assemblyEnd - assemblyCode;
    return {header, body};
}


