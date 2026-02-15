#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <vector>
#include <algorithm>
#include <array>
#include <ranges>
#include <map>

using namespace std;


#include "../../ir.hpp"
#include "../../logger.hpp"
#include "../../optimization/optimizer.hpp"
#include "../../analysis/analizators.hpp"
#include "../codegen.hpp"
#include "../registerAllocator.hpp"
#include "../memoryAllocator.hpp"


class WinDLLAssembler : public CodeAssembler
{
public:
    WinDLLAssembler()
    {}

private:
    map<int64_t, WorkerDeclarationContext *> idToWorker;
    BuildResult *ir;
    BYTE *assemblyCode, *assemblyEnd;
    int64_t assemblyAlloc;
    int64_t nextLabelId;

public:
    map<int64_t, int64_t> resultWorkerPositions;
    
    pair<BYTE *, BYTE *> Build(BuildResult *input, BYTE *header, BYTE *body, int64_t bodyOffset) override 
    {
        ir = input;
        /* init build context */
        nextLabelId = 0;

        /* initializate string */
        assemblyAlloc = 0;
        assemblyCode = assemblyEnd = (BYTE *)malloc(1024 * 1024);

        /* build each worker */
        for (auto &[fn, id] : ir->workers) { idToWorker[id] = fn; }
        for (auto &[fn, id] : ir->workers) { if (fn->used_providers.contains("dll")){BuildFn(fn, id);} }

        return ExportToFile(header, body, bodyOffset);
    }

private:
    // worker id -> vector of input sizes + output size
    struct dll_import_entry
    {
        int64_t output;
        vector<tuple<int64_t, BYTE, int64_t, int64_t>> inputs;
        string library;
        string entry;
    };
    map<int64_t, dll_import_entry> dllImportHeader;

    int64_t GetClassSize(TypeContext *type)
    {
        int64_t sum = 0;
        for (auto &i : type->_struct.fields)
        {
            sum += i->size;
        }
        return sum;
    }

    void BuildFn(WorkerDeclarationContext *wk, int64_t workerId)
    {
        if (wk->content != NULL)
        {
            logError(ir->filename, ir->code, wk->code_start, wk->code_end, "DLL provider function can't have body");
            return;
        }
        
        // if this function is extern, create it's header
        if (!wk->attributes.contains("dllimport") || !holds_alternative<string>(wk->attributes["dllimport"]))
        {
            logError(ir->filename, ir->code, wk->code_start, wk->code_end, "DLL provider function have no dllimport attribute");
            return;
        }

        string &lib = get<string>(wk->attributes["dllimport"]);
        string entry = (wk->attributes.contains("dllimport.entry") && holds_alternative<string>(wk->attributes["dllimport.entry"])
                       ? get<string>(wk->attributes["dllimport.entry"]) : wk->name);
        if (wk->outputs.size() > 1)
        {
            logError(ir->filename, ir->code, wk->code_start, wk->code_end, "DLLIMPORT function have more than 1 return argument");
            return;
        }
        if (!wk->outputs.empty() && wk->outputs[0].second->type != TYPE_PROMISE)
        {
            logError(ir->filename, ir->code, wk->code_start, wk->code_end, "DLLIMPORT function's return argument isn't promise");
            return;
        }
        int64_t out_size = (wk->outputs.empty() ? -1 : wk->outputs[0].second->_vector.base->size);
        // generate parameter sizes
        vector<tuple<int64_t, BYTE, int64_t, int64_t>> sizes;
        map<string, int64_t> inputIds;
        int64_t id = 0;
        for (auto &[name, type] : wk->inputs)
        {
            inputIds[name] = id++;
            switch (type->type)
            {
                case TYPE_ARRAY:
                    sizes.emplace_back(ProviderId(type->provider), 0, type->size, type->_vector.base->size);
                    break;
                case TYPE_PROMISE:
                    sizes.emplace_back(ProviderId(type->provider), 1, type->size, type->_vector.base->size);
                    break;
                case TYPE_CLASS:
                    sizes.emplace_back(ProviderId(type->provider), 1, type->size, GetClassSize(type));
                    break;
                case TYPE_RECORD:
                case TYPE_UNION:
                case TYPE_SCALAR:
                    sizes.emplace_back(ProviderId(type->provider), 2, type->size, type->size);
                    break;
                case TYPE_PIPE:
                    logError(ir->filename, ir->code, wk->code_start, wk->code_end, "pipes as DLLIMPORT function argument are unsupported");
                    break;
            }
        }
        
        if (wk->attributes.contains("dllimport.out") && holds_alternative<string>(wk->attributes["dllimport.out"]))
        {
            for (auto token : views::split(get<string>(wk->attributes["dllimport.out"]), ',')) 
            {
                get<1>(sizes[inputIds[(string){from_range, token}]]) |= 0x10;
            }
        }
        
        // for now, there is no extra marshalling
        dllImportHeader[workerId] = {out_size, sizes, lib, entry};
        return;
    }

    pair<BYTE *, BYTE *> ExportToFile(BYTE *header, BYTE *body, int64_t bodyOffset)
    {
        (void)bodyOffset;
        
        /* add dll import data */
        for (auto &[id, data] : dllImportHeader)
        {
            printf("Export dllimport data for worker %lld [export id=%lld] [%s %s]\n", id, GetExportWorkerId(ir, id, "dll"), data.library.c_str(), data.entry.c_str());
            *header++ = GetHeaderId(HEADER_DLL_IMPORT);
            /* export id */
            *(uint64_t *)header = GetExportWorkerId(ir, id, "dll");
            header += 8;
            /* export library name */
            *(uint64_t *)header = data.library.size();
            header += 8;
            memcpy(header, data.library.c_str(), data.library.size());
            header += data.library.size();
            /* export function name */
            *(uint64_t *)header = data.entry.size();
            header += 8;
            memcpy(header, data.entry.c_str(), data.entry.size());
            header += data.entry.size();
            /* export affinity */
            *(uint64_t *)header = idToWorker[id]->attributes.contains("affinity") &&
                                  holds_alternative<string>(idToWorker[id]->attributes["affinity"]) ? 
                                      stoi(get<string>(idToWorker[id]->attributes["affinity"])) : -1;
            header += 8;
            /* export output size [base from promise] */
            *(uint64_t *)header = data.output;
            header += 8;
            /* export inputs count */
            *(uint64_t *)header = data.inputs.size();
            header += 8;
            /* export inputs */
            for (auto &[provider, type, size, param] : data.inputs)
            {
                printf("    dllarg [prov=%lld: %lld %02x, param=%lld]\n", provider, size, type, param);
                *header++ = type;
                *(uint64_t *)header = provider;
                header += 8;
                *(uint64_t *)header = size;
                header += 8;
                *(uint64_t *)header = param;
                header += 8;
            }
        }
        
        return {header, body};
    }
};


CodeAssembler *new_DLL_Assembler()
{
    return new WinDLLAssembler();
}
