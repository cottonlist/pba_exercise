/* The objective of this program is to identify read and write instructions in target program 
 * and corresponding accessed memory addresses. 
 * Version 1.0
 */

#include <stdio.h>
#include "pin.H"
using std::string;

KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "memory_access_2.log", "specify output file name");

class access {
public:
	int threadId;
	void *ip;
	int mode;
	void *addr;
};

static access accesses[2000];
static int i=0;

//==============================================================
//  Analysis Routines
//==============================================================
PIN_LOCK pinLock;
FILE *out;

// This routine is executed every time a thread is created.
VOID ThreadStart(THREADID threadid, CONTEXT *ctxt, INT32 flags, VOID *v){
	PIN_GetLock(&pinLock, threadid+1);
	fprintf(out, "thread begin %d\n", threadid);
	fflush(out);
	PIN_ReleaseLock(&pinLock);
}

// This routine is executed every time a thread is destroyed.
VOID ThreadFini(THREADID threadid, const CONTEXT *ctxt, INT32 code, VOID *v){
	PIN_GetLock(&pinLock, threadid+1);
	fprintf(out, "thread end %d, code %d\n", threadid, code);
	fflush(out);
	PIN_ReleaseLock(&pinLock);
}

// This routine is executed to check if the instruction writes memory. 
VOID RecordMemRead(VOID *ip, VOID *addr, THREADID threadid){
	PIN_GetLock(&pinLock, threadid+1);
	if (threadid == 1 || threadid == 2)
	{
		accesses[i].threadId = threadid;
		accesses[i].ip = ip;
		accesses[i].mode = 1;
		accesses[i].addr = addr;
		printf("%d, %p, %d, %p\n", accesses[i].threadId, accesses[i].ip, accesses[i].mode, accesses[i].addr);
		i++;
		fprintf(out, "thread id %d, %p: memory read, address %p\n", threadid, ip, addr);
		fflush(out);
	}
	PIN_ReleaseLock(&pinLock);
}
 // This routine is executed to check if the instruction reads memory. 
VOID RecordMemWrite(VOID *ip, VOID *addr, THREADID threadid){
	PIN_GetLock(&pinLock, threadid+1);
	if (threadid != 0)
	{
		fprintf(out, "thread id %d, %p: memory write, address %p\n", threadid, ip, addr);
		fflush(out);
	}
	PIN_ReleaseLock(&pinLock);
}

//====================================================================
// Instrumentation Routines
//====================================================================

// This routine is executed for each instruction.
VOID Instruction(INS ins, VOID *v){
	UINT32 memOperands = INS_MemoryOperandCount(ins);

	for (UINT32 memOp = 0; memOp < memOperands; memOp++)
	{
		if (INS_MemoryOperandIsRead(ins, memOp))
		{
			INS_InsertPredicatedCall(
				ins, IPOINT_BEFORE, (AFUNPTR)RecordMemRead, 
				IARG_INST_PTR, IARG_MEMORYOP_EA, 
				memOp, IARG_THREAD_ID, IARG_END);
		}
		if (INS_MemoryOperandIsWritten(ins, memOp))
		{
			INS_InsertPredicatedCall(
				ins, IPOINT_BEFORE, (AFUNPTR)RecordMemWrite, 
				IARG_INST_PTR, IARG_MEMORYOP_EA, 
				memOp, IARG_THREAD_ID, IARG_END);
		}
	}
}

// This routine is executed once at the end.
VOID Fini(INT32 code, VOID *v){
	fprintf(out, "#eof\n");
	fclose(out);
}

// =====================================================================
// Print Help Message                                                 
// =====================================================================

INT32 Usage(){
	PIN_ERROR("This Pintool identifies each memory write and memory read instruction\n" + KNOB_BASE::StringKnobSummary() + "\n");
	return -1;
}

// =====================================================================
// Main                                                                 
// =====================================================================

int main(int argc, char *argv[])
{	
	// Initialize the pin lock
    PIN_InitLock(&pinLock);

    // Initialize pin
    if (PIN_Init(argc, argv)){
    	return Usage();
    }
    PIN_InitSymbols();

	out = fopen("memory_access_2.log", "w");

    // Register Analysis routines to be called when a thread begins/ends
	PIN_AddThreadStartFunction(ThreadStart, 0);
	PIN_AddThreadFiniFunction(ThreadFini, 0);

	INS_AddInstrumentFunction(Instruction, 0);

    // Register Fini to be called when the application exits
	PIN_AddFiniFunction(Fini, 0);
	
	PIN_StartProgram();

	

	return 0;
}